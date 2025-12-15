/***************************************************************
 * Copyright (C) 2025, Val Doroshchuk <valbok@gmail.com>       *
 *                                                             *
 * This file is part of QtAVPlayer.                            *
 * Free Qt Media Player based on FFmpeg.                       *
 ***************************************************************/

#include "qavrealtimevideodecoder.h"
#include "qavvideocodec_p.h"
#include "qavhwdevice_p.h"
#include "qavpacket.h"

#include <QDebug>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

QT_BEGIN_NAMESPACE

class QAVRealTimeVideoDecoderPrivate
{
public:
    void reset();

    QSharedPointer<QAVVideoCodec> codec = QSharedPointer<QAVVideoCodec>::create();
    QAVStream stream;
    AVFormatContext *ctx = nullptr;
    AVRational timeBase{1, 1000};
};

void QAVRealTimeVideoDecoderPrivate::reset()
{
    stream = QAVStream();
    if (codec)
        codec->flushBuffers();

    if (ctx)
        avformat_free_context(ctx);
    ctx = nullptr;
}

QAVRealTimeVideoDecoder::QAVRealTimeVideoDecoder()
    : d_ptr(new QAVRealTimeVideoDecoderPrivate)
{
}

QAVRealTimeVideoDecoder::~QAVRealTimeVideoDecoder()
{
    close();
}

bool QAVRealTimeVideoDecoder::open(AVCodecID codecId,
                                   const QByteArray &extraData,
                                   const AVRational &timeBase,
                                   const QMap<QString, QString> &options)
{
    Q_D(QAVRealTimeVideoDecoder);
    d->reset();
    d->timeBase = timeBase;

    const AVCodec *codec = avcodec_find_decoder(codecId);
    if (!codec) {
        qWarning() << "No decoder could be found for codec:" << codecId;
        return false;
    }

    d->ctx = avformat_alloc_context();
    if (!d->ctx) {
        qWarning() << "Failed to allocate AVFormatContext";
        return false;
    }

    AVStream *avStream = avformat_new_stream(d->ctx, nullptr);
    if (!avStream) {
        qWarning() << "Failed to allocate AVStream";
        d->reset();
        return false;
    }

    avStream->time_base = timeBase;
    avStream->avg_frame_rate = {0, 1};
    avStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    avStream->codecpar->codec_id = codecId;

    if (!extraData.isEmpty()) {
        avStream->codecpar->extradata_size = extraData.size();
        avStream->codecpar->extradata = static_cast<uint8_t *>(
            av_mallocz(extraData.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!avStream->codecpar->extradata) {
            qWarning() << "Failed to allocate extradata";
            d->reset();
            return false;
        }
        memcpy(avStream->codecpar->extradata, extraData.constData(), extraData.size());
    }

    d->codec->setCodec(codec);

    AVDictionary *opts = nullptr;
    for (auto it = options.begin(); it != options.end(); ++it)
        av_dict_set(&opts, it.key().toUtf8().constData(), it.value().toUtf8().constData(), 0);

    if (!d->codec->open(avStream, &opts)) {
        av_dict_free(&opts);
        qWarning() << "Could not open the codec:" << codec->name;
        d->reset();
        return false;
    }
    av_dict_free(&opts);

    d->stream = QAVStream(avStream->index, d->ctx, d->codec);
    return true;
}

void QAVRealTimeVideoDecoder::close()
{
    Q_D(QAVRealTimeVideoDecoder);
    d->reset();
}

void QAVRealTimeVideoDecoder::setDevice(const QSharedPointer<QAVHWDevice> &device)
{
    Q_D(QAVRealTimeVideoDecoder);
    d->codec->setDevice(device);
}

QAVHWDevice *QAVRealTimeVideoDecoder::device() const
{
    return d_func()->codec->device();
}

int QAVRealTimeVideoDecoder::decode(const QByteArray &data,
                                    QList<QAVFrame> &frames,
                                    int64_t pts,
                                    int64_t dts,
                                    bool keyFrame)
{
    Q_D(QAVRealTimeVideoDecoder);
    if (!d->stream)
        return AVERROR(EINVAL);

    QAVPacket pkt;
    pkt.setStream(d->stream);
    auto avpkt = pkt.packet();

    int ret = 0;
    if (!data.isEmpty()) {
        ret = av_new_packet(avpkt, data.size());
        if (ret < 0)
            return ret;
        memcpy(avpkt->data, data.constData(), data.size());
        avpkt->pts = pts;
        avpkt->dts = dts;
        avpkt->flags = keyFrame ? AV_PKT_FLAG_KEY : 0;
        avpkt->stream_index = d->stream.index();
    }

    int sent = 0;
    do {
        sent = pkt.send();
        if (sent < 0 && sent != AVERROR(EAGAIN) && sent != AVERROR_EOF)
            break;

        while (true) {
            QAVFrame frame;
            frame.setStream(d->stream);
            frame.setTimeBase(d->timeBase);
            int received = frame.receive();
            if (received < 0)
                break;
            frames.push_back(frame);
        }
    } while (sent == AVERROR(EAGAIN));

    av_packet_unref(avpkt);
    return sent;
}

QT_END_NAMESPACE
