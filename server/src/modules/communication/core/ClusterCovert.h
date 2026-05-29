/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_CLUSTERCOVERT_H
#define PROFILER_SERVER_CLUSTERCOVERT_H

#include "CommunicationProtocolResponse.h"
#include "ClusterDomainObject.h"

namespace Dic {
namespace Module {
namespace Communication {
class ClusterCovert {
  public:
    static inline Protocol::MatrixData CovertMatrixDoToInfo(const MatrixInfoDo &matrixDo) {
        Protocol::MatrixData matrix;
        matrix.transitSize = matrixDo.transitSize;
        matrix.transitTime = matrixDo.transitTime;
        matrix.transportType = matrixDo.transportType;
        matrix.opName = matrixDo.opName;
        matrix.bandwidth = matrixDo.bandwidth;
        return matrix;
    }

    static inline Protocol::OperatorTimeItem CovertDoToOperatorTime(const OperatorTimeDo &operatorTimeDo) {
        Protocol::OperatorTimeItem operatorTimeItem;
        operatorTimeItem.operatorName = operatorTimeDo.operatorName;
        operatorTimeItem.startTime = operatorTimeDo.startTime;
        operatorTimeItem.elapseTime = operatorTimeDo.elapseTime;
        return operatorTimeItem;
    }

    static inline Protocol::DurationData CovertDoToDuration(const DurationDo &durationDo) {
        Protocol::DurationData duration;
        duration.startTime = durationDo.startTime;
        duration.elapseTime = durationDo.elapseTime;
        duration.transitTime = durationDo.transitTime;
        duration.synchronizationTime = durationDo.synchronizationTime;
        duration.waitTime = durationDo.waitTime;
        duration.idleTime = durationDo.idleTime;
        duration.synchronizationTimeRatio = durationDo.synchronizationTimeRatio;
        duration.waitTimeRatio = durationDo.waitTimeRatio;
        duration.sdmaBw = durationDo.sdmaBw;
        duration.rdmaBw = durationDo.rdmaBw;
        duration.sdmaTime = durationDo.sdmaTime;
        duration.rdmaTime = durationDo.rdmaTime;
        return duration;
    }
};
}
}
}
#endif // PROFILER_SERVER_CLUSTERCOVERT_H
