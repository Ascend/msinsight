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

#ifndef PROFILER_SERVER_EXPERTHOTSPOTPARSER_H
#define PROFILER_SERVER_EXPERTHOTSPOTPARSER_H
#include <vector>
#include <string>
#include "ClusterDef.h"
#include "VirtualClusterDatabase.h"

namespace Dic::Module::Summary {
class ExpertHotspotParser {
  public:
    explicit ExpertHotspotParser(std::shared_ptr<VirtualClusterDatabase> &database, const ModelGenConfig &config)
        : config(config), db(database) {}
    bool Parse(const std::string &filePath, const std::string &version);
    std::map<std::string, ModelInfo> GetModelInfoMap();

  private:
    const static int regexMatchNumber = 3;
    ModelGenConfig config;
    std::shared_ptr<VirtualClusterDatabase> db = nullptr;
    std::map<std::string, ModelInfo> modelInfoMap;
};
}

#endif // PROFILER_SERVER_EXPERTHOTSPOTPARSER_H
