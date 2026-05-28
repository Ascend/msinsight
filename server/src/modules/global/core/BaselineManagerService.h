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

#ifndef PROFILER_SERVER_BASELINEMANAGERSERVCE_H
#define PROFILER_SERVER_BASELINEMANAGERSERVCE_H
#include <string>
#include "GlobalDefs.h"
#include "GlobalProtocolRequest.h"
#include "SystemMemoryDatabaseDef.h"
namespace Dic {
namespace Module {
namespace Global {
class BaselineManagerService {
  public:
    static void ResetBaseline(bool force);
    static bool InitBaselineData(const Protocol::BaselineSettingRequest &request, BaselineInfo &baselineInfo);

  private:
    static bool IsClusterBaseline(ProjectTypeEnum projectTypeEnum,
        const std::vector<ProjectExplorerInfo> &projectInfoList, const std::string &filePath);
    static void InitBaselineParallelStrategy(const std::string &compareClusterPath);
    static bool CheckIsSupportCompare(const std::vector<ProjectExplorerInfo> &baseline,
        const std::vector<ProjectExplorerInfo> &cur, std::string &errorMsg, const std::string &filePath);
};
}
}
}
#endif // PROFILER_SERVER_BASELINEMANAGERSERVCE_H
