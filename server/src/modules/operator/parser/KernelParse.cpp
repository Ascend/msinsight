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
#include "pch.h"
#include "DataBaseManager.h"
#include "TraceFileParser.h"
#include "ParserStatusManager.h"
#include "CommonDefs.h"
#include "WsSessionManager.h"
#include "OperatorProtocolEvent.h"
#include "RegexUtil.h"
#include "TraceTime.h"
#include "BaselineManager.h"
#include "TrackInfoManager.h"
#include "ParserStatusManager.h"
#include "KernelParse.h"

namespace Dic::Module::Summary {
using namespace Dic::Server;
bool KernelParse::blockDimReplacedByBlockNum = false;

std::map<std::vector<std::string>,
    std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
    KernelParse::parseFuncMap;

KernelParse &KernelParse::Instance() {
    static KernelParse instance;
    return instance;
}

KernelParse::KernelParse() {
    threadPool = std::make_unique<ThreadPool>(KernelParse::maxThreadNum);
    InitKernelParseMap();
}

KernelParse::~KernelParse() {
    if (threadPool != nullptr) {
        threadPool->ShutDown();
    }
}

void KernelParse::InitKernelParseMap() {
    parseFuncMap.emplace(std::vector<std::string>{FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE, FIELD_DURATION,
                             FIELD_WAIT_TIME, FIELD_BLOCK_DIM},
        std::bind(&KernelParse::ParsePyTorchOpBaseInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE, FIELD_DURATION,
                             FIELD_WAIT_TIME, FIELD_BLOCK_NUM},
        std::bind(&KernelParse::ParsePyTorchOpBaseInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_OP_NAME, FIELD_OP_TYPE, FIELD_TASK_TYPE, FIELD_TASK_DURATION,
                             FIELD_TASK_WAIT_TIME, FIELD_BLOCK_DIM},
        std::bind(&KernelParse::ParseMsProfOpBaseInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_OP_NAME, FIELD_OP_TYPE, FIELD_TASK_TYPE, FIELD_TASK_DURATION,
                             FIELD_TASK_WAIT_TIME, FIELD_BLOCK_NUM},
        std::bind(&KernelParse::ParseMsProfOpBaseInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{STEP_ID},
        std::bind(&KernelParse::ParsePyTorchStepInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_TASK_ID},
        std::bind(&KernelParse::ParseTaskIdInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_OP_STATE},
        std::bind(&KernelParse::ParseOpStateInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_INPUT_DATA_TYPES, FIELD_INPUT_SHAPES, FIELD_INPUT_FORMATS,
                             FIELD_OUTPUT_DATA_TYPES, FIELD_OUTPUT_SHAPES, FIELD_OUTPUT_FORMATS},
        std::bind(&KernelParse::ParseShapeInfoData, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
            std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_START_TIME},
        std::bind(&KernelParse::ParseStartTimeInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_TASK_START_TIME},
        std::bind(&KernelParse::ParseTaskStartTimeInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
    parseFuncMap.emplace(std::vector<std::string>{FIELD_AICORE_TIME},
        std::bind(&KernelParse::ParseAICoreMetricsInfoData, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
}

std::vector<std::string> KernelParse::GetKernelFiles(const std::vector<std::string> &paths) {
    std::vector<std::string> fileList = {};
    for (const std::string &path : paths) {
        auto files = FileUtil::FindFilesWithFilter(path, std::regex(KERNEL_DETAIL_REG));
        fileList.insert(fileList.end(), files.begin(), files.end());
    }
    if (fileList.empty()) {
        ServerLog::Warn("There is no kernel file.");
        return {};
    }
    return fileList;
}

bool KernelParse::Parse(const RankEntry &rankEntry) {
    if (threadPool == nullptr) {
        ServerLog::Error("Failed to get thread pool in kernel parse.");
        return false;
    }
    auto kernelFiles = GetKernelFiles({rankEntry.parseFolder});
    if (kernelFiles.empty()) {
        ServerLog::Warn("Kernel file is empty.");
        return false;
    }
    SetParseCallBack();
    Timeline::ParserStatusManager::Instance().SetParserStatus(
        KERNEL_PREFIX + rankEntry.rankId, Timeline::ParserStatus::INIT);
    threadPool->AddTask(PreParseTask, TraceIdManager::GetTraceId(), kernelFiles, rankEntry.rankId, rankEntry.fileId);
    return true;
}

void KernelParse::PreParseTask(
    const std::vector<std::string> &filePathList, const std::string &rankId, const std::string &fileId) {
    std::string message;
    Dic::Module::Timeline::ParserStatusManager::Instance().WaitStartParse();
    if (!InitParser(filePathList, rankId, fileId, message)) {
        Timeline::ParserStatusManager::Instance().SetFinishStatus(KERNEL_PREFIX + rankId);
        ServerLog::Error("Failed to parse summary files for fileId:", rankId, "reason: ", message);
        ParseEndCallBack(rankId, "", false, message);
    }
}

void KernelParse::PostParseTask(
    const std::set<std::string> &devices, const std::string &rankId, const std::string &fileId) {
    Timeline::ParserStatusManager::Instance().SetFinishStatus(KERNEL_PREFIX + rankId);
    if (Global::BaselineManager::Instance().IsBaselineRankId(rankId)) {
        return;
    }
    if (devices.size() == 1 && devices.count(rankId) == 1) {
        ParseEndCallBack(rankId, fileId, true, "");
    } else {
        for (const std::string &device : devices) {
            auto tmpFileId = std::string().append(MSPROF_PREFIX).append(rankId).append(MSPROF_CONNECT).append(device);
            ParseEndCallBack(tmpFileId, fileId, true, "");
        }
    }
}

bool KernelParse::InitParser(const std::vector<std::string> &filePathList, const std::string &rankId,
    const std::string &fileId, std::string &message) {
    if (filePathList.empty()) {
        return false;
    }
    if (!Timeline::ParserStatusManager::Instance().SetRunningStatus(KERNEL_PREFIX + rankId)) {
        ServerLog::Info("Pre task skip this kernel file.");
        return false;
    }
    std::string dbPath = fileId;
    auto databaseTmp = Timeline::DataBaseManager::Instance().CreateSummaryDatabase(rankId, fileId);
    auto database = std::dynamic_pointer_cast<TextSummaryDataBase, VirtualSummaryDataBase>(databaseTmp);
    if (database == nullptr) {
        message = "Failed to get summary database, fileId: ." + rankId + " filePath: " + filePathList[0];
        return false;
    }
    if (!database->IsOpen() && !database->OpenDb(dbPath, false)) {
        message = "Failed to init summary database. fileId: " + rankId + " filePath: " + filePathList[0] +
            " fileId: " + dbPath;
#if defined(__linux__) || defined(__APPLE__)
        message += FILE_DESCRIPTOR_RUN_OUT_MESSAGE;
#endif
        return false;
    }
    if (database->HasFinishedParseLastTime()) {
        uint64_t minTimestamp = database->QueryMinStartTime();
        Timeline::TraceTime::Instance().UpdateTime(minTimestamp, 0);
        std::set<std::string> devices = database->QueryRankIds();
        PostParseTask(devices, rankId, dbPath);
        return true;
    }
    if (!database->DropTable() or !database->CreateTable() or !database->SetConfig() or
        !database->UpdateParseStatus(NOT_FINISH_STATUS)) {
        message = "Failed to init summary database. fileId: " + rankId + " filePath: " + filePathList[0] +
            " fileId: " + dbPath;
        return false;
    }
    if (!ParseTask(filePathList, rankId, message) or !database->UpdateParseStatus(FINISH_STATUS)) {
        return false;
    }
    return true;
}

bool KernelParse::ParseTask(
    const std::vector<std::string> &filePathList, const std::string &fileId, std::string &message) {
    std::string statusId = KERNEL_PREFIX + fileId;
    if (!ValidateUtil::CheckCsvFileList(filePathList)) {
        message = "Check file Failed: " + fileId;
        return false;
    }
    std::set<std::string> devices = {};
    for (const auto &filePath : filePathList) {
        if (!ParseKernelCsv(filePath, fileId, statusId, message, devices)) {
            return false;
        }
    }
    // 判断是否为训练场景
    PostParseTask(devices, fileId, FileUtil::GetDbPath(filePathList[0], fileId));
    return true;
}

bool KernelParse::GetUtilizationColumns(const std::vector<std::string> &rowVector, std::vector<std::string> &columns) {
    if (rowVector.size() > 100) { // no more than 100 columns
        return false;
    }
    size_t index = 0;
    auto it = std::find(rowVector.begin(), rowVector.end(), FIELD_AICORE_TIME);
    if (it != rowVector.end()) {
        index = static_cast<size_t>(std::distance(rowVector.begin(), it));
    }
    if (index == 0) {
        return true;
    }
    for (auto i = index; i < rowVector.size(); ++i) {
        std::string origin = rowVector.at(i);
        std::map<std::string, std::string> strPairs = {
            {"(us)", "_us_"}, {"(%)", "_PCT_"}, {"(GB/s)", "_GB_s_"}, {"(KB)", "_KB_"}, {"\r", ""}};
        for (const auto &item : strPairs) {
            size_t pos = origin.find(item.first);
            if (pos != std::string::npos) {
                origin.replace(pos, item.first.length(), item.second);
            }
        }
        // 表头只能是字母、数字、下划线、短线、空格
        if (!RegexUtil::RegexMatch(origin, PMU_HEADER_WHITE_LIST_REG)) {
            ServerLog::Error("Invalid pmu header %", origin);
            columns.clear();
            return false;
        }
        columns.push_back(origin);
    }
    return true;
}

bool KernelParse::CheckHeaderFieldAndFilterParseFunc(std::vector<std::string> rowVector,
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>> &parseFuncList) {
    bool flag = false;
    std::sort(rowVector.begin(), rowVector.end());
    // 检查基础的字段
    for (auto item : VALID_HEADERS) {
        if (std::includes(rowVector.begin(), rowVector.end(), item.begin(), item.end())) {
            flag = true;
            break;
        }
    }
    if (!flag) {
        ServerLog::Error("Failed to check base head field.");
        return false;
    }
    // 根据字段匹配解析函数，可扩展
    for (auto &it : parseFuncMap) {
        std::vector key = std::vector<std::string>(it.first);
        std::sort(key.begin(), key.end());
        if (std::includes(rowVector.begin(), rowVector.end(), key.begin(), key.end())) {
            parseFuncList.push_back(it.second);
        }
    }
    if (parseFuncList.empty()) {
        ServerLog::Error("There is no valid parsing function.");
        return false;
    }

    return true;
}

inline void KernelParse::ParsePyTorchOpBaseInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    if (dataMap.count(DEVICE_ID)) {
        kernel.rankId = row[dataMap.at(DEVICE_ID)];
    } else {
        kernel.rankId = fileId;
    }
    kernel.name = row[dataMap.at(FIELD_NAME)];
    kernel.type = row[dataMap.at(FIELD_TYPE)];
    kernel.acceleratorCore = row[dataMap.at(FIELD_ACCELERATOR_CORE)];
    kernel.duration = NumberUtil::StringToDouble(row[dataMap.at(FIELD_DURATION)], true);
    kernel.waitTime = NumberUtil::StringToDouble(row[dataMap.at(FIELD_WAIT_TIME)], true);
    if (blockDimReplacedByBlockNum) {
        kernel.blockNum = NumberUtil::StringToLongLong(row[dataMap.at(FIELD_BLOCK_NUM)]);
    } else {
        kernel.blockNum = NumberUtil::StringToLongLong(row[dataMap.at(FIELD_BLOCK_DIM)]);
    }
}

inline void KernelParse::ParsePyTorchStepInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    kernel.stepId = row[dataMap.at(STEP_ID)];
}

inline void KernelParse::ParseTaskIdInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Dic::Module::Summary::Kernel &kernel) {
    kernel.taskId = row[dataMap.at(FIELD_TASK_ID)];
}

inline void KernelParse::ParseMsProfOpBaseInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    kernel.rankId = row[dataMap.at(DEVICE_ID)];
    kernel.name = row[dataMap.at(FIELD_OP_NAME)];
    kernel.type = row[dataMap.at(FIELD_OP_TYPE)];
    kernel.acceleratorCore = row[dataMap.at(FIELD_TASK_TYPE)];
    kernel.duration = NumberUtil::StringToDouble(row[dataMap.at(FIELD_TASK_DURATION)], true);
    kernel.waitTime = NumberUtil::StringToDouble(row[dataMap.at(FIELD_TASK_WAIT_TIME)], true);
    if (blockDimReplacedByBlockNum) {
        kernel.blockNum = NumberUtil::StringToLongLong(row[dataMap.at(FIELD_BLOCK_NUM)]);
    } else {
        kernel.blockNum = NumberUtil::StringToLongLong(row[dataMap.at(FIELD_BLOCK_DIM)]);
    }
}

inline void KernelParse::ParseShapeInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    kernel.inputDataTypes = row[dataMap.at(FIELD_INPUT_DATA_TYPES)];
    kernel.inputShapes = row[dataMap.at(FIELD_INPUT_SHAPES)];
    kernel.inputFormats = row[dataMap.at(FIELD_INPUT_FORMATS)];
    kernel.outputDataTypes = row[dataMap.at(FIELD_OUTPUT_DATA_TYPES)];
    kernel.outputShapes = row[dataMap.at(FIELD_OUTPUT_SHAPES)];
    kernel.outputFormats = row[dataMap.at(FIELD_OUTPUT_FORMATS)];
}

inline void KernelParse::ParseOpStateInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    kernel.state = row[dataMap.at(FIELD_OP_STATE)];
}

inline void KernelParse::ParseStartTimeInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    const std::string &timestamp = row[dataMap.at(FIELD_START_TIME)];
    kernel.startTime = NumberUtil::TimestampUsToNsStable(timestamp);
}

inline void KernelParse::ParseTaskStartTimeInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    const std::string &timestamp = row[dataMap.at(FIELD_TASK_START_TIME)];
    kernel.startTime = NumberUtil::TimestampUsToNsStable(timestamp);
}

inline void KernelParse::ParseAICoreMetricsInfoData(const std::map<std::string, size_t> &dataMap,
    const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel) {
    if (dataMap.find(FIELD_AICORE_TIME) == dataMap.end()) {
        return;
    }
    for (size_t i = dataMap.at(FIELD_AICORE_TIME); i < row.size(); ++i) {
        kernel.utilizationInfo.push_back(row.at(i));
    }
}

bool KernelParse::ProcessHeaderGetParseFunc(std::shared_ptr<TextSummaryDataBase> db,
    std::vector<std::string> &rowVector, std::vector<std::string> &columns, std::map<std::string, size_t> &dataMap,
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>> &parseProcessList) {
    // 检查表头并拿到每一类数据对应的解析函数
    if (!CheckHeaderFieldAndFilterParseFunc(rowVector, parseProcessList)) {
        return false;
    }
    // 拿到每一类数据在哪一列，存储在dataMap中
    for (size_t i = 0; i < rowVector.size(); ++i) {
        dataMap[rowVector[i]] = i;
    }
    blockDimReplacedByBlockNum = (std::find(rowVector.begin(), rowVector.end(), FIELD_BLOCK_NUM) != rowVector.end());
    // 获取每一列，更新db表
    if (!GetUtilizationColumns(rowVector, columns) or !db->ExtendColumns(TABLE_KERNEL, columns) or
        !db->InitStmt(columns)) {
        return false;
    }
    return true;
}

bool KernelParse::ParseKernelCsv(const std::string &filePath, const std::string &rankId, const std::string &statusId,
    std::string &message, std::set<std::string> &devices) {
    auto start = std::chrono::high_resolution_clock::now();
    ServerLog::Info("Start to parse kernel detail. rankId: ", rankId, ", file path: ", filePath);
    std::ifstream file = OpenReadFileSafely(filePath);
    std::string line;
    std::map<std::string, size_t> dataMap;
    auto db = std::dynamic_pointer_cast<TextSummaryDataBase, VirtualSummaryDataBase>(
        Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId));
    if (db == nullptr) {
        ServerLog::Error("Failed to get connection.");
        return false;
    }
    // 用来存储数据处理的系列函数
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseProcessList;

    std::string realDeviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    std::vector<std::string> columns;
    uint64_t lineNumber = 0;
    while (getline(file, line)) {
        lineNumber++;
        if (Timeline::ParserStatusManager::Instance().GetParserStatus(statusId) != Timeline::ParserStatus::RUNNING) {
            ServerLog::Error("Parsing process of kernel_details.csv is interrupted.");
            return false;
        }
        // 获取每一行数据存储在rowVector里面
        std::vector<std::string> rowVector = StringUtil::StringSplit(line);
        // 如果是表头且非空
        if (lineNumber == 1 and !rowVector.empty()) {
            // 获取每一列，更新db表
            ProcessHeaderGetParseFunc(db, rowVector, columns, dataMap, parseProcessList);
            continue;
        }
        // 如果解析行数与列映射map大小不一致，则认为异常数据，直接丢弃，避免后续ParseFunc执行时出现越界
        if (rowVector.size() != dataMap.size()) {
            ServerLog::Warn("Invalid data is discarded in the line: ", lineNumber);
            continue;
        }
        // 解析非表头数据存储在db里
        Kernel kernel{};
        for (const auto &parseFunc : parseProcessList) {
            parseFunc(dataMap, rowVector, realDeviceId, kernel);
        }
        // 如果这列数据和列名个数对不上，说明可能有数据缺了，不存储这一行
        if (kernel.utilizationInfo.size() != columns.size()) {
            ServerLog::Warn("Invalid data is discarded in the line: ", lineNumber);
            continue;
        }
        // 记录有多少device
        devices.emplace(dataMap.find(DEVICE_ID) != dataMap.end() ? rowVector[dataMap[DEVICE_ID]] : realDeviceId);
        // 读取每一行数据写入kernel内
        db->InsertKernelDetail(kernel, columns);
    }
    // 读取剩下的数据写入kernel内
    db->SaveKernelDetail(columns);
    ServerLog::Info("Finish to parse kernel detail, ", filePath, " cost time:",
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start)
            .count());
    Timeline::TraceTime::Instance().UpdateTime(db->QueryMinStartTime(), 0);
    file.close();
    return true;
}

void KernelParse::ParseEndCallBack(
    const std::string &rankId, const std::string &fileId, bool result, const std::string &msg) {
    ServerLog::Info("Parse kernel file end: ", rankId);
    // 错误处理逻辑后续增加
    if (!result) {
        return;
    }
    auto &instance = KernelParse::Instance();
    if (instance.parseEndCallback != nullptr) {
        instance.parseEndCallback(rankId, fileId, result, msg);
    }
}

void KernelParse::ParseCallBack(
    const std::string &rankId, const std::string &fileId, bool result, const std::string &msg) {
    auto *session = WsSessionManager::Instance().GetSession();
    if (session == nullptr) {
        ServerLog::Error("Failed to get session for summary callback.");
        return;
    }
    if (rankId.empty()) {
        auto event = std::make_unique<Protocol::ModuleResetEvent>();
        event->moduleName = MODULE_OPERATOR;
        event->result = true;
        event->reset = true;
        session->OnEvent(std::move(event));
    } else {
        std::string realRankId = rankId;
        if (auto pos = rankId.find(MSPROF_PREFIX); pos != std::string::npos) {
            realRankId =
                rankId.substr(pos + MSPROF_PREFIX.size(), rankId.find(MSPROF_CONNECT) - pos - MSPROF_PREFIX.size());
        }
        auto event = std::make_unique<Protocol::OperatorParseStatusEvent>();
        event->moduleName = MODULE_OPERATOR;
        event->result = true;
        event->data.rankId = rankId;
        event->data.status = result;
        event->data.error = msg;
        event->fileId = fileId;
        event->rankList = Timeline::TrackInfoManager::Instance().GetRankListByFileId(fileId, realRankId);
        session->OnEvent(std::move(event));
    }
}

void KernelParse::SetParseCallBack() {
    std::function<void(const std::string, const std::string, bool, const std::string)> func = std::bind(
        ParseCallBack, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    KernelParse::Instance().SetParseEndCallBack(func);
}

bool KernelParse::Parse(const std::vector<std::string> &filePaths, const std::string &rankId,
    const std::string &selectedFolder, const std::string &fileId) {
    if (threadPool == nullptr) {
        ServerLog::Error("Failed to get thread pool in kernel parse.");
        return false;
    }
    // 获取kernel文件
    std::vector<std::string> kernelFile = FileUtil::FindFilesWithFilter(selectedFolder, std::regex(KERNEL_DETAIL_REG));
    if (kernelFile.empty()) {
        return false;
    }
    // 初始化DB
    std::string dbPath = FileUtil::GetDbPath(kernelFile[0], fileId);
    Timeline::DataBaseManager::Instance().CreateSummaryDatabase(rankId, dbPath);
    // 初始化解析状态
    Timeline::ParserStatusManager::Instance().SetParserStatus(KERNEL_PREFIX + rankId, Timeline::ParserStatus::INIT);
    threadPool->AddTask(PreParseTask, TraceIdManager::GetTraceId(), kernelFile, rankId, fileId);
    return true;
}

void KernelParse::Reset() {
    ServerLog::Info("Summary reset. wait task completed.");
    ParseCallBack("", "", true, "");
    if (threadPool != nullptr) {
        threadPool->Reset();
    }
    ServerLog::Info("Summary task completed.");
    auto databaseList = Timeline::DataBaseManager::Instance().GetAllSummaryDatabase();
    for (auto &db : databaseList) {
        auto database = dynamic_cast<TextSummaryDataBase *>(db);
        if (database != nullptr) {
            database->ReleaseStmt();
            database->CloseDb();
        }
    }
    Timeline::DataBaseManager::Instance().Clear(Timeline::DatabaseType::SUMMARY);
}

} // end of namespace Dic::Module::Summary
