#include "qavrealtimevideodecoder_p.h"
#include "qavvideocodec_p.h"
#include "qavpacket.h"
#include "qavframe.h"
#include "qavstream.h"
#include "qavvideoframe.h"
#include "qavhwdevice_p.h"

#if defined(QT_AVPLAYER_VA_X11) && QT_CONFIG(opengl)
#include "qavhwdevice_vaapi_x11_glx_p.h"
#endif
#if defined(QT_AVPLAYER_VDPAU)
#include "qavhwdevice_vdpau_p.h"
#endif
#if defined(QT_AVPLAYER_VA_DRM) && QT_CONFIG(egl)
#include "qavhwdevice_vaapi_drm_egl_p.h"
#endif
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#include "qavhwdevice_videotoolbox_p.h"
#endif
#if defined(Q_OS_WIN)
#include "qavhwdevice_d3d11_p.h"
#endif
#if defined(Q_OS_ANDROID)
#include "qavhwdevice_mediacodec_p.h"
#include <QtCore/private/qjnihelpers_p.h>
extern "C" {
#include "libavcodec/jni.h"
}
#endif

#include <QList>
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

QAVRealTimeVideoDecoder::QAVRealTimeVideoDecoder(QObject *parent)
    : QObject(parent)
{
}

QAVRealTimeVideoDecoder::~QAVRealTimeVideoDecoder()
{
    releaseCodec();
}

bool QAVRealTimeVideoDecoder::init(const QByteArray &extradata)
{
    m_extradata = extradata;
    return openCodec(extradata);
}

bool QAVRealTimeVideoDecoder::openCodec(const QByteArray &extradata)
{
    releaseCodec();

    // 创建假的 AVFormatContext 和一个视频流
    m_formatCtx = avformat_alloc_context();
    if (!m_formatCtx)
        return false;

    m_stream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_stream) {
        avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }

    // 配置流信息
    m_stream->id = 0;
    m_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    m_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    m_stream->time_base = AVRational{1, 90000};      // 90 kHz 时基，用于 pts
    m_stream->avg_frame_rate = AVRational{0, 1};

    // 拷贝 extradata (SPS/PPS)
    if (!extradata.isEmpty()) {
        uint8_t *p = (uint8_t *)av_malloc(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!p) {
            avformat_free_context(m_formatCtx);
            m_formatCtx = nullptr;
            m_stream = nullptr;
            return false;
        }
        memcpy(p, extradata.constData(), extradata.size());
        memset(p + extradata.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
        m_stream->codecpar->extradata = p;
        m_stream->codecpar->extradata_size = extradata.size();
    }

    // 创建 QAVVideoCodec
    QAVVideoCodec *videoCodec = new QAVVideoCodec;
    m_codec.reset(videoCodec);

    // 如果用户指定了 -vcodec 选项，可以使用 m_inputVideoCodec；此处留空则自动选择
    if (!m_inputVideoCodec.isEmpty()) {
        const AVCodec *c = avcodec_find_decoder_by_name(m_inputVideoCodec.toUtf8().constData());
        if (c)
            videoCodec->setCodec(c);
    }

    // 枚举所有硬件设备，复用 qavdemuxer.cpp 中的静态逻辑
    QList<QSharedPointer<QAVHWDevice>> devices;
#if defined(QT_AVPLAYER_VA_X11) && QT_CONFIG(opengl)
    devices.append(QSharedPointer<QAVHWDevice>(new QAVHWDevice_VAAPI_X11_GLX));
#endif
#if defined(QT_AVPLAYER_VDPAU)
    devices.append(QSharedPointer<QAVHWDevice>(new QAVHWDevice_VDPAU));
#endif
#if defined(QT_AVPLAYER_VA_DRM) && QT_CONFIG(egl)
    devices.append(QSharedPointer<QAVHWDevice>(new QAVHWDevice_VAAPI_DRM_EGL));
#endif
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    devices.append(QSharedPointer<QAVHWDevice>(new QAVHWDevice_VideoToolbox));
#endif
#if defined(Q_OS_WIN)
    devices.append(QSharedPointer<QAVHWDevice>(new QAVHWDevice_D3D11));
#endif
#if defined(Q_OS_ANDROID)
    devices.append(QSharedPointer<QAVHWDevice>(new QAVHWDevice_MediaCodec));
#endif

    // 判断是否禁用硬件设备
    const bool ignoreHW = qEnvironmentVariableIsSet("QT_AVPLAYER_NO_HWDEVICE");

    // Android 特殊：需设置 Java VM
#if defined(Q_OS_ANDROID)
    if (!ignoreHW && !videoCodec->codec()) {
        videoCodec->setCodec(avcodec_find_decoder_by_name("h264_mediacodec"));
    }
    void *vm = QtAndroidPrivate::javaVM();
    if (vm)
        av_jni_set_java_vm(vm, NULL);
#endif

    // 准备 codec options 字典（目前为空）
    AVDictionary *dict = nullptr;

    // 尝试创建硬件设备并绑定到 CodecContext
    if (!ignoreHW) {
        AVBufferRef *hw_device_ctx = nullptr;
        for (const auto &dev : devices) {
            auto devName = av_hwdevice_get_type_name(dev->type());
            qDebug() << "Creating hardware device context:" << devName;
            if (av_hwdevice_ctx_create(&hw_device_ctx, dev->type(), nullptr, dict, 0) >= 0) {
                qDebug() << "Using hardware device context:" << devName;
                videoCodec->avctx()->hw_device_ctx = hw_device_ctx;
                videoCodec->avctx()->pix_fmt = dev->format();
                videoCodec->setDevice(dev);
                break;
            }
            av_buffer_unref(&hw_device_ctx);
        }
    }

    // 打开解码器（必须在设置硬件上下文之后调用）
    if (!videoCodec->open(m_stream, &dict)) {
        qWarning() << "Could not open video codec for stream";
        if (dict)
            av_dict_free(&dict);
        avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
        m_stream = nullptr;
        m_codec.clear();
        return false;
    }
    if (dict)
        av_dict_free(&dict);

    // 构造 QAVStream 用于 send/receive 接口
    m_qavStream = QAVStream(0, m_formatCtx, m_codec);
    m_ready = true;
    return true;
}

void QAVRealTimeVideoDecoder::releaseCodec()
{
    m_ready = false;
    m_qavStream = QAVStream();
    m_codec.clear();
    // 释放 extradata
    if (m_stream && m_stream->codecpar) {
        if (m_stream->codecpar->extradata) {
            av_freep(&m_stream->codecpar->extradata);
            m_stream->codecpar->extradata_size = 0;
        }
    }
    if (m_formatCtx)
        avformat_free_context(m_formatCtx);
    m_formatCtx = nullptr;
    m_stream = nullptr;
}

bool QAVRealTimeVideoDecoder::pushEncodedPacket(const QByteArray &data, bool keyFrame, qint64 pts)
{
    if (!m_ready)
        return false;

    // 创建 QAVPacket 并附加到当前流
    QAVPacket pkt;
    pkt.setStream(m_qavStream);
    AVPacket *avpkt = pkt.packet();
    av_packet_unref(avpkt);

    // 分配新包并拷贝数据
    if (av_new_packet(avpkt, data.size()) < 0)
        return false;
    memcpy(avpkt->data, data.constData(), data.size());
    avpkt->pts = pts;
    avpkt->dts = pts;
    avpkt->flags = keyFrame ? AV_PKT_FLAG_KEY : 0;
    avpkt->stream_index = 0;

    // 发送并接收可能输出的多帧
    int sendRet;
    do {
        sendRet = pkt.send();
        if (sendRet < 0 && sendRet != AVERROR(EAGAIN))
            return false;
        while (true) {
            QAVFrame frame;
            frame.setStream(m_qavStream);
            int ret = frame.receive();
            if (ret < 0)
                break;
            // 使用拷贝构造函数将 QAVFrame 转为 QAVVideoFrame
            QAVVideoFrame vframe(frame);
            if (vframe)
                emit frameDecoded(vframe);
        }
    } while (sendRet == AVERROR(EAGAIN));

    return true;
}
