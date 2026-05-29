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

#include <algorithm>
#include "pch.h"
#include "PythonUtil.h"
#include "CommunicationMatrixRapidHandler.h"
#include "CommunicationRapidSaxHandler.h"
#include "ConstantDefs.h"
#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "DbClusterDataBase.h"
#include "TraceTime.h"
#include "CollectionUtil.h"
#include "CommunicationGroupParser.h"
#include "MetaDataCacheManager.h"
#include "ClusterFileParser.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
using namespace rapidjson;
using namespace Dic::Module::FullDb;
void ClusterFileParser::ParseCommunication(const std::vector<std::string> &filePathList) {
    if (filePathList.empty()) {
        ServerLog::Error("Communication file list is empty.");
        return;
    }
    const std::string &filePath = FileUtil::PathPreprocess(filePathList[0].c_str());
    Server::ServerLog::Info("ParseCommunication: " + filePath);
    SaxParseJsonFile(filePath, 0);
}

void ClusterFileParser::ParseCommunicationMatrix(const std::vector<std::string> &filePathList) {
    if (filePathList.empty()) {
        ServerLog::Error("Communication matrix file list is empty.");
        return;
    }
    const std::string &filePath = FileUtil::PathPreprocess(filePathList[0].c_str());
    Server::ServerLog::Info("ParseCommunicationMatrix: " + filePath);
    SaxParseJsonFile(filePath, 1);
}

void ClusterFileParser::SaxParseJsonFile(const std::string &filePath, int saxHandlerType) {
    auto start = std::chrono::high_resolution_clock::now();
    auto checkFilePath = FileUtil::CheckPathSecurity(filePath, CHECK_FILE_WRITE);
    if (!checkFilePath) {
        return;
    }
    std::shared_ptr<TextClusterDatabase> textDb = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (textDb == nullptr) {
        ServerLog::Error("Fail to parse json file, file type:", saxHandlerType);
        return;
    }
    // 打开JSON文件
    FILE *fp = fopen(filePath.c_str(), "rb");
    if (fp == nullptr) {
        return;
    }
    constexpr size_t bufferLen = 65536;
    std::string readBuffer;
    readBuffer.reserve(bufferLen);
    rapidjson::FileReadStream is(fp, readBuffer.data(), sizeof(bufferLen));
    rapidjson::Reader reader;
    if (saxHandlerType == 0) {
        CommunicationRapidSaxHandler rapidSaxHandler(textDb, uniqueKey);
        reader.Parse<kParseNumbersAsStringsFlag>(is, rapidSaxHandler);
    } else {
        CommunicationMatrixRapidHandler matrixRapidHandler(textDb, uniqueKey);
        reader.Parse(is, matrixRapidHandler);
    }
    auto end = std::chrono::high_resolution_clock::now();
    ServerLog::Info("End sax_parse_json_file data into db, file:", filePath, "cost time:", (end - start).count());
    fclose(fp);
}

void ClusterFileParser::ParseStepStatisticsFile(const std::vector<std::string> &filePathList) {
    const std::string &filePath = FileUtil::PathPreprocess(filePathList[0].c_str());
    auto start = std::chrono::high_resolution_clock::now();
    if (!ValidateUtil::CheckCsvFile(filePath)) {
        return;
    }
    std::ifstream stepTraceFileCsv = OpenReadFileSafely(filePath);
    std::string line;
    std::map<std::string, size_t> indexMap;
    std::shared_ptr<TextClusterDatabase> textDb = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (textDb == nullptr) {
        ServerLog::Error("Can't get cluster database when parse step statistics file.");
        stepTraceFileCsv.close();
        return;
    }
    bool isHeader = true;
    std::map<std::string, size_t> dataMap;
    while (std::getline(stepTraceFileCsv, line)) {
        std::vector<std::string> tokens = StringUtil::StringSplit(line);
        if (!tokens.empty() and isHeader) {
            // 校验表头，求必要表头和当前文件表头的差集，如果差集数量大于0，则校验不通过
            std::vector<std::string> difference =
                CollectionUtil::CalDifferenceVector(VALID_STEP_STATISTICS_HEADERS, tokens);
            if (difference.size() != 0) {
                ServerLog::Error("The header of step statistics file is invalid, "
                                 "missing header data as follows: %, filePath: %",
                    StringUtil::join(difference, ","), filePath);
                stepTraceFileCsv.close();
                return;
            }
            for (size_t i = 0; i < tokens.size(); ++i) {
                dataMap[tokens[i]] = i;
            }
            isHeader = false;
            continue;
        }
        // 行内容校验，列数不相等则跳过
        if (tokens.size() != dataMap.size()) {
            ServerLog::Warn("Row size is not equal to header number.");
            continue;
        }
        StepStatistic statistic = MapToStepStatistic(dataMap, tokens);
        textDb->InsertStepStatisticsInfo(statistic);
    }
    auto end = std::chrono::high_resolution_clock::now();
    ServerLog::Info("End parse step statistics file data into db, cost time:", (end - start).count());
    stepTraceFileCsv.close();
}

void ClusterFileParser::SaveClusterBaseInfo(const std::string &selectedPath) {
    ClusterBaseInfo baseInfo;
    baseInfo.filePath = selectedPath;
    std::shared_ptr<TextClusterDatabase> textDb = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (textDb == nullptr) {
        ServerLog::Error("Can't get cluster database when sava cluster base info.");
        return;
    }
    std::optional<DistributedArgs> args = MetaDataCacheManager::Instance().GetDistributedArgsInfo();
    if (args.has_value()) {
        baseInfo.config = args.value().config;
        baseInfo.level = PARALLEL_CONFIG_LEVEL_COLLECTED;
    } else {
        bool result = textDb->GetParallelConfigFromStepTrace(baseInfo.config, baseInfo.level);
        if (!result) {
            ServerLog::Error("Failed to get initial parallel config from profiler_metadata.json or step trace.");
        }
    }
    textDb->InsertClusterBaseInfo(baseInfo);
    ServerLog::Info("End save cluster base info data into db, path: ", selectedPath);
}

bool ClusterFileParser::InitCommunicationGroupInfo(std::vector<CommGroupParallelInfo> &groupInfos) {
    // 先解析communication group文件
    std::vector<std::string> communicationGroupList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, std::regex(R"(communication_group.json)"));
    if (communicationGroupList.empty()) {
        ServerLog::Error("Failed to get communicationGroup files");
        return false;
    }
    groupInfos = Communication::CommunicationGroupParser::ParseCommunicationGroup(communicationGroupList[0]);
    if (groupInfos.empty()) {
        return false;
    }
    return true;
}

bool ClusterFileParser::ParseClusterFiles() {
    ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::INIT);
    // 初始化集群
    if (!InitClusterDatabase()) {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        ServerLog::Error("Init cluster database occur an err");
        return false;
    }

    // 判断是跳过解析
    if (!needClearDb) {
        bool skipStatus = SkipClusterParse();
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        return skipStatus;
    }
    // 正常解析
    ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::RUNNING);
    bool parseRes = InitBaseInfoAndMatrixData();
    if (!parseRes) {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
    }
    return parseRes;
}

bool ClusterFileParser::SkipClusterParse() {
    std::shared_ptr<TextClusterDatabase> textDb = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (textDb == nullptr) {
        ServerLog::Error("Fail to skip cluster parse, can't get cluster database when parse cluster files.");
        return false;
    }
    ServerLog::Info("cluster db file is already exist, skip parse ");
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    textDb->QueryExtremumTimestamp(min, max);
    if (min != UINT64_MAX && max != 0) {
        Timeline::TraceTime::Instance().UpdateTime(min, max);
    }
    return true;
}

bool ClusterFileParser::InitBaseInfoAndMatrixData() {
    std::shared_ptr<TextClusterDatabase> textDb = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (textDb == nullptr) {
        ServerLog::Error("Fail to init base info and matrixData, can't get cluster database when parse cluster files.");
        return false;
    }
    // parse communication file
    std::regex patternCommunicationMatrix(R"(cluster_communication_matrix.json)");
    std::vector<std::string> communicationMatrixFileList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, patternCommunicationMatrix);
    std::regex patternCommunicationTime(R"(cluster_communication.json)");
    std::vector<std::string> communicationTimeFileList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, patternCommunicationTime);
    // cluster analysis
    if ((communicationMatrixFileList.empty() && communicationTimeFileList.empty()) &&
        !AttAnalyze(selectedFilePath, ATT_MODEL_MATRIX, AttDataType::TEXT)) {
        return false;
    }
    // 解析group数据并进行落库，解析失败不阻塞进程
    std::vector<CommGroupParallelInfo> groupInfos;
    if (!InitCommunicationGroupInfo(groupInfos) || !textDb->InsertGroupInfos(groupInfos)) {
        ServerLog::Warn("Fail to parse communication group file.");
    }
    // 解析矩阵数据
    std::vector<std::string> communicationMatrixList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, patternCommunicationMatrix);
    if (!communicationMatrixList.empty()) {
        ParseCommunicationMatrix(communicationMatrixList);
    }
    textDb->SaveLastData();
    std::regex patternStepTrace(R"(cluster_step_trace_time.csv)");
    std::vector<std::string> stepTraceFileList = FileUtil::FindFirstFileByRegex(selectedFilePath, patternStepTrace);
    if (!stepTraceFileList.empty()) {
        ParseStepStatisticsFile(stepTraceFileList);
        SaveClusterBaseInfo(selectedFilePath);
    }
    if (!textDb->CreateIndex()) {
        ServerLog::Error("Failed to create index on cluster database. path:", selectedFilePath);
        return false;
    }
    return true;
}

bool ClusterFileParser::BackupExistedClusterFiles(const std::vector<std::string> &backUpMatrixList,
    const std::vector<std::string> &backUpGroupList, const std::vector<std::string> &backUpStepList) {
    if (backUpMatrixList.empty() || backUpGroupList.empty() || backUpStepList.empty()) {
        return false;
    }
    bool isCopyFile = FileUtil::CopyFileByPath(
        FileUtil::PathPreprocess(backUpMatrixList[0]), FileUtil::SplicePath(selectedFilePath, "tmp_matrix.json"));
    isCopyFile = isCopyFile &&
        FileUtil::CopyFileByPath(
            FileUtil::PathPreprocess(backUpStepList[0]), FileUtil::SplicePath(selectedFilePath, "tmp_step.csv"));
    isCopyFile = isCopyFile &&
        FileUtil::CopyFileByPath(
            FileUtil::PathPreprocess(backUpGroupList[0]), FileUtil::SplicePath(selectedFilePath, "tmp_group.json"));
    return isCopyFile;
}

bool ClusterFileParser::RestoreClusterFiles(const std::vector<std::string> &backUpMatrixList,
    const std::vector<std::string> &backUpGroupList, const std::vector<std::string> &backUpStepList) {
    std::string tempMatrixPath = FileUtil::SplicePath(selectedFilePath, "tmp_matrix.json");
    std::string tempStepPath = FileUtil::SplicePath(selectedFilePath, "tmp_step.csv");
    std::string tempGroupPath = FileUtil::SplicePath(selectedFilePath, "tmp_group.json");
    bool reductionFileRes =
        FileUtil::CopyFileByPath(tempMatrixPath, FileUtil::PathPreprocess(backUpMatrixList[0].c_str())) &&
        FileUtil::RemoveFile(tempMatrixPath);
    reductionFileRes = reductionFileRes &&
        FileUtil::CopyFileByPath(tempStepPath, FileUtil::PathPreprocess(backUpStepList[0].c_str())) &&
        FileUtil::RemoveFile(tempStepPath);
    reductionFileRes = reductionFileRes &&
        FileUtil::CopyFileByPath(tempGroupPath, FileUtil::PathPreprocess(backUpGroupList[0].c_str())) &&
        FileUtil::RemoveFile(tempGroupPath);
    return reductionFileRes;
}

bool ClusterFileParser::ParseClusterStep2Files() {
    if (ParserStatusManager::Instance().IsClusterParserFinalState(uniqueKey)) {
        return true;
    }
    // parse communication file
    std::vector<std::string> communicationFileList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, std::regex(R"(cluster_communication.json)"));

    std::vector<std::string> backUpMatrixList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, std::regex(R"(cluster_communication_matrix.json)"));
    std::vector<std::string> backUpGroupList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, std::regex(R"(communication_group.json)"));
    std::vector<std::string> backUpStepList =
        FileUtil::FindFirstFileByRegex(selectedFilePath, std::regex(R"(cluster_step_trace_time.csv)"));

    bool isCopyFile = false;
    // cluster_communication不存在时，将matrix等可能存在的三份文件复制备份到selectedPath下的临时文件中
    if (communicationFileList.empty()) {
        isCopyFile = BackupExistedClusterFiles(backUpMatrixList, backUpGroupList, backUpStepList);
        if (!isCopyFile) {
            ServerLog::Warn("Copy matrix and other files failed.");
        }
    }

    // cluster analysis
    if (communicationFileList.empty() && !AttAnalyze(selectedFilePath, ATT_MODEL_TIME, AttDataType::TEXT)) {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        return false;
    }

    // 如果发生过文件的复制，则将临时文件复制回cluster_analysis_output文件夹中，并且删除临时文件
    if (isCopyFile && !RestoreClusterFiles(backUpMatrixList, backUpGroupList, backUpStepList)) {
        ServerLog::Warn("Copy and clear matrix temp file failed.");
    }
    bool res = TransCommunicationToDb(selectedFilePath, std::regex(R"(cluster_communication.json)"));
    ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
    return res;
}

bool ClusterFileParser::TransCommunicationToDb(
    const std::string &selectedPath, const std::regex &patternCommunication) {
    std::shared_ptr<TextClusterDatabase> textDb = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (textDb == nullptr) {
        ServerLog::Error("Failed to connect to cluster database.", selectedPath);
        return false;
    }
    std::vector<std::string> communicationFileList = FileUtil::FindFirstFileByRegex(selectedPath, patternCommunication);
    if (!communicationFileList.empty()) {
        ParseCommunication(communicationFileList);
    }
    textDb->SaveLastData();
    if (!textDb->CreateTimeIndex()) {
        ServerLog::Error("Failed to CreateTimeIndex on cluster database. path:", selectedPath);
        return false;
    }
    if (ParserStatusManager::Instance().IsClusterParserFinalState(uniqueKey)) {
        ServerLog::Warn("Parser Cluster Status Is Terminal");
        return false;
    }
    textDb->UpdateClusterParseStatus(FINISH_STATUS);
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    textDb->QueryExtremumTimestamp(min, max);
    if (min != UINT64_MAX && max != 0) {
        Timeline::TraceTime::Instance().UpdateTime(min, max);
    }
    return true;
}

bool ClusterFileParser::InitClusterDatabase() {
    clusterDbPath = selectedFilePath + FILE_SEPARATOR + "cluster.db";

    // 创建新的db连接池
    DataBaseManager::Instance().CreateClusterConnectionPool(selectedFilePath, clusterDbPath, DataType::TEXT);
    database = DataBaseManager::Instance().GetClusterDatabase(selectedFilePath);
    std::shared_ptr<TextClusterDatabase> databaseWrite = std::dynamic_pointer_cast<TextClusterDatabase>(database);
    if (databaseWrite == nullptr) {
        ServerLog::Error("Can't get text cluster database.");
        return false;
    }
    std::ifstream file = OpenReadFileSafely(clusterDbPath, std::ios::in);
    if (!file.good()) {
        if (!(databaseWrite->DropAllTable() && databaseWrite->CreateTable() && databaseWrite->SetConfig() &&
                databaseWrite->SetDbVersion() && databaseWrite->InitStmt())) {
            ServerLog::Error("Failed to update databaseWrite. path:", selectedFilePath);
            return false;
        }
    } else {
        // 判断数据库版本是否变更，若变更不能跳过解析
        auto isChange = databaseWrite->IsDatabaseVersionChange();
        std::string status = databaseWrite->QueryParseClusterStatus();
        needClearDb = isChange || status.empty() || strcmp(status.c_str(), FINISH_STATUS.c_str()) != 0;
        if (needClearDb && !(databaseWrite->DropAllTable() && databaseWrite->CreateTable())) {
            ServerLog::Error("Failed to dropAllTable. path:", selectedFilePath, "isChange:", isChange);
            return false;
        }
        if (!(databaseWrite->SetConfig() && databaseWrite->SetDbVersion() && databaseWrite->InitStmt())) {
            ServerLog::Error("Failed to init databaseWrite. path:", selectedFilePath);
            return false;
        }
    }
    return true;
}

// LCOV_EXCL_BR_START
bool ClusterFileParser::CheckDocumentValid(const Document &doc) {
    if (doc.HasParseError()) {
        ServerLog::Error("JSON file is invalid.");
        return false;
    }
    bool isLegal = doc.IsObject() && doc.HasMember("p2p") && doc.FindMember("p2p")->value.IsArray() &&
        doc.HasMember("collective") && doc.FindMember("collective")->value.IsArray();
    if (!isLegal) {
        ServerLog::Error("JSON file is illegal.");
        return false;
    }
    return true;
}

bool ClusterFileParser::AttAnalyze(const std::string &selectedPath, const std::string &mode, AttDataType dataType) {
    ServerLog::Info("Start execute cluster analysis");
    if (!StringUtil::ValidateCommandFilePathParam(selectedPath)) {
        ServerLog::Warn("validate string select path failed! select path", selectedPath);
        return false;
    }

    const std::string scriptPath =
        std::string("msprof_analyze") + FILE_SEPARATOR + "cluster_analyse" + FILE_SEPARATOR + "cluster_analysis.py";
    std::vector<std::string> arguments{"-d", selectedPath};
    if (!mode.empty()) {
        arguments.emplace_back("-m");
        arguments.emplace_back(mode);
    }
    ServerLog::Info("Start execute command, selected path:", selectedPath, " ,mode: ", mode);
    if (PythonUtil::ExecuteScript(scriptPath, arguments) != 0) {
        ServerLog::Warn(
            "Execute cluster analysis failed, skip parse cluster file, selected path:", selectedPath, " ,mode: ", mode);
        return false;
    }
    ServerLog::Info("Execute cluster analysis succeeded, selected path: ", selectedPath, ",mode: ", mode);
    return true;
}
// LCOV_EXCL_BR_STOP

StepStatistic ClusterFileParser::MapToStepStatistic(
    std::map<std::string, size_t> &dataMap, const std::vector<std::string> &tokens) {
    StepStatistic statistic;
    std::string stepId = GetStrValue(dataMap, tokens, FIELD_STEP);
    std::string flag = GetStrValue(dataMap, tokens, FIELD_TYPE);
    std::string order = GetStrValue(dataMap, tokens, FIELD_INDEX);
    statistic.stepId = stepId.empty() ? "0" : stepId;
    statistic.rankId = std::strcmp(flag.c_str(), "rank") == 0 ? order : "";
    // 去掉stage的首尾引号
    if (std::strcmp(flag.c_str(), "stage") == 0 && order.find('\"') != std::string::npos &&
        order.length() > subStrlen) {
        order = order.substr(1, order.length() - subStrlen);
        statistic.stageId = order;
    }
    statistic.computingTime = NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_COMPUTING));
    statistic.pureCommunicationTime =
        NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_COMMUNICATION_NOT_OVERLAPPED));
    statistic.overlapCommunicationTime = NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_OVERLAPPED));
    statistic.communicationTime = NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_COMMUNICATION));
    statistic.freeTime = NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_FREE));
    statistic.stageTime = NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_STAGE));
    statistic.bubbleTime = NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_BUBBLE));
    statistic.pureCommunicationExcludeReceiveTime =
        NumberUtil::StringToDouble(GetStrValue(dataMap, tokens, FIELD_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE));

    std::string prepareTime = GetStrValue(dataMap, tokens, FIELD_PREPARE_TIME);
    statistic.prepareTime = prepareTime.empty() ? -1 : NumberUtil::StringToDouble(prepareTime);

    // 判断表头中是否存在所有并行策略的key值
    bool allParallelKeys = std::all_of(PARALLEL_STRATEGY_HEADERS.begin(), PARALLEL_STRATEGY_HEADERS.end(),
        [&dataMap](const std::string &str) { return dataMap.find(str) != dataMap.end(); });
    // 存在则读取相关的值
    if (allParallelKeys) {
        // 如果非数字字符串，这里会返回0
        statistic.dpIndex = NumberUtil::StringToLong(GetStrValue(dataMap, tokens, FIELD_DP_INDEX));
        statistic.ppIndex = NumberUtil::StringToLong(GetStrValue(dataMap, tokens, FIELD_PP_INDEX));
        statistic.tpIndex = NumberUtil::StringToLong(GetStrValue(dataMap, tokens, FIELD_TP_INDEX));
    }
    return statistic;
}

std::string ClusterFileParser::GetStrValue(
    std::map<std::string, size_t> &dataMap, const std::vector<std::string> &tokens, const std::string &key) {
    if (dataMap.find(key) == dataMap.end()) {
        return "";
    }
    size_t index = dataMap[key];
    return tokens[index];
}

void UpdateTraceTimeOfDb(std::shared_ptr<FullDb::DbClusterDataBase> clusterDatabase) {
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    clusterDatabase->QueryExtremumTimestamp(min, max);
    if (min != UINT64_MAX && max != 0) {
        Timeline::TraceTime::Instance().UpdateTime(min, max);
    }
}

// LCOV_EXCL_BR_START
bool ClusterFileParser::ParserClusterOfDb() {
    ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::INIT);
    std::string tempPath(selectedFilePath);
    // 如果selectedPath是单个文件，则使用该文件所在文件夹作为分析路径
    if (!FileUtil::IsFolder(selectedFilePath)) {
        tempPath = FileUtil::GetParentPath(selectedFilePath);
    }
    std::vector<std::string> clusterPath = FileUtil::FindFilesWithFilter(tempPath, std::regex(clusterDBReg));
    // 集群解析，判断是否已经存在集群db，如果存在则不进行重复解析，如果不存在，则调用mstt进行重新解析
    if (clusterPath.empty()) {
        if (!AttAnalyze(tempPath, ATT_MODEL_DEFAULT, AttDataType::DB)) {
            ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
            return false;
        }
        clusterPath = FileUtil::FindFilesWithFilter(tempPath, std::regex(clusterDBReg));
    }
    if (clusterPath.empty()) {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        return false;
    }
    ServerLog::Info("Cluster Db Path: " + clusterPath[0]);
    clusterDbPath = clusterPath[0];
    Dic::Module::FullDb::DataBaseManager::Instance().CreateClusterConnectionPool(
        selectedFilePath, clusterDbPath, Dic::Module::Timeline::DataType::DB);
    database = Dic::Module::FullDb::DataBaseManager::Instance().GetClusterDatabase(selectedFilePath);
    std::shared_ptr<FullDb::DbClusterDataBase> clusterDatabase = std::dynamic_pointer_cast<DbClusterDataBase>(database);
    if (clusterDatabase == nullptr) {
        ServerLog::Error("Failed to get db cluster connection.");
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        return false;
    }
    if (!clusterDatabase->IsDatabaseVersionChange() && clusterDatabase->HasFinishedParseLastTime()) {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        /// FIX: 单独导入 cluster_analysis.db 时，由于没有 Timeline 的数据需要解析，因此无法运行到 Timeline 的更新 TraceTime 的方法
        /// 这导致 TraceTime 的 minTimestamp 是初始最大值。
        /// 当后面查找通信算子缩略图的数据时，将 start_timestamp 的值减去 TraceTime 的 minTimestamp 后只能得到负值。
        /// 数据处理时发现负值就改成了 0，因此通信算子缩略图的数据所有通信算子开始时间都是 0。
        /// 解决方法：在 db 情况解析集群完成后，更新 TraceTime 的值。
        UpdateTraceTimeOfDb(clusterDatabase);
        return true;
    }

    ClusterBaseInfo baseInfo;
    InitFullDbClusterBaseInfo(clusterDatabase, baseInfo);

    if (!clusterDatabase->DropTable() or !clusterDatabase->CreateTable() or !clusterDatabase->SetDataBaseVersion() or
        !clusterDatabase->UpdatesClusterParseStatus(NOT_FINISH_STATUS)) {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
        return false;
    }
    clusterDatabase->InsertClusterBaseInfo(baseInfo);
    clusterDatabase->UpdatesClusterParseStatus(FINISH_STATUS);
    ServerLog::Info("ParseClusterFiles is success");
    ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::FINISH);
    /// FIX: 单独导入 cluster_analysis.db 时，由于没有 Timeline 的数据需要解析，因此无法运行到 Timeline 的更新 TraceTime 的方法
    /// 这导致 TraceTime 的 minTimestamp 是初始最大值。
    /// 当后面查找通信算子缩略图的数据时，将 start_timestamp 的值减去 TraceTime 的 minTimestamp 后只能得到负值。
    /// 数据处理时发现负值就改成了 0，因此通信算子缩略图的数据所有通信算子开始时间都是 0。
    /// 解决方法：在 db 情况解析集群完成后，更新 TraceTime 的值。
    UpdateTraceTimeOfDb(clusterDatabase);
    return true;
}

void ClusterFileParser::InitFullDbClusterBaseInfo(
    std::shared_ptr<FullDb::DbClusterDataBase> &clusterDatabase, ClusterBaseInfo &baseInfo) {
    if (clusterDatabase == nullptr) {
        return;
    }
    // 如果数据库中初始就有ClusterBaseInfo表，将其中的并行策略信息保存到baseInfo结构体中
    // 将并行策略信息保存到baseInfo结构体后，删除ClusterBaseInfo表，然后新建同名表，按照指定格式存储信息
    // 如果数据库中初始无ClusterBaseInfo表，从ClusterStepTraceTime表获取并行策略信息
    clusterDatabase->SetHasClusterBaseInfoTable();
    if (clusterDatabase->HasClusterBaseInfoTable()) {
        clusterDatabase->QueryDistributedArgs(baseInfo.config, baseInfo.level);
    } else {
        clusterDatabase->GetParallelConfigFromStepTrace(baseInfo.config, baseInfo.level);
    }
}
// LCOV_EXCL_BR_STOP

std::string ClusterFileParser::GetClusterDbPath() { return clusterDbPath; }

ClusterFileParser::ClusterFileParser(
    const std::string &filePath, std::shared_ptr<VirtualClusterDatabase> database, const std::string &uniqueKey)
    : selectedFilePath(filePath), uniqueKey(uniqueKey), database(database) {}

bool ClusterFileParser::CheckIsCluster(const std::string &filePath) {
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (filePath.find(CLUSTER_ANALYSIS_OUTPUT) != std::string::npos) {
        ServerLog::Info("this folder is cluster_analysis_output, Check Cluster is true");
        return true;
    }
    if (!FileUtil::FindFolders(filePath, folders, files)) {
        ServerLog::Info("FindFolders is empty, Check Cluster is false");
        return false;
    }
    return std::any_of(
        folders.begin(), folders.end(), [](std::string &folder) { return folder == CLUSTER_ANALYSIS_OUTPUT; });
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
