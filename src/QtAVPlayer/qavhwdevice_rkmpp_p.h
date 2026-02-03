/*********************************************************
 * Copyright (C) 2020, Val Doroshchuk <valbok@gmail.com> *
 *                                                       *
 * This file is part of QtAVPlayer.                      *
 * Free Qt Media Player based on FFmpeg.                 *
 *********************************************************/

#ifndef QAVHWDEVICE_RKMPP_P_H
#define QAVHWDEVICE_RKMPP_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qavhwdevice_p.h"
#include "qavvideobuffer_gpu_p.h"

QT_BEGIN_NAMESPACE

class QAVHWDevice_RKMPP : public QAVHWDevice
{
public:
    QAVHWDevice_RKMPP() = default;
    ~QAVHWDevice_RKMPP() = default;

    AVPixelFormat format() const override;
    AVHWDeviceType type() const override;
    QAVVideoBuffer *videoBuffer(const QAVVideoFrame &frame) const override;
};

QT_END_NAMESPACE

#endif // QAVHWDEVICE_RKMPP_P_H

