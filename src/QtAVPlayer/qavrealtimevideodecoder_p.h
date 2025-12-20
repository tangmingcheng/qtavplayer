// SPDX-License-Identifier: LGPL-3.0-or-later
//
// qavrealtimevideodecoder_p.h
//
// 实时访问单元解码器声明，复用 qavdemuxer 中的硬件设备逻辑。
// 通过构造一个假的 AVFormatContext/AVStream 和 QAVVideoCodec，
// 以便在没有 Demuxer 的情况下仍然能够使用硬件加速解码。

#ifndef QAVREALTIMEVIDEODECODER_P_H
#define QAVREALTIMEVIDEODECODER_P_H

#include <QObject>
#include <QByteArray>
#include <QSharedPointer>
#include <QString>
#include "qavstream.h"

extern "C" {
struct AVFormatContext;
struct AVStream;
}

class QAVVideoFrame;
class QAVCodec;

/*!
 * \internal
 * \brief 实时访问单元解码器
 *
 * 该解码器用于实时视频场景：调用 init() 创建内部解码器并选择可用的
 * 硬件设备，然后持续调用 pushEncodedPacket() 投喂完整的 H.264 访问单元。
 * 解码出的每个视频帧通过 frameDecoded() 信号送出。
 */
class QAVRealTimeVideoDecoder : public QObject
{
    Q_OBJECT
public:
    explicit QAVRealTimeVideoDecoder(QObject *parent = nullptr);
    ~QAVRealTimeVideoDecoder() override;

    /*!
     * 初始化解码器。extradata 应当包含 SPS/PPS 等参数集（Annex B 格式）。
     */
    bool init(const QByteArray &extradata);

    /*!
     * 推送一个完整访问单元。keyFrame 表示是否为关键帧 (IDR)，pts 单位为 90kHz。
     */
    bool pushEncodedPacket(const QByteArray &data, bool keyFrame = false, qint64 pts = 0);

signals:
    //! 解码出新的视频帧时发出。
    void frameDecoded(const QAVVideoFrame &frame);

private:
    bool openCodec(const QByteArray &extradata);
    void releaseCodec();

    AVFormatContext *m_formatCtx = nullptr;
    AVStream *m_stream = nullptr;
    QSharedPointer<QAVCodec> m_codec;
    QAVStream m_qavStream;
    QByteArray m_extradata;
    QString m_inputVideoCodec;   // 可选：用户指定解码器名称
    bool m_ready = false;
};

#endif // QAVREALTIMEVIDEODECODER_P_H
