/***************************************************************
 * Copyright (C) 2025, Val Doroshchuk <valbok@gmail.com>       *
 *                                                             *
 * This file is part of QtAVPlayer.                            *
 * Free Qt Media Player based on FFmpeg.                       *
 ***************************************************************/

#ifndef QAVREALTIMEVIDEODECODER_H
#define QAVREALTIMEVIDEODECODER_H

#include <QtAVPlayer/qtavplayerglobal.h>
#include <QtAVPlayer/qavframe.h>
#include <QtAVPlayer/qavstream.h>
#include <libavcodec/avcodec.h>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <memory>

QT_BEGIN_NAMESPACE

struct AVRational;
class QAVHWDevice;
class QAVRealTimeVideoDecoderPrivate;

class Q_AVPLAYER_EXPORT QAVRealTimeVideoDecoder
{
public:
    QAVRealTimeVideoDecoder();
    ~QAVRealTimeVideoDecoder();

    bool open(AVCodecID codecId,
              const QByteArray &extraData = {},
              const AVRational &timeBase = {1, 1000},
              const QMap<QString, QString> &options = {});
    void close();

    void setDevice(const QSharedPointer<QAVHWDevice> &device);
    QAVHWDevice *device() const;

    int decode(const QByteArray &data,
               QList<QAVFrame> &frames,
               int64_t pts = AV_NOPTS_VALUE,
               int64_t dts = AV_NOPTS_VALUE,
               bool keyFrame = false);

private:
    std::unique_ptr<QAVRealTimeVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QAVRealTimeVideoDecoder)
    Q_DISABLE_COPY(QAVRealTimeVideoDecoder)
};

QT_END_NAMESPACE

#endif
