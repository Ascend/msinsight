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
#ifndef PROFILER_SERVER_SLICEQUERY_H
#define PROFILER_SERVER_SLICEQUERY_H
#include <string>
#include <limits>
#include "DomainObject.h"

namespace Dic::Module::Timeline {
struct SliceQuery {
    uint64_t trackId = 0;
    /* *
     * 界面选取的开始时间
     */
    uint64_t startTime = 0;
    /* *
     * 界面选取的结束时间
     */
    uint64_t endTime = 0;
    /* *
     * 泳道在数据库中的最小时间
     */
    uint64_t minTimestamp = 0;
    PROCESS_TYPE metaType = PROCESS_TYPE::NONE;
    std::string cat;
    /*
     * timeline框选时使用
    */
    std::string rankId;
    std::string pid;
    std::string tid;
    std::string sliceId;
    /**
     * 算子名字
     */
    std::string name;
    /**
     *
     */
    uint64_t timePoint = 0;

    uint64_t depth{std::numeric_limits<uint64_t>::max()};
    std::string startDepth;
    std::string endDepth;
    bool isFilterPythonFunction = false;
    bool isPythonStack = false;
    bool QueryThreadsCheck(std::string &error) const {
        if (startTime > endTime) {
            error = "start time is bigger than end time";
            return false;
        }
        if (trackId == 0) {
            error = "track id is not correct";
            return false;
        }
        return true;
    }
};

struct SliceQueryByNameList {
    std::string rankId;
    std::string processName;
    std::vector<std::string> nameList;
    PROCESS_TYPE metaType = PROCESS_TYPE::NONE;
    uint64_t startTime = UINT64_MAX;
    uint64_t endTime = 0;

    // 以下字段目前只有text场景有效，如果其他场景也需要，请注意代码适配
    std::vector<std::string> processNameExclusion;
    std::string processLabel;
};

struct FlowQuery {
    uint64_t trackId = 0;
    /* *
     * 连线的id
     */
    std::string flowId;
    /* *
     * 界面选取的开始时间
     */
    uint64_t startTime = 0;
    /* *
     * 界面选取的结束时间
     */
    uint64_t endTime = 0;
    /* *
     * 泳道在数据库中的最小时间
     */
    uint64_t minTimestamp = 0;
    /**
     * 连线类别
     */
    std::string cat;
    std::string fileId;
    PROCESS_TYPE metaType = PROCESS_TYPE::NONE;
    /**
     * flowId集合
     */
    std::vector<std::string> flowIds;
};

struct ThreadQuery {
    std::string fileId;
    PROCESS_TYPE metaType = PROCESS_TYPE::NONE;
};

struct TrackQuery {
    /* *
     * 数据库db文件路径唯一标识
     */
    std::string rankId;
    /* *
     * 进程信息
     */
    std::string processId;
    /* *
     * 线程信息
     */
    std::string threadId;
    /* *
     * 线程泳道唯一标识
     */
    uint64_t trackId = 0;

    /**
     * 线程泳道属于哪个类型
     */
    std::string metaType;
    std::string displayMetaType;
    bool isPythonStack = false;

    /**
     * 泳道开始时间
     */
    uint64_t startTime = 0;

    /**
     * 泳道结束时间
     */
    uint64_t endTime = 0;

    std::string fileId;
};

struct SliceBaseInfo {
    std::string rankId;
    std::string pid;
    std::string tid;
    std::string metaType;
    uint64_t startTime = 0;
    uint64_t duration = 0;
};
}
#endif // PROFILER_SERVER_SLICEQUERY_H
