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

#ifndef PROFILER_SERVER_METADATAPARSER_H
#define PROFILER_SERVER_METADATAPARSER_H
#include <vector>
#include "DomainObject.h"
#include "JsonUtil.h"
#include "ClusterDef.h"

namespace Dic::Module::Timeline {
class MetaDataParser {
  public:
    static std::vector<ParallelGroupInfo> ParserParallelGroupInfoByFilePath(const std::string &filePath);
    static std::vector<ParallelGroupInfo> ParserParallelGroupInfoByText(const std::string &text);
    static std::optional<DistributedArgs> ParserDistributedArgsByFilePath(const std::string &filePath);

  private:
    const static inline std::string GLOBAL_RANKS = "global_ranks";
    const static inline std::string GLOBAL_NAME = "group_name";
    static std::vector<ParallelGroupInfo> ConvertGroupInfoJsonToObject(const document_t &json);
    static std::optional<DistributedArgs> ConvertDistributedArgsJsonToObject(const document_t &json);
};
}
#endif // PROFILER_SERVER_METADATAPARSER_H
