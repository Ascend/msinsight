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

#ifndef PROFILER_SERVER_IMPORT_ACTION_HANDLER_H
#define PROFILER_SERVER_IMPORT_ACTION_HANDLER_H

#include <set>
#include <regex>
#include <vector>
#include "TimelineRequestHandler.h"
#include "ProjectParserFactory.h"
#include "FileParser.h"
#include "GlobalDefs.h"

namespace Dic::Module::Timeline {
class ImportActionHandler : public TimelineRequestHandler {
  public:
    ImportActionHandler() {
        command = Protocol::REQ_RES_IMPORT_ACTION;
        async = false;
    };

    ~ImportActionHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    static void SendParseFailEvent(const std::string &message);

    static void LogIfFileNotExist(const Global::ProjectExplorerInfo &projectExplorerInfo);

    static bool TransferProject(ImportActionRequest &request);

    static bool ImportFile(ImportActionRequest &request, std::string &warnMsg);

    static std::vector<ParserType> GetParserTypeList(const std::string &importPath);

    /**
     * @brief 处理单个数据类型的导入事件
     */
    static std::optional<ProjectExplorerInfo> BuildProjectInfo(
        ParserType allocType, ImportActionRequest &request, std::string &warnMsg);

    static bool IsNeedReset(const ImportActionRequest &request);
};
} // end of namespace Dic::Module::Global

#endif // PROFILER_SERVER_IMPORT_ACTION_HANDLER_H
