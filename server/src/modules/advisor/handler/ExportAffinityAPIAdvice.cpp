/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "pch.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"
#include "AffinityAPIAdvisor.h"
#include "WsSessionManager.h"
#include "TraceTime.h"
#include "NumberUtil.h"
#include "FileUtil.h"
#include "ProjectExplorerManager.h"
#include "StringUtil.h"
#include "ExportAffinityAPIAdvice.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;

const std::vector<std::string> AFFINITY_API_CSV_HEADERS = {
    "Name", "Origin API", "Replacement API", "Start Time", "Duration(ns)", "Process Id", "Thread Id", "Notes"};

bool ExportAffinityAPIAdvice::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    std::unique_ptr<ExportAffinityAPIResponse> responsePtr = std::make_unique<ExportAffinityAPIResponse>();
    ExportAffinityAPIResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    SetResponseResult(response, true);

    std::string error;
    if (!request.Check(error)) {
        ServerLog::Error(error);
        SetAdvisorError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    std::vector<Protocol::AffinityAPIData> allData;
    Protocol::APITypeParams exportParams;
    exportParams.rankId = request.rankId;
    exportParams.orderBy = request.orderBy;
    exportParams.orderType = request.orderType;
    exportParams.startTime = request.startTime;
    exportParams.endTime = request.endTime;
    if (!AffinityAPIAdvisor::ProcessAll(exportParams, allData)) {
        ServerLog::Error("Failed to Export Affinity API for rank ", request.rankId);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    response.body.size = allData.size();

    if (!CreateCsvFile(request.rankId, request.projectName, response.body)) {
        ServerLog::Error("[Advisor] Failed to create CSV file for affinity API export.");
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    std::string csvContent = GenerateCsv(allData);
    if (!AppendFileContent(csvContent)) {
        response.body.exceedingFileLimit = true;
    }
    DestroyFile();

    SendResponse(std::move(responsePtr), true);
    return true;
}

std::string ExportAffinityAPIAdvice::CheckColumn(const std::string &column) {
    if (NumberUtil::IsDouble(column)) {
        return column;
    }
    std::regex re(R"(^[\=\+\-\@].*)");
    if (std::regex_match(column, re)) {
        return "\\" + column;
    }
    return column;
}

std::string ExportAffinityAPIAdvice::GenerateCsv(const std::vector<Protocol::AffinityAPIData> &data) {
    std::ostringstream csv;
    csv << "\xEF\xBB\xBF";
    for (size_t i = 0; i < AFFINITY_API_CSV_HEADERS.size(); ++i) {
        if (i > 0) {
            csv << ",";
        }
        csv << AFFINITY_API_CSV_HEADERS[i];
    }
    csv << "\n";
    for (const auto &item : data) {
        csv << CheckColumn(item.name) << ",";
        csv << CheckColumn(item.originAPI) << ",";
        csv << CheckColumn(item.replaceAPI) << ",";
        csv << CheckColumn(std::to_string(item.baseInfo.startTime)) << ",";
        csv << CheckColumn(std::to_string(item.baseInfo.duration)) << ",";
        csv << CheckColumn(item.baseInfo.pid) << ",";
        csv << CheckColumn(item.baseInfo.tid) << ",";
        csv << CheckColumn(item.note) << "\n";
    }
    return csv.str();
}

bool ExportAffinityAPIAdvice::CreateCsvFile(
    const std::string &rankId, const std::string &projectName, Protocol::ExportAffinityAPIResBody &resBody) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    std::vector<Global::ProjectExplorerInfo> projectInfo =
        Global::ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, {});
    if (projectInfo.empty()) {
        ServerLog::Error("[Advisor] Project information is missing when exporting affinity API.");
        return false;
    }
#ifdef _WIN32
    const std::string_view SEP = "\\";
#else
    const std::string_view SEP = "/";
#endif
    std::string filePath = projectInfo[0].fileName + std::string(SEP) + "advisor_affinity_api_" + rankId + "_" +
        std::to_string(timestamp) + ".csv";
    if (FileUtil::CheckPathInvalidChar(filePath)) {
        ServerLog::Error("[Advisor] File path is invalid in export affinity API.");
        return false;
    }
    ofs.open(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        ServerLog::Error("[Advisor] Failed to open file for export affinity API: ", filePath);
        return false;
    }
    currentFileSize = 0;
    resBody.filePath = filePath;
#if defined(__linux__) || defined(__APPLE__)
    ofs.close();
    mode_t mode = 0640;
    FileUtil::ModifyFilePermissions(filePath, mode);
    ofs.open(filePath, std::ofstream::out | std::ofstream::app);
#endif
    return true;
}

bool ExportAffinityAPIAdvice::AppendFileContent(const std::string &str) {
    if (ofs.is_open()) {
        ofs << str;
        ofs.flush();
        currentFileSize += str.length();
    }
    if (currentFileSize > maxFileSize) {
        ServerLog::Warn("[Advisor] The file exceeds 1GB in export affinity API");
        return false;
    }
    return true;
}

void ExportAffinityAPIAdvice::DestroyFile() {
    if (ofs.is_open()) {
        ofs.close();
    }
}
} // Dic::Module::Advisor
