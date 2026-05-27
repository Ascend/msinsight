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

#ifndef PROFILER_SERVER_SOURCE_DEFS_H
#define PROFILER_SERVER_SOURCE_DEFS_H
#include <string>
namespace Dic {
namespace Module {
namespace Source {
struct ColumnDataType {
    enum Type : int {
        SKIP = 0,
        INT,
        FLOAT,
        STRING,
        PERCENTAGE,
        JSON_STR,
        PERCENTAGEANDDETAIL = 100,
    };
};

enum class GRPStatus { SPACE = 0, READ = 1, WRITE = 2, READ_WRITE = 3, IN_USE = 4 };

inline GRPStatus GetGRPStatus(int status) { return static_cast<GRPStatus>(status); }

inline int GetGRPStatus2Int(GRPStatus status) { return static_cast<int>(status); }

enum class GRPProgress { BEGIN = 0, END = 1, IN_USE = 2 };

struct GRPInfo {
    std::string regName;
    int lifeTime;
    GRPStatus status{GRPStatus::SPACE};
    GRPProgress progress;
    int index; // 当前寄存器已经经历的指令周期，用于内部排序，不对外返回
};

struct PercentageAndDetails {
    float percentage;
    std::vector<std::pair<std::string, int>> details;
};
}
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_SOURCE_DEFS_H
