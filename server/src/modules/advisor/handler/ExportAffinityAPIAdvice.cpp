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
#include <cerrno>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace Dic::Module::Advisor {
using namespace Dic::Server;

namespace {
const std::vector<std::string> AFFINITY_API_CSV_HEADERS = {
    "Name", "Origin API", "Replacement API", "Start Time", "Duration(ns)", "Process Id", "Thread Id", "Notes"};
constexpr size_t MAX_EXPORT_FILE_ATTEMPTS = 1000;
} // namespace

std::string ExportAffinityAPIAdvice::EscapeCsvField(const std::string &value) {
    // 仅当字段包含分隔符、引号或换行时才加引号，其余字段保持原格式，避免改变已有导出内容。
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') {
            escaped += "\"\"";
        } else {
            escaped += character;
        }
    }
    escaped += '"';
    return escaped;
}

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
    if (allData.empty()) {
        // 查询成功但无匹配数据：保留count=0及空路径，不创建只有表头的文件。
        SendResponse(std::move(responsePtr), true);
        return true;
    }

    if (!CreateCsvFile(request.rankId, request.projectName, response.body)) {
        ServerLog::Error("[Advisor] Failed to create CSV file for affinity API export.");
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    bool withinLimit = AppendFileContent("\xEF\xBB\xBF" + StringUtil::join(AFFINITY_API_CSV_HEADERS, ",") + "\n");
    for (const auto &item : allData) {
        if (!withinLimit) {
            break;
        }
        // 与算子导出一致：写完一整行后判断1GB阈值，超限即停止，不再生成剩余行。
        withinLimit = AppendFileContent(GenerateCsvRow(item));
    }
    const bool writeSucceeded = ofs.good();
    response.body.exceedingFileLimit = !withinLimit && writeSucceeded;
    if (!DestroyFile() || !writeSucceeded) {
        ServerLog::Error("[Advisor] Failed to write CSV content for affinity API export.");
        response.body.filePath.clear();
        SendResponse(std::move(responsePtr), false);
        return false;
    }

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

std::string ExportAffinityAPIAdvice::GenerateCsvRow(const Protocol::AffinityAPIData &item) {
    std::ostringstream csv;
    // Origin API 可能由多个 API 以逗号拼接，必须转义后再写入，否则后续列会整体错位。
    csv << EscapeCsvField(CheckColumn(item.name)) << ",";
    csv << EscapeCsvField(CheckColumn(item.originAPI)) << ",";
    csv << EscapeCsvField(CheckColumn(item.replaceAPI)) << ",";
    csv << EscapeCsvField(CheckColumn(std::to_string(item.baseInfo.startTime))) << ",";
    csv << EscapeCsvField(CheckColumn(std::to_string(item.baseInfo.duration))) << ",";
    csv << EscapeCsvField(CheckColumn(item.baseInfo.pid)) << ",";
    csv << EscapeCsvField(CheckColumn(item.baseInfo.tid)) << ",";
    csv << EscapeCsvField(CheckColumn(item.note)) << "\n";
    return csv.str();
}

std::string ExportAffinityAPIAdvice::SanitizeFileName(const std::string &value) {
    // trace_view.json等文本工程的rankId是完整文件路径，需替换文件名及路径校验禁止的字符，
    // 正常卡号保持不变，避免改变已有导出文件命名。
    std::string sanitized;
    sanitized.reserve(value.size());
    constexpr std::string_view invalidCharacters = "<>:\"/\\|?*%&$;'`";
    for (const unsigned char character : value) {
        const bool control = character < 0x20 || character == 0x7F;
        const bool invalid = invalidCharacters.find(static_cast<char>(character)) != std::string_view::npos;
        sanitized += (control || invalid) ? '_' : static_cast<char>(character);
    }
    return sanitized;
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
    const std::string fileStem = projectInfo[0].fileName + std::string(SEP) + "advisor_affinity_api_" +
        SanitizeFileName(rankId) + "_" + std::to_string(timestamp);
    return OpenUniqueCsvFile(fileStem, resBody);
}

bool ExportAffinityAPIAdvice::OpenUniqueCsvFile(
    const std::string &fileStem, Protocol::ExportAffinityAPIResBody &resBody) {
    for (size_t attempt = 0; attempt < MAX_EXPORT_FILE_ATTEMPTS; ++attempt) {
        const std::string filePath = fileStem + (attempt == 0 ? "" : "_" + std::to_string(attempt)) + ".csv";
        if (!FileUtil::CheckFilePathLength(filePath) || FileUtil::CheckPathInvalidChar(filePath)) {
            ServerLog::Error("[Advisor] File path is invalid in export affinity API.");
            return false;
        }
        // 先由系统原子创建并占用名称，不能用exists检查后再trunc打开。
#ifdef _WIN32
        const std::wstring nativePath = FileUtil::ConvertToLongPathW(filePath);
        if (nativePath.empty()) {
            return false;
        }
        const HANDLE reservation = CreateFileW(
            nativePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (reservation == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
                continue;
            }
            ServerLog::Error("[Advisor] Failed to create unique affinity API export file: ", filePath);
            return false;
        }
        if (!CloseHandle(reservation)) {
            return false;
        }
#else
        const int reservation = open(filePath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0640);
        if (reservation < 0) {
            if (errno == EEXIST) {
                continue;
            }
            ServerLog::Error("[Advisor] Failed to create unique affinity API export file: ", filePath);
            return false;
        }
        if (close(reservation) != 0) {
            return false;
        }
        FileUtil::ModifyFilePermissions(filePath, 0640);
#endif
        // 排他创建已占用名称；后续只追加打开，绝不截断已有文件。
        ofs = FileUtil::OpenWrite(filePath, std::ios::out | std::ios::app);
        if (!ofs.is_open()) {
            ServerLog::Error("[Advisor] Failed to open reserved affinity API export file: ", filePath);
            return false;
        }
        currentFileSize = 0;
        resBody.filePath = filePath;
        return true;
    }
    ServerLog::Error("[Advisor] Too many files with the same affinity API export name.");
    return false;
}

bool ExportAffinityAPIAdvice::AppendFileContent(const std::string &str) {
    if (!ofs.is_open()) {
        return false;
    }
    ofs << str;
    if (!ofs.good()) {
        return false;
    }
    currentFileSize += str.length();
    if (currentFileSize > maxFileSize) {
        ServerLog::Warn("[Advisor] The file exceeds 1GB in export affinity API");
        return false;
    }
    return true;
}

bool ExportAffinityAPIAdvice::DestroyFile() {
    if (!ofs.is_open()) {
        return false;
    }
    // 逐行写入使用流缓冲，最终统一刷新，避免每行一次磁盘flush。
    ofs.flush();
    const bool written = ofs.good();
    ofs.close();
    return written && !ofs.fail();
}
} // Dic::Module::Advisor
