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
#include <string_view>
#include "ProjectParserJson.h"
#include "ModuleRequestHandler.h"
#include "TraceFileSimulationParser.h"
#include "ClusterParseThreadPoolExecutor.h"
#include "ClusterFileParser.h"
#include "DataBaseManager.h"
#include "MemoryParse.h"
#include "ParserStatusManager.h"
#include "EventNotifyThreadPoolExecutor.h"
#include "BaselineManager.h"
#include "ProjectExplorerManager.h"
#include "MetaDataParser.h"
#include "MetaDataCacheManager.h"
#include "TraceTime.h"
#include "TimeUtil.h"
#include "ProjectAnalyze.h"
#include "KernelParse.h"
#include "ParseUnitManager.h"
#include "TrackInfoManager.h"
#include "JsonFileProcess.h"

namespace Dic::Module {
using namespace Timeline;
using namespace Global;
using namespace Dic::Server;

namespace {
constexpr std::string_view PROCESS_SCHEDULING_PID = "Process Scheduling";
constexpr std::string_view CPU_SCHEDULING_PID = "CPU Scheduling";
constexpr std::string_view TRACE_EVENTS_KEY = "traceEvents";

enum class FtraceJsonCheckResult { NOT_FOUND, MATCHED, MISMATCHED };

FtraceJsonCheckResult CheckFtraceSchedulingData(const document_t &jsonDoc) {
    const json_t *events = nullptr;
    if (jsonDoc.IsArray()) {
        events = &jsonDoc;
    } else if (jsonDoc.IsObject() && jsonDoc.HasMember(TRACE_EVENTS_KEY.data()) &&
        jsonDoc[TRACE_EVENTS_KEY.data()].IsArray()) {
        events = &jsonDoc[TRACE_EVENTS_KEY.data()];
    }
    if (events == nullptr) {
        return FtraceJsonCheckResult::NOT_FOUND;
    }
    for (const auto &event : events->GetArray()) {
        if (!event.IsObject() || !event.HasMember("ph") || !event["ph"].IsString() ||
            std::string_view(event["ph"].GetString(), event["ph"].GetStringLength()) != "X") {
            continue;
        }
        if (!event.HasMember("pid") || !event["pid"].IsString()) {
            return FtraceJsonCheckResult::MISMATCHED;
        }
        std::string_view pid(event["pid"].GetString(), event["pid"].GetStringLength());
        return pid == PROCESS_SCHEDULING_PID || pid == CPU_SCHEDULING_PID ? FtraceJsonCheckResult::MATCHED
                                                                          : FtraceJsonCheckResult::MISMATCHED;
    }
    return FtraceJsonCheckResult::NOT_FOUND;
}

bool HasFtraceJsonImportData(const std::vector<Global::ProjectExplorerInfo> &projectInfos) {
    return std::any_of(projectInfos.begin(), projectInfos.end(),
        [](const auto &project) { return ProjectParserJson::IsFtraceJsonData(project.fileName); });
}
} // namespace

// LCOV_EXCL_BR_START
void ProjectParserJson::Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos,
    ImportActionRequest &request, ImportActionResponse &response) {
    // 基础信息填充
    FillBaseResponseInfo(request, response, projectInfos);
    // 获取rankid及文件映射关系信息
    // GetRankEntryMap()会调用FileUtil中的方法，而FileUtil中方法会抛出错误，但这些错误不会影响导入解析流程
    // 所以调用方法前将错误保存，调用方法后将错误清空，避免抛出GetRankEntryMap()方法中遭遇的报错
    ModuleRequestHandler::SetResponseErrorFromRequestContext(response);
    std::map<std::string, RankEntry> rankListMap = GetRankEntryMap(projectInfos, false);
    ModuleRequestHandler::ResetRequestContextError();
    UpdateRankIdToDevice(rankListMap);
    std::for_each(rankListMap.begin(), rankListMap.end(), [](const auto &item) {
        Timeline::DataBaseManager::Instance().SetDataType(Timeline::DataType::TEXT, item.second.fileId);
    });

    // 设置基础响应内容
    SetBaseAction(rankListMap, response, projectInfos[0].projectType);
    response.body.isFtrace = response.body.isFtrace || HasFtraceJsonImportData(projectInfos);
    // 解析内容
    auto projectTypeEnum = Global::ProjectExplorerManager::GetProjectType(projectInfos);
    if (projectTypeEnum == ProjectTypeEnum::SIMULATION) {
        SetParseCallBack(Timeline::TraceFileSimulationParser::Instance());
        response.body.isSimulation = true;
        auto [hasJson, hasMemoryData, hasOperatorData] = CheckHasJsonMemoryDataOperatorData(projectInfos);
        response.body.isOnlyTraceJson =
            hasJson && !hasMemoryData && !hasOperatorData; // isOnlyTraceJson 包括 aclGraphDebugJson
        for (const auto &rankEntry : rankListMap) {
            Timeline::TraceFileSimulationParser::Instance().Parse(
                rankEntry.second.parseFileList, rankEntry.first, rankEntry.second.parseFolder, rankEntry.second.fileId);
        }
        TraceTime::Instance().SetIsSimulation(true);
        return;
    }
    bool isCluster = CheckIsOpenClusterTag(request.params.projectAction, projectTypeEnum, projectInfos[0].projectName);
    response.body.isCluster = isCluster;
    SetParseCallBack(_fileParser);
    if (rankListMap.size() >= PENDIND_CRITICAL_VALUE) {
        response.body.isPending = true;
    }
    auto [hasJson, hasMemoryData, hasOperatorData] = CheckHasJsonMemoryDataOperatorData(projectInfos);
    response.body.isOnlyTraceJson =
        hasJson && !hasMemoryData && !hasOperatorData && !isCluster; // isOnlyTraceJson 包括 aclGraphDebugJson
    ModuleRequestHandler::SetResponseResult(response, true);
    std::for_each(projectInfos.begin(), projectInfos.end(), [](const auto &project) {
        if (!Global::ProjectExplorerManager::Instance().UpdateParseFileInfo(
                project.projectName, project.subParseFileInfo)) {
            ServerLog::Error("Failed to update project in parsing");
        }
    });
    ParserJsonData(rankListMap, projectInfos, isCluster);
}
// LCOV_EXCL_BR_STOP

void ProjectParserJson::FillBaseResponseInfo(const ImportActionRequest &request, ImportActionResponse &response,
    const std::vector<ProjectExplorerInfo> &projectInfos) {
    ModuleRequestHandler::SetBaseResponse(request, response);
    std::for_each(projectInfos.begin(), projectInfos.end(), [&response](const ProjectExplorerInfo &info) {
        std::copy(info.projectFileTree.begin(), info.projectFileTree.end(),
            std::back_inserter(response.body.projectFileTree));
    });
    response.command = Protocol::REQ_RES_IMPORT_ACTION;
    response.moduleName = MODULE_TIMELINE;
}

std::map<std::string, RankEntry> ProjectParserJson::GetRankEntryMap(
    const std::vector<Global::ProjectExplorerInfo> &projectInfos, bool isBaseline) {
    // 获取单卡文件，并根据单卡所在目录获取其单卡信息
    std::map<std::string, RankEntry> rankToTraceMap;
    for (const auto &project : projectInfos) {
        bool isMultiCluster = project.GetClusterInfos().size() > 1;
        for (const auto &parseFileInfo : project.subParseFileInfo) {
            std::string fileId = parseFileInfo->fileId;
            bool isDevice = parseFileInfo->type == ParseFileType::DEVICE_CHIP;
            std::vector<std::string> jsonFiles = GetJsonFileUnderFolder(parseFileInfo->parseFilePath);
            if (!CheckParseFileInfoSize(parseFileInfo, jsonFiles)) {
                continue;
            }
            std::string rankId = FileUtil::GetRankIdFromFile(jsonFiles[0]);
            // 如果rankId重复了，添加_{数字}后缀，应用场景：算子调优支持以一个文件夹下多个子文件夹方式导入指令流水图
            rankId = AddSuffixToDuplicatedRankId(rankToTraceMap, rankId);
            std::string deviceId = isDevice ? rankId : GetDeviceId(parseFileInfo->parseFilePath, rankId);
            std::string cluster = parseFileInfo->clusterId;
            if (isDevice) {
                rankId = parseFileInfo->deviceId;
                deviceId = rankId;
            }
            parseFileInfo->deviceId = deviceId;
            if (isMultiCluster) {
                rankId = StringUtil::StrJoin(parseFileInfo->clusterId, "_", rankId);
            } else {
                parseFileInfo->clusterId.clear();
            }
            if (isBaseline) {
                rankId = StringUtil::StrJoin("Baseline_", rankId);
            }
            std::string rankName = rankId;
            RankEntry &entry = rankToTraceMap[rankId];
            entry.projectType = project.projectType;
            entry.deviceId = deviceId;
            parseFileInfo->rankId = rankId;
            entry.rankId = rankId;
            entry.fileId = fileId;
            entry.parseFolder = parseFileInfo->parseFilePath;
            entry.isDevice = isDevice;
            RankInfo rankInfo{parseFileInfo->clusterId, parseFileInfo->host, parseFileInfo->rankId,
                parseFileInfo->deviceId, rankName};
            entry.rankInfo.emplace_back(rankInfo);
            TrackInfoManager::Instance().SetRankListByFileId(fileId, rankInfo);
            TrackInfoManager::Instance().AddRankToCluster(cluster, parseFileInfo->rankId);
            entry.parseFileList = jsonFiles;
        }
    }
    return rankToTraceMap;
}

// 当rankId重复时，增加_{数字}后缀，最多100000（和搜索文件时的最大允许文件数相同），如果超过100000会覆盖
std::string ProjectParserJson::AddSuffixToDuplicatedRankId(
    const std::map<std::string, RankEntry> &rankToTraceMap, const std::string &rankId) {
    if (rankToTraceMap.find(rankId) == rankToTraceMap.end()) {
        return rankId;
    }
    const int indexMax = 100000;
    int index = 2;
    while (index <= indexMax) {
        std::string rankIdWithSuffix = rankId + "_" + std::to_string(index);
        if (rankToTraceMap.find(rankIdWithSuffix) == rankToTraceMap.end()) {
            return rankIdWithSuffix;
        }
        ++index;
    }
    return rankId;
}

bool ProjectParserJson::CheckParseFileInfoSize(
    const std::shared_ptr<Global::ParseFileInfo> &parseFileInfo, std::vector<std::string> &jsonFiles) const {
    if (jsonFiles.empty()) {
        return false;
    }
    if (jsonFiles.size() > JSON_FILE_COUNT_LIMIT) {
        ServerLog::Warn("The number of json fragments in the ",
            StringUtil::GetPrintAbleString(parseFileInfo->parseFilePath), " exceeds ",
            std::to_string(JSON_FILE_COUNT_LIMIT));
        return false;
    }
    int64_t jsonFileSize = 0;
    for (const auto &item : jsonFiles) {
        int64_t singleJsonFileSize = fileReader->GetFileSize(item);
        if (singleJsonFileSize > JSON_MAX_FILE_SIZE || singleJsonFileSize + jsonFileSize > JSON_MAX_FILE_SIZE) {
            ServerLog::Warn("The file size in the ", StringUtil::GetPrintAbleString(parseFileInfo->parseFilePath),
                " exceeds ", std::to_string(JSON_MAX_FILE_SIZE));
            return false;
        }
        jsonFileSize += singleJsonFileSize;
    }
    return true;
}

std::vector<std::string> ProjectParserJson::GetJsonFileUnderFolder(const std::string &path) {
    std::vector<std::string> jsonFiles;
    if (!FileUtil::IsFolder(path)) {
        jsonFiles.emplace_back(path);
        return jsonFiles;
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return jsonFiles;
    }
    for (const auto &file : files) {
        if (IsJsonValid(file)) {
            std::string jsonFile = FileUtil::SplicePath(path, file);
            jsonFiles.emplace_back(jsonFile);
        }
    }
    return jsonFiles;
}

void ProjectParserJson::ParserJsonData(const std::map<std::string, RankEntry> &rankListMap,
    const std::vector<Global::ProjectExplorerInfo> &projectInfos, bool isShowCluster) {
    std::vector<std::string> fileList;
    for (const auto &item : projectInfos) {
        fileList.push_back(item.fileName);
    }
    // 对metadata数据进行解析
    ParserMetaData(projectInfos);
    bool isParseJson = rankListMap.size() < PENDIND_CRITICAL_VALUE;
    for (const auto &rankEntry : rankListMap) {
        if (!Summary::KernelParse::Instance().Parse(rankEntry.second)) {
            ServerLog::Warn("Failed to parse kernel files.");
        }
        if (!Memory::MemoryParse::Instance().Parse(rankEntry.second)) {
            ServerLog::Warn("Failed to parse memory files.");
        }
        if (!isParseJson) {
            /// FIX: JSON 解析情况目前有两种类型：trace view json 和 aclgraph debug json
            /// 需要区分这两种 json
            /// aclgraph debug json => Enum 就是对应的值，即 ACLGRAPH_DEBUG
            /// trace view json     => Enum 统一归类为 TRACE，给 parse/cards 接口使用
            auto projectTypeEnum = static_cast<ProjectTypeEnum>(rankEntry.second.projectType);
            if (projectTypeEnum != ProjectTypeEnum::ACLGRAPH_DEBUG) {
                projectTypeEnum = ProjectTypeEnum::TRACE;
            }
            ParserStatusManager::Instance().SetPendingStatus(
                rankEntry.first, {projectTypeEnum, rankEntry.second.parseFileList});
            continue;
        }
        _fileParser.Parse(rankEntry.second.parseFileList, rankEntry.second.rankId, rankEntry.second.parseFolder,
            rankEntry.second.fileId);
    }
    auto projectTypeEnum = Global::ProjectExplorerManager::GetProjectType(projectInfos);
    auto clusterInfos = Global::ProjectExplorerManager::GetClusterFilePath(projectInfos);
    if (clusterInfos.empty()) {
        std::for_each(projectInfos.begin(), projectInfos.end(), [&clusterInfos](const ProjectExplorerInfo &item) {
            auto cluster = std::make_shared<ParseFileInfo>();
            cluster->parseFilePath = item.fileName;
            cluster->type = ParseFileType::CLUSTER;
            cluster->clusterId = item.fileName;
            cluster->subParseFile = item.subParseFileInfo;
            clusterInfos.emplace_back(cluster);
        });
    }
    auto clusterParse = [projectTypeEnum, isShowCluster, &projectInfos, this](const auto &item) {
        Timeline::ClusterParseThreadPoolExecutor::Instance().GetThreadPool()->AddTask(ProjectParserJson::ClusterProcess,
            TraceIdManager::GetTraceId(), item, projectTypeEnum, isShowCluster, dataPathToDbMap,
            projectInfos[0].projectName);
    };
    std::for_each(clusterInfos.begin(), clusterInfos.end(), clusterParse);
    Timeline::EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->AddTask(
        ParsePostProcess, TraceIdManager::GetTraceId(), clusterInfos);
}

bool ProjectParserJson::isSimulation(std::string filePath) {
    std::ifstream file = OpenReadFileSafely(filePath);
    if (!file.is_open()) {
        return false;
    }
    std::string headerString;
    int64_t contentStart = 0;
    file.seekg(contentStart, std::ios::beg);
    char startTemp;
    while (file.get(startTemp)) {
        headerString += startTemp;
        if (startTemp == '[') {
            break;
        }
        contentStart++;
    }
    if (headerString.find("profilingType") != std::string::npos && headerString.find("op") != std::string::npos) {
        return true;
    }
    return false;
}

void ProjectParserJson::SetParseCallBack(FileParser &fileParser) {
    std::function<void(const std::string, const std::string, bool, const std::string)> func = std::bind(
        ParseEndCallBack, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    fileParser.SetParseEndCallBack(func);

    // 复用解析完成回调函数设置逻辑
    std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> progressFunc =
        std::bind(ParseProgressCallBack, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
            std::placeholders::_4);
    fileParser.SetParseProgressCallBack(progressFunc);
}

void ProjectParserJson::ClusterProcess(std::shared_ptr<ParseFileInfo> clusterInfo, ProjectTypeEnum projectType,
    bool isShowCluster, std::map<std::string, std::vector<std::string>> &dataPathToDbMap,
    const std::string &projectName) {
    ParserStatusManager::Instance().WaitStartParse();
    if (clusterInfo == nullptr || clusterInfo->type != ParseFileType::CLUSTER || clusterInfo->parseFilePath.empty()) {
        ServerLog::Warn("Invalid cluster to parsed, end process");
        return;
    }
    std::string parseClusterResult = PARSE_RESULT_NONE;
    if (projectType == ProjectTypeEnum::TEXT_CLUSTER) {
        // 集群解析，如果集群已解析，则只会初始化db，然后结束流程
        // database先传空指针，等完成mstt分析之后再对该指针赋值
        ClusterFileParser clusterFileParser(
            clusterInfo->parseFilePath, nullptr, clusterInfo->clusterId + TimeUtil::Instance().NowStr());
        if (clusterFileParser.ParseClusterFiles()) {
            ServerLog::Info("The cluster file is parsed successfully.");
            parseClusterResult = PARSE_RESULT_OK;
            dataPathToDbMap[clusterInfo->parseFilePath].push_back(clusterFileParser.GetClusterDbPath());
            ClusterParseThreadPoolExecutor::Instance().GetThreadPool()->AddTask(
                ClusterProcessAsyncStep, TraceIdManager::GetTraceId(), clusterFileParser);
        } else {
            ServerLog::Warn("Failed to parse cluster files.");
            parseClusterResult = PARSE_RESULT_FAIL;
        }
    }
    // send event
    ProjectParserBase::ParseClusterEndProcess(parseClusterResult, isShowCluster, clusterInfo->parseFilePath);
    SaveDbPath(projectName, dataPathToDbMap);
}

void ProjectParserJson::ClusterProcessAsyncStep(ClusterFileParser clusterFileParser) {
    std::string parseClusterResult;
    if (clusterFileParser.ParseClusterStep2Files()) {
        ServerLog::Info("The cluster step2 file is parsed successfully.");
        parseClusterResult = PARSE_RESULT_OK;
    } else {
        ServerLog::Warn("Failed to parse cluster step2 files.");
        parseClusterResult = PARSE_RESULT_FAIL;
    }
    // send event
    ServerLog::Info("Parse Cluster File end, send event");
    auto event = std::make_unique<ParseClusterStep2CompletedEvent>();
    event->moduleName = MODULE_TIMELINE;
    event->result = true;
    event->body.clusterPath = clusterFileParser.GetClusterPath();
    event->body.parseResult = std::move(parseClusterResult);
    SendEvent(std::move(event));
}

std::vector<std::string> ProjectParserJson::FindAllTraceFile(const std::string &path, std::string &error) {
    std::vector<std::string> traceFiles;
    if (path == "browser") {
        return FindTraceFile(ExecUtil::SelectFolder(), error, curScene);
    }
    auto files = FindTraceFile(path, error, curScene);
    if (files.empty()) {
        ServerLog::Warn("Can't find trace file");
    }
    traceFiles.insert(traceFiles.end(), files.begin(), files.end());
    return traceFiles;
}

std::vector<std::string> ProjectParserJson::FindTraceFile(
    const std::string &path, std::string &error, std::string &curScene) {
    std::vector<std::string> traceFiles = {};
    if (!FileUtil::CheckFilePathLength(path)) {
        error =
            " File path length is limit " + std::to_string(FileUtil::GetFilePathLengthLimit()) + ",please shorten it!";
        return traceFiles;
    }
    if (!FileUtil::IsFolder(path)) {
        size_t length = JSON_FILE_SUFFIX.size();
        if (path.size() > length && path.substr(path.size() - length) == JSON_FILE_SUFFIX) {
            traceFiles.emplace_back(path);
        }
        return traceFiles;
    }
    FindTraceFiles(path, 0, error, traceFiles, curScene);
    return traceFiles;
}

void ProjectParserJson::FindTraceFiles(const std::string &path, int depth, std::string &error,
    std::vector<std::string> &traceFiles, std::string &curScene) {
    if (!std::empty(error)) {
        return;
    }
    if (!FileUtil::IsWithinRecursionLimit(traceFiles, depth, error)) {
        return;
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return;
    }
    if (std::find(folders.begin(), folders.end(), ASCEND_PROFILER_OUTPUT) != folders.end()) {
        curScene = "train";
        FindAscendFolder(path, traceFiles);
        return;
    }
    if (std::find(folders.begin(), folders.end(), MINDSTUDIO_PROFILER_OUTPUT) != folders.end()) {
        std::string tmpPath = FileUtil::SplicePath(path, MINDSTUDIO_PROFILER_OUTPUT);
        if (FileUtil::IsFolder(tmpPath)) {
            curScene = "infer";
            FindTraceFiles(tmpPath, depth + 1, error, traceFiles, curScene);
            return;
        }
    }

    for (const auto &folder : folders) {
        std::string tmpPath = FileUtil::SplicePath(path, folder);
        FindTraceFiles(tmpPath, depth + 1, error, traceFiles, curScene);
    }
    for (const auto &file : files) {
        if (IsJsonValid(file)) {
            traceFiles.push_back(FileUtil::SplicePath(path, file));
        }
    }
}

bool ProjectParserJson::IsJsonValid(const std::string &fileName) {
    static std::string reg = R"(^(trace_view|trace|msprof(_slice)?(_[0-9]{1,15}){1,4})\.json$)";
    auto result = RegexUtil::RegexMatch(fileName, reg);
    return result.has_value();
}

// LCOV_EXCL_BR_START
void ProjectParserJson::FindAscendFolder(const std::string &path, std::vector<std::string> &traceFiles) {
    std::string traceFilePath = FileUtil::SplicePath(path, ASCEND_PROFILER_OUTPUT);
    traceFilePath = FileUtil::SplicePath(traceFilePath, "trace_view.json");
    if (FileUtil::IsRegularFile(traceFilePath)) {
        traceFiles.emplace_back(traceFilePath);
        return;
    }
    std::string error;
    std::function<void(const std::string &, int)> find = [&find, &traceFiles, &error](
                                                             const std::string &path, int depth) {
        if (!std::empty(error)) {
            return;
        }
        if (!FileUtil::IsWithinRecursionLimit(traceFiles, depth, error)) {
            return;
        }
        std::vector<std::string> folders;
        std::vector<std::string> files;
        if (!FileUtil::FindFolders(path, folders, files)) {
            return;
        }
        for (const auto &folder : folders) {
            std::string tmpPath = FileUtil::SplicePath(path, folder);
            find(tmpPath, depth + 1);
        }
        for (const auto &file : files) {
            if (IsJsonValid(file)) {
                traceFiles.push_back(FileUtil::SplicePath(path, file));
            }
        }
    };
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return;
    }
    static std::string reg = R"(PROF_[_\d\w]{0,64})";
    for (const auto &folder : folders) {
        if (!RegexUtil::RegexMatch(folder, reg).has_value()) {
            continue;
        }
        std::string tmpPath = FileUtil::SplicePath(path, folder);
        find(tmpPath, 0);
        break;
    }
    if (!std::empty(error)) {
        ServerLog::Warn(StringUtil::GetPrintAbleString(path), " warn is: ", error);
    }
}

ProjectTypeEnum ProjectParserJson::GetProjectType(const std::string &dataPath) {
    std::string error;
    if (IsACLGraphDebugJSON(dataPath)) {
        return ProjectTypeEnum::ACLGRAPH_DEBUG;
    }
    std::vector<std::string> traceFiles = FindAllTraceFile(dataPath, error);
    bool isCluster = (traceFiles.size() > 1 && (curScene == "train" || curScene == "infer")) ||
        ClusterFileParser::CheckIsCluster(dataPath);
    if (isCluster) {
        return ProjectTypeEnum::TEXT_CLUSTER;
    }
    if (!std::empty(traceFiles) && isSimulation(traceFiles[0])) {
        return ProjectTypeEnum::SIMULATION;
    }
    return ProjectTypeEnum::TRACE;
}

std::vector<std::string> ProjectParserJson::GetParseFileByImportFile(
    const std::string &importFile, std::string &error) {
    // 如果是文件，直接返回
    if (!FileUtil::IsFolder(importFile)) {
        return {importFile};
    }
    // 分别获取trace、operator、memory文件
    auto traceFiles = FindAllTraceFile(importFile, error);
    auto opFiles = FileUtil::FindFilesWithFilter(importFile, std::regex(KERNEL_DETAIL_REG));
    auto memoryFiles = FileUtil::FindFilesWithFilter(importFile, std::regex(memoryRecordReg));
    if (traceFiles.empty() && opFiles.empty() && memoryFiles.empty()) {
        error = "No parsable text files found";
        ServerLog::Info(error);
        return {importFile};
    }
    // 将所有文件的父目录放到一个set集合中（利用set进行去重）
    std::set<std::string> resultSet;
    for (const auto &item : traceFiles) {
        resultSet.insert(FileUtil::GetParentPath(item));
    }
    for (const auto &item : opFiles) {
        resultSet.insert(FileUtil::GetParentPath(item));
    }
    for (const auto &item : memoryFiles) {
        resultSet.insert(FileUtil::GetParentPath(item));
    }

    if (resultSet.empty()) {
        return {importFile};
    }
    // 转换成vector返回
    std::vector<std::string> result(resultSet.begin(), resultSet.end());
    return result;
}

void ProjectParserJson::ParserClusterBaseline(
    const Global::ProjectExplorerInfo &projectInfo, BaselineInfo &baselineInfo) {
    std::string clusterPath = BaselineManager::Instance().GetBaseLineClusterPath();
    std::string uniqueKey = FileUtil::GetFileName(clusterPath) + TimeUtil::Instance().NowStr();
    // 集群解析，如果集群已解析，则只会初始化db，然后结束流程
    // database先传空指针，等完成mstt解析之后再对该指针赋值
    ClusterFileParser clusterFileParser(clusterPath, nullptr, uniqueKey);
    baselineInfo.fileId = clusterFileParser.GetClusterDbPath();
    if (clusterFileParser.ParseClusterFiles()) {
        ServerLog::Info("The cluster file is parsed successfully.");
        ClusterParseThreadPoolExecutor::Instance().GetThreadPool()->AddTask(
            [](ClusterFileParser parser) -> bool { return parser.ParseClusterStep2Files(); },
            TraceIdManager::GetTraceId(), clusterFileParser);
    }
}
// LCOV_EXCL_BR_STOP

void ProjectParserJson::ParserSingleCardBaseline(
    const Global::ProjectExplorerInfo &projectInfos, Global::BaselineInfo &baselineInfo) {
    std::string filePath = baselineInfo.parsedFilePath;
    std::vector<std::string> jsonFiles = GetJsonFileUnderFolder(filePath);
    if (std::empty(jsonFiles)) {
        return;
    }
    // 判断项目类型，如果是算子调优数据，则直接解析
    auto projectTypeEnum = static_cast<ProjectTypeEnum>(projectInfos.projectType);
    // 创建db连接池
    std::string dbPath;
    // 设置单个json文件作为基线时，db文件名设置为`${该文件名称}_mindstudio_insight_data.db`，和导入单个json的db文件名保持一致
    // 否则设置单个json作为基线，系统视图卡序号下拉框不会显示基线卡
    if (StringUtil::EndWith(filePath, ".json")) {
        dbPath = FileUtil::GetSingleFileIdWithDb(filePath);
    } else {
        dbPath = FileUtil::GetDbPath(jsonFiles[0]);
    }
    bool isParsed = DataBaseManager::Instance().IsContainDatabasePath(dbPath);
    std::map<std::string, RankEntry> rankListMap = GetRankEntryMap({projectInfos}, !isParsed);
    // 过滤非目标目录下的节点
    for (auto it = rankListMap.begin(); it != rankListMap.end();) {
        if (it->second.parseFolder != filePath) {
            it = rankListMap.erase(it);
        } else {
            it++;
        }
    }
    if (std::empty(rankListMap)) {
        Global::BaselineManager::Instance().SetBaselineInfo(baselineInfo);
        baselineInfo.errorMessage = "Json get rank id failed!";
        return;
    }
    std::string rankId = rankListMap.begin()->first;
    baselineInfo.rankId = rankId;
    baselineInfo.cardName = rankId;
    baselineInfo.fileId = dbPath;
    baselineInfo.isFtrace = std::any_of(jsonFiles.begin(), jsonFiles.end(),
        [](const std::string &jsonFile) { return ProjectParserJson::IsFtraceJsonData(jsonFile); });
    Global::BaselineManager::Instance().SetBaselineInfo(baselineInfo);
    UpdateRankIdToDevice(rankListMap);
    if (isParsed) {
        ServerLog::Warn("Init Baseline, Already parsed.");
        return;
    }
    if (!DataBaseManager::Instance().CreateTraceConnectionPool(rankId, dbPath)) {
        ServerLog::Error("Failed to create connection pool. fileId:", rankId, ". path:", dbPath);
    }

    if (projectTypeEnum == ProjectTypeEnum::SIMULATION) {
        if (!TraceFileSimulationParser::Instance().HasCallbackFuncSet()) {
            SetParseCallBack(TraceFileSimulationParser::Instance());
        }
        Timeline::TraceFileSimulationParser::Instance().Parse(jsonFiles, rankId, filePath, baselineInfo.fileId);
        return;
    }
    if (!_fileParser.HasCallbackFuncSet()) {
        SetParseCallBack(_fileParser);
    }
    ParseBaselineTraceFile(jsonFiles, rankId, baselineInfo.fileId, filePath);
}

void ProjectParserJson::ParseBaselineTraceFile(const std::vector<std::string> &jsonFiles, const std::string &rankId,
    const std::string &fileId, const std::string &filePath) {
    // 如果是系统调优数据，分别解析trace、kernel和memory数据
    if (!_fileParser.Parse(jsonFiles, rankId, filePath, fileId)) {
        ServerLog::Warn("Failed to parse baseline trace files.");
    }

    if (!Summary::KernelParse::Instance().Parse(std::vector<std::string>(), rankId, filePath, fileId)) {
        ServerLog::Warn("Failed to parse baseline kernel files.");
    }

    if (!Memory::MemoryParse::Instance().Parse(std::vector<std::string>(), rankId, filePath, fileId)) {
        ServerLog::Warn("Failed to parse baseline memory files.");
    }
    Timeline::EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->AddTask(
        SendAllParseSuccess, TraceIdManager::GetTraceId());
}

void ProjectParserJson::ParserBaseline(
    const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) {
    if (projectInfo.fileInfoMap.empty()) {
        return;
    }
    // 判断是否为集群
    if (baselineInfo.isCluster) {
        ParserClusterBaseline(projectInfo, baselineInfo);
    } else {
        ParserSingleCardBaseline(projectInfo, baselineInfo);
    }
}

void ProjectParserJson::ParserMetaData(const std::vector<Global::ProjectExplorerInfo> &projectInfos) {
    for (const auto &project : projectInfos) {
        for (const auto &item : project.subParseFileInfo) {
            std::string parent = FileUtil::GetParentPath(item->parseFilePath);
            std::string metaDataFilePath = FileUtil::SplicePath(parent, PROFILER_METADATA_FILE);
            if (!FileUtil::IsRegularFile(metaDataFilePath)) {
                ServerLog::Error("Meta data file % is not valid.", metaDataFilePath);
                continue;
            }
            auto groupInfoList = MetaDataParser::ParserParallelGroupInfoByFilePath(metaDataFilePath);
            MetaDataCacheManager::Instance().AddParallelGroupInfo(groupInfoList);
            if (MetaDataCacheManager::Instance().GetDistributedArgsInfo() != std::nullopt) {
                continue;
            }
            std::optional<DistributedArgs> args = MetaDataParser::ParserDistributedArgsByFilePath(metaDataFilePath);
            MetaDataCacheManager::Instance().SetDistributedArgsInfo(args);
        }
    }
}

bool ProjectParserJson::ExistJsonFormatFile(const std::string &file) {
    if (file.empty()) {
        return false;
    }
    std::string error;
    std::string select = (file == "browser") ? ExecUtil::SelectFolder() : file;
    std::string scene;
    auto traceFiles = FindTraceFile(file, error, scene);
    auto opFiles = FileUtil::FindFilesWithFilter(file, std::regex(KERNEL_DETAIL_REG));
    auto memoryFiles = FileUtil::FindFilesWithFilter(file, std::regex(memoryRecordReg));
    if (traceFiles.empty() && opFiles.empty() && memoryFiles.empty()) {
        error = "Not find valid json text dir!";
        ServerLog::Info(error);
        return false;
    }
    return true;
}

bool ProjectParserJson::IsACLGraphDebugJSON(const std::string &filePath) {
    if (filePath.empty()) {
        return false;
    }

    std::ifstream file(filePath, std::ios::binary); // 二进制模式避免换行转换
    if (!file.is_open() || file.fail()) {
        return false;
    }

    // =============== 阶段1: 定位根数组后的首个对象起始 ===============
    bool foundRootArray = false;
    char c;
    // 跳过空白，寻找根数组 '['
    while (file.get(c)) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        if (c == '[') {
            foundRootArray = true;
            break;
        }
        return false; // 非数组根结构，不符合预期
    }
    if (!foundRootArray || file.eof()) {
        return false;
    }

    // 跳过 '[' 后的空白，寻找首个 '{'（第一个元素必须是对象）
    while (file.get(c)) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        if (c == '{') {
            break;
        } // 找到目标对象起点
        // 其他字符均视为无效
        return false;
    }
    if (file.eof()) {
        return false;
    }

    // =============== 阶段2: 状态机提取完整首个对象（含嵌套） ===============
    std::string firstObject;
    firstObject += c; // 加入起始 '{'

    int braceCount = 1; // 花括号嵌套深度（已含起始{）

    while (braceCount > 0 && file.get(c)) {
        firstObject += c;
        if (c == '{') {
            ++braceCount;
        } else if (c == '}') {
            if (--braceCount == 0) {
                break;
            } // 首对象完整结束
        }
    }
    // 对象未闭合（文件截断/格式错误）
    if (braceCount != 0) {
        return false;
    }
    // =============== 阶段3: 仅在提取的对象内正则匹配 ===============
    // 严格匹配小写 "aclGraph"（移除 icase 标志）
    static const std::regex pattern(
        R"("pid":\s*"[^"]*aclGraph")", std::regex_constants::optimize); // 仅 optimize，不忽略大小写
    return std::regex_search(firstObject, pattern);
}

bool ProjectParserJson::IsFtraceJsonData(const std::string &filePath) {
    if (filePath.empty() || FileUtil::IsFolder(filePath)) {
        return false;
    }

    FileReader fileReader;
    for (const auto &position : JsonFileProcess::SplitFile(filePath)) {
        std::string content = fileReader.ReadJsonArray(filePath, position.first, position.second);
        if (content.empty()) {
            continue;
        }

        std::string error;
        auto jsonDoc = JsonUtil::TryParse(content, error);
        if (!jsonDoc.has_value()) {
            ServerLog::Warn("Failed to parse ftrace json candidate. file:", StringUtil::GetPrintAbleString(filePath),
                " error:", error);
            continue;
        }
        FtraceJsonCheckResult checkResult = CheckFtraceSchedulingData(jsonDoc.value());
        if (checkResult == FtraceJsonCheckResult::MATCHED) {
            return true;
        }
        if (checkResult == FtraceJsonCheckResult::MISMATCHED) {
            return false;
        }
    }
    return false;
}

std::tuple<bool, bool, bool> ProjectParserJson::CheckHasJsonMemoryDataOperatorData(
    const std::vector<Global::ProjectExplorerInfo> &projectInfos) {
    static const std::regex memoryRegex(memoryRecordReg);
    static const std::regex kernelRegex(KERNEL_DETAIL_REG);

    bool hasJson = false;
    bool hasMemoryCsv = false;
    bool hasOperatorCsv = false;

    for (const auto &project : projectInfos) {
        if (hasJson && hasMemoryCsv && hasOperatorCsv) {
            break; // 提前退出
        }

        for (const auto &item : project.subParseFileInfo) {
            std::string fileName = FileUtil::GetFileName(item->parseFilePath);
            if (!hasJson && StringUtil::EndWith(fileName, JSON_FILE_SUFFIX)) {
                hasJson = true;
            } else if (!hasMemoryCsv && std::regex_match(fileName, memoryRegex)) {
                hasMemoryCsv = true;
            } else if (!hasOperatorCsv && std::regex_match(fileName, kernelRegex)) {
                hasOperatorCsv = true;
            }

            if (hasJson && hasMemoryCsv && hasOperatorCsv) {
                break; // 内层循环提前退出
            }
        }
    }

    return {hasJson, hasMemoryCsv, hasOperatorCsv}; // 使用列表初始化
}

void ProjectParserJson::BuildProjectExploreInfo(
    ProjectExplorerInfo &info, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(info, parsedFiles);
    std::for_each(parsedFiles.begin(), parsedFiles.end(),
        [&info](const std::string &file) { ProjectParserJson::BuildProjectFromParseFile(info, file); });
}

void ProjectParserJson::BuildProjectFromParseFile(ProjectExplorerInfo &info, const std::string &parseFile) {
    std::vector<std::string> parentFolders = GetParentFileList(info.fileName, parseFile);
    // Json工程层次：project-cluster-host-rank
    auto parseFileInfoRank = std::make_shared<ParseFileInfo>();
    parseFileInfoRank->parseFilePath = parseFile;
    parseFileInfoRank->type = ParseFileType::RANK;
    parseFileInfoRank->subId = parseFile;
    parseFileInfoRank->curDirName = FileUtil::GetFileName(parseFile);
    parseFileInfoRank->fileId = GetFileIdWithDb(parseFile);
    parseFileInfoRank->projectType = info.projectType;
    // import single file
    if (FileUtil::IsRegularFile(parseFile) || parseFileInfoRank->subId == info.fileName) {
        parseFileInfoRank->subId = FileUtil::GetFileName(parseFile);
        info.AddSubParseFileInfo(info.fileName, ParseFileType::PROJECT, parseFileInfoRank);
        return;
    }
    // 设置cluster信息
    std::string cluster;
    std::string clusterPrefix;
    constexpr uint64_t clusterFolderCount = 2;
    if (parentFolders.size() >= clusterFolderCount && !IsMindFormsRankData(parentFolders)) {
        std::tie(cluster, clusterPrefix) = GetClusterInfo(parentFolders);
        if (info.GetSubParseFileInfo(clusterPrefix, ParseFileType::CLUSTER) == nullptr) {
            auto clusterInfo = std::make_shared<ParseFileInfo>();
            clusterInfo->subId = clusterPrefix;
            clusterInfo->type = ParseFileType::CLUSTER;
            clusterInfo->clusterId = FileUtil::GetFileName(cluster);
            clusterInfo->parseFilePath = clusterPrefix;
            clusterInfo->curDirName = FileUtil::GetFileName(cluster);
            clusterInfo->projectType = info.projectType;
            info.AddSubParseFileInfo(info.fileName, ParseFileType::PROJECT, clusterInfo);
        }
        parentFolders.erase(parentFolders.begin());
    }
    if (!clusterPrefix.empty()) {
        parseFileInfoRank->clusterId = FileUtil::GetFileName(cluster);
    } else {
        parseFileInfoRank->clusterId = FileUtil::GetFileName(info.fileName);
    }
    AddRankDeviceParseFileInfo(info, parseFileInfoRank);
}

std::string ProjectParserJson::GetFileIdWithDb(const std::string &filePath) {
    // 仅当导入单json或者csv时会进入如下if，以保证单目录导入两个json或csv后db文件不会相互覆盖
    if (!FileUtil::IsFolder(filePath)) {
        return FileUtil::GetSingleFileIdWithDb(filePath);
    }

    std::string rankId = FileUtil::GetProfilerFileId(FileUtil::SplicePath(filePath, "mindstudio_data.db"));
    std::string dbPath = FileUtil::SplicePath(filePath, "mindstudio_data.db");
    return FileUtil::GetDbPath(dbPath, rankId);
}

bool IsSingleJSONWithoutValidDeviceId(const std::map<std::string, RankEntry> &rankEntry) {
    // 检查是否只有一个元素
    if (rankEntry.size() != 1) {
        return false;
    }

    // 获取唯一的元素
    const auto &entry = *rankEntry.begin();
    const std::string &deviceId = entry.second.deviceId;

    // 检查是否所有字符都不是数字，或者包含非数字字符
    // 如果字符串为空，或者包含非数字字符，返回 true
    if (deviceId.empty()) {
        return true;
    }

    // 检查是否包含非数字字符
    return std::any_of(deviceId.begin(), deviceId.end(), [](unsigned char c) { return !std::isdigit(c); });
}

void ProjectParserJson::UpdateRankIdToDevice(std::map<std::string, RankEntry> &rankEntry) {
    if (IsSingleJSONWithoutValidDeviceId(rankEntry)) {
        // 如果是单 JSON 文件，且对应的 deviceId 不是数字，只设置 TraceConnectionPool 不设置 rankIdToDeviceIdMap
        const auto &entry = *rankEntry.begin();
        DataBaseManager::Instance().CreateTraceConnectionPool(entry.first, entry.second.fileId);
        return;
    }
    for (auto &[rankId, entry] : rankEntry) {
        DataBaseManager::Instance().CreateTraceConnectionPool(rankId, entry.fileId);
        DataBaseManager::Instance().UpdateRankIdToDeviceId(entry.fileId, rankId, entry.deviceId);
    }
}

void ProjectParserJson::SetBaseAction(
    const std::map<std::string, RankEntry> &rankListMap, ImportActionResponse &response, int64_t projectType) {
    for (const auto &rankEntry : rankListMap) {
        auto folder = rankEntry.second.parseFolder;
        std::string fileId = rankEntry.second.fileId;
        SetBaseActionOfResponse(
            response, rankEntry.first, fileId, FileUtil::GetRankIdFromPath(fileId), {folder}, projectType);
        if (rankEntry.second.isDevice) {
            response.body.isMultiDevice = true;
        }
    }
}

std::string ProjectParserJson::GetDeviceId(const std::string &parseFolder, const std::string &rankId) {
    auto deviceIdFromMemoryFile = GetDeviceIdFromMemory(parseFolder);
    if (!deviceIdFromMemoryFile.empty()) {
        return deviceIdFromMemoryFile;
    }
    auto deviceIdFromKernelFile = GetDeviceIdFromKernel(parseFolder);
    if (!deviceIdFromKernelFile.empty()) {
        return deviceIdFromKernelFile;
    }
    auto deviceIdFromPath = GetDeviceIdFromPath(parseFolder);
    if (!deviceIdFromPath.empty()) {
        return deviceIdFromPath;
    }
    return rankId;
}

std::string ProjectParserJson::GetDeviceIdFromMemory(const std::string &parseFolder) {
    auto memoryFiles = MemoryParse::Instance().GetMemoryFile(parseFolder);
    if (memoryFiles.operatorFiles.empty()) {
        return "";
    }
    std::string operatorFile = *memoryFiles.operatorFiles.begin();
    return GetDeviceIdFromCSVFile(operatorFile);
}

std::string ProjectParserJson::GetDeviceIdFromKernel(const std::string &parseFolder) {
    auto kernelFiles = Summary::KernelParse::GetKernelFiles({parseFolder});
    if (kernelFiles.empty()) {
        return "";
    }
    return GetDeviceIdFromCSVFile(kernelFiles[0]);
}

std::string ProjectParserJson::GetDeviceIdFromCSVFile(const std::string &filePath) {
    auto file = OpenReadFileSafely(filePath);
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    std::map<std::string, std::string> dataMap;
    getline(file, line);
    auto headerRow = StringUtil::StringSplit(line);
    getline(file, line);
    auto dataRow = StringUtil::StringSplit(line);
    // 如果data行缺列数据，则为异常数据。
    if (dataRow.size() < headerRow.size()) {
        ServerLog::Warn("Some columns in the first row of this CSV file are missing.File path: %", filePath);
    }
    for (size_t i = 0; i < std::min(dataRow.size(), headerRow.size()); i++) {
        dataMap[headerRow[i]] = dataRow[i];
    }
    std::string deviceId;
    if (dataMap.find(DEVICE_ID) != dataMap.end()) {
        deviceId = dataMap[DEVICE_ID];
    } else if (dataMap.find(DEVICETYPE) != dataMap.end()) {
        deviceId = dataMap[DEVICETYPE];
    }
    if (StringUtil::StartWith(deviceId, "NPU:")) {
        return deviceId.substr(strlen("NPU:"));
    }
    return deviceId;
}

std::string ProjectParserJson::GetDeviceIdFromPath(const std::string &parseFolder) {
    auto deviceIds = ProjectParserBase::SearchDeviceInfo(FileUtil::GetParentPath(parseFolder));
    if (deviceIds.empty()) {
        return "";
    }
    return deviceIds[0];
}

ProjectAnalyzeRegister<ProjectParserJson> pRegJson(ParserType::JSON);
ProjectAnalyzeRegister<ProjectParserJson> pRegACLGraphDebugJson(ParserType::ACLGRPAH_DEBUG_JSON);
} // Module
// Dic
