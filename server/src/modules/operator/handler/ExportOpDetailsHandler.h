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

#ifndef PROFILER_SERVER_EXPORTOPDETAILSHANDLER_H
#define PROFILER_SERVER_EXPORTOPDETAILSHANDLER_H

#include <fstream>
#include "ModuleRequestHandler.h"
#include "OperatorRequestHandler.h"
#include "OperatorProtocolRequest.h"

namespace Dic::Module::Operator {
class ExportOpDetailsHandler : public OperatorRequestHandler {
  public:
    ExportOpDetailsHandler() { command = Protocol::REQ_RES_OPERATOR_EXPORT_DETAILS; }

    ~ExportOpDetailsHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    bool HandleCompareDataRequest(OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &responsePtr);
    bool HandleStatisticCompareDataRequest(
        OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &response);
    bool HandleNotStatisticCompareDataRequest(
        OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &response);
    bool HandleDataRequest(OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &responsePtr);
    bool HandleStatisticDataRequest(OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &responsePtr);
    bool HandleNotStatisticDataRequest(
        OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &responsePtr);
    bool CreateCsvFile(OperatorExportDetailsRequest &request, OperatorExportDetailsResponse &response);
    bool AppendFileContent(const std::string &str); // 写入文件的内容需校验再传入
    void DestroyFile();
    std::ofstream ofs;
    size_t maxFileSize = 1024 * 1024 * 1024; // 1GB
    size_t currentFileSize = 0;
};
}

#endif // PROFILER_SERVER_EXPORTOPDETAILSHANDLER_H
