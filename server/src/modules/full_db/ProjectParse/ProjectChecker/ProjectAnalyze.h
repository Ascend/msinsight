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

#ifndef PROFILER_SERVER_PROJECTCHECKER_H
#define PROFILER_SERVER_PROJECTCHECKER_H

#include "GlobalDefs.h"
#include "ProjectExplorerManager.h"
#include "ProjectParserFactory.h"
#include <functional>
#include <string>

namespace Dic::Module {
using namespace Dic;
using namespace Dic::Module::Global;
/**
 * @brief 用于构建工程的目录结构和判断文件的解析类型
 */
class ProjectAnalyze {
  public:
    using FileTypeCheckFunc = std::function<bool(const std::string &)>;
    using ProjectBuildFunc =
        std::function<void(ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parseFile)>;

    static ProjectAnalyze &Instance() {
        static ProjectAnalyze instance;
        return instance;
    }

    void Register(ParserType type, ProjectBuildFunc g) { projectInfoBuildFuncMap.emplace(type, g); }

    void ProjectExportInfoBuild(
        ParserType type, const std::vector<std::string> &parseFile, ProjectExplorerInfo &projectInfo) {
        auto it = projectInfoBuildFuncMap.find(type);
        if (it != projectInfoBuildFuncMap.end()) {
            it->second(projectInfo, parseFile);
        }
    }

  private:
    ProjectAnalyze() = default;

    std::unordered_map<ParserType, ProjectBuildFunc> projectInfoBuildFuncMap{};
};

template <class T> struct ProjectAnalyzeRegister {
    explicit ProjectAnalyzeRegister(ParserType type) {
        static_assert(std::is_base_of_v<ProjectParserBase, T>, "Please register a drived class of ProjectParserBase");
        ProjectAnalyze::Instance().Register(type, T::BuildProjectExploreInfo);
    }
};
} // Module

#endif // PROFILER_SERVER_PROJECTCHECKER_H
