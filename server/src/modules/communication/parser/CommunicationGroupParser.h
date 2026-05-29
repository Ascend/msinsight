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

#ifndef PROFILER_SERVER_COMMUNICATIONGROUPPARSER_H
#define PROFILER_SERVER_COMMUNICATIONGROUPPARSER_H
#include <vector>
#include "ClusterDef.h"
#include "JsonUtil.h"

namespace Dic {
namespace Module {
namespace Communication {
class CommunicationGroupParser {
  public:
    static std::vector<CommGroupParallelInfo> ParseCommunicationGroup(const std::string &selectedPath);
    static std::vector<CommGroupParallelInfo> ParseCommunicationGroupByText(const std::string &fileContent);

  private:
    static std::vector<CommGroupParallelInfo> GetGroupFromParallelInfo(const json_t &json);
    static std::vector<CommGroupParallelInfo> GetGroupFromP2pAndCollective(const json_t &json);
};
}
}
}
#endif // PROFILER_SERVER_COMMUNICATIONGROUPPARSER_H
