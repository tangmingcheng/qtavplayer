/*********************************************************
 * Copyright (C) 2020, Val Doroshchuk <valbok@gmail.com> *
 *                                                       *
 * This file is part of QtAVPlayer.                      *
 * Free Qt Media Player based on FFmpeg.                 *
 *********************************************************/

#include "qavhwdevice_rkmpp_p.h"
#include <QDebug>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/hwcontext.h>
}

QT_BEGIN_NAMESPACE

AVPixelFormat QAVHWDevice_RKMPP::format() const
{
    return AV_PIX_FMT_DRM_PRIME;
}

AVHWDeviceType QAVHWDevice_RKMPP::type() const
{
    return AV_HWDEVICE_TYPE_RKMPP;
}

QAVVideoBuffer *QAVHWDevice_RKMPP::videoBuffer(const QAVVideoFrame &frame) const
{
    // Use generic GPU buffer which performs av_hwframe_transfer_data when needed
    return new QAVVideoBuffer_GPU(frame);
}

QT_END_NAMESPACE

