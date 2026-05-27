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
#include <climits>
#include "ProjectParserFactory.h"
#include "ProjectParserBin.h"
#include "ProjectParserJson.h"
#include "ProjectParserDb.h"
#include "ProjectParserDbNPUMonitor.h"
#include "ProjectParserMemScope.h"
#include "ProjectParserMemSnapshot.h"
#include "ParserIE.h"
#include "DataBaseManager.h"
#include "ClusterFileParser.h"
#include "TraceTime.h"
#include "MemoryParse.h"
#include "FullDbParser.h"
#include "ProjectExplorerManager.h"
#include "MetaDataCacheManager.h"
#include "ServitizationOpenApi.h"
#include "ParserStatusManager.h"
#include "SystemMemoryDatabaseDef.h"
#include "ProjectAnalyze.h"
#include "TrackInfoManager.h"
#include "ExpertHotspotManager.h"
#include "JsonFileParserManager.h"
#include "RLMstxConfigManager.h"
#include "RenderEngine.h"
#include "TrackInfoManager.h"
#include "JsonParseMemPool.h"
#include "MemScopeParser.h"
#include "MemSnapshotParser.h"
#include "ProjectParserTriton.h"
#include "ProjectParserFtrace.h"

namespace Dic::Module {
using namespace Dic;
using namespace Dic::Server;
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Global;
// 静态变量 锁初始化
std::mutex ParserFactory::mutex;
std::pair<std::string, ParserType> ParserFactory::GetImportType(const std::string &path) {
    // 判别顺序是优先判别只支持单个文件导入的memscope(leaks)/pickle bin ipynb npumonitor aclGraph_debug文件，然后判别服务化调优数据，最后判别系统调优数据
    // 判别系统调优数据时，优先db，然后text，都没有再寻找npumonitor数据
    const std::string filename = FileUtil::GetFileName(path);
    std::string error;
    if (!ProjectParserTriton().GetParseFileByImportFile(path, error).empty()) {
        return std::make_pair(path, ParserType::TRITON_MEMORY);
    }
    if (!FileUtil::IsFolder(path) && ProjectParserMemScope::IsMemScopeDbFile(filename)) {
        return std::make_pair(path, ParserType::DB_MEMSCOPE);
    }
    if (!FileUtil::IsFolder(path) && ProjectParserMemSnapshot::IsSnapshotPickleFile(filename)) {
        return std::make_pair(path, ParserType::PKL_MEM_SNAPSHOT);
    }
    if (!FileUtil::IsFolder(path) && std::regex_match(filename, std::regex(npumonitorDBReg))) {
        return std::make_pair(path, ParserType::DB_NPUMONITOR);
    }
    if (!FileUtil::IsFolder(path) && std::regex_match(filename, std::regex(ftraceDbReg))) {
        return std::make_pair(path, ParserType::DB_FTRACE);
    }
    if (StringUtil::EndWith(path, computeBinSuffix)) {
        return std::make_pair(path, ParserType::BIN);
    }
    if (!FileUtil::IsFolder(path) && ProjectParserJson::IsACLGraphDebugJSON(path)) {
        return std::make_pair(path, ParserType::ACLGRPAH_DEBUG_JSON);
    }
    if (FileUtil::FindIfDbTypeByRegex(path, std::regex(traceViewReg), std::regex(DB_REG))) {
        return std::make_pair(path, ParserType::DB);
    }
    if (ProjectParserJson::ExistJsonFormatFile(path) || ClusterFileParser::CheckIsCluster(path)) {
        return std::make_pair(path, ParserType::JSON);
    }
    if (FileUtil::FindIfDbTypeByRegex(path, std::regex(traceViewReg), std::regex(npumonitorDBReg))) {
        return std::make_pair(path, ParserType::DB_NPUMONITOR);
    }
    return std::make_pair(path, ParserType::OTHER);
}

std::shared_ptr<ProjectParserBase> ParserFactory::GetProjectParser(ParserType allocType) {
    std::shared_ptr<ProjectParserBase> alloc;
    switch (allocType) {
    case ParserType::DB:
        alloc = std::make_shared<ProjectParserDb>();
        break;
    case ParserType::BIN:
        alloc = std::make_shared<ProjectParserBin>();
        break;
    case ParserType::ACLGRPAH_DEBUG_JSON:
        alloc = std::make_shared<ProjectParserJson>(JsonFileParserManager::GetACLGraphDebugParser());
        break;
    case ParserType::JSON:
        alloc = std::make_shared<ProjectParserJson>(JsonFileParserManager::GetTraceFileParser());
        break;
    case ParserType::IE:
        alloc = std::make_shared<ParserIE>();
        break;
    case ParserType::DB_NPUMONITOR:
        alloc = std::make_shared<ProjectParserDbNPUMonitor>();
        break;
    case ParserType::DB_MEMSCOPE:
        alloc = std::make_shared<ProjectParserMemScope>();
        break;
    case ParserType::PKL_MEM_SNAPSHOT:
        alloc = std::make_shared<ProjectParserMemSnapshot>();
        break;
    case ParserType::TRITON_MEMORY:
        alloc = std::make_shared<ProjectParserTriton>();
        break;
    case ParserType::DB_FTRACE:
        alloc = std::make_shared<ProjectParserFtrace>();
        break;
    default:
        alloc = std::make_shared<ProjectParserBase>();
        break;
    }
    return alloc;
}

void ParserFactory::Reset() {
    std::lock_guard<std::mutex> lock(mutex);
    std::shared_ptr<IE::ServitizationOpenApi> openApi = std::make_shared<IE::ServitizationOpenApi>();
    openApi->Reset();
    JsonFileParserManager::Instance().ResetAll();
    Summary::KernelParse::Instance().Reset();
    Memory::MemoryParse::Instance().Reset();
    FullDb::FullDbParser::Instance().Reset();
    MetaDataCacheManager::Instance().Clear();
    MemScopeParser::Instance().Reset();
    MemSnapshotParser::Instance().Reset();
}

void ProjectParserBase::SetBaseActionOfResponse(ImportActionResponse &response, const std::string &rankId,
    const std::string &fileId, const std::string &cardPath, std::vector<std::string> dataPath, int64_t projectType) {
    auto rankList = TrackInfoManager::Instance().GetRankListByFileId(fileId, rankId);
    Action action;
    if (!rankList.empty()) {
        action.cluster = rankList[0].cluster;
    }
    action.cardName = rankId;
    action.rankId = rankId;
    action.result = true;
    action.fileId = fileId;
    // 路径信息，与rankId对应，用于页面上删除时，能够正确找到要删除的甬道信息（目前只有导入单卡数据需要这个信息）
    action.dataPathList = dataPath;
    // 将文件所在路径的三级目录名称作为rank的tooltip信息
    action.cardPath = "Directory: " + cardPath;
    action.projectType = projectType;
    response.body.result.emplace_back(action);
}

void ProjectParserBase::ParseClusterEndProcess(std::string result, bool isShowCluster, const std::string &clusterId) {
    ServerLog::Info("Parse Cluster File end, send event");
    auto event = std::make_unique<ParseClusterCompletedEvent>();
    event->moduleName = MODULE_TIMELINE;
    event->result = true;
    event->body.parseResult = std::move(result);
    event->body.isShowCluster = isShowCluster;
    event->body.clusterPath = clusterId;
    SendEvent(std::move(event));
}

void ProjectParserBase::ParseEndCallBack(
    const std::string &rankId, const std::string &fileId, bool result, const std::string &message) {
    ServerLog::Info("Parse end, fileId:", rankId, ", result:", result);
    if (result) {
        SendParseSuccessEvent(rankId, fileId);
    } else {
        SendParseFailEvent(rankId, fileId, message);
    }
}

void ProjectParserBase::ParseProgressCallBack(
    const std::string &fileId, uint64_t parsedSize, uint64_t totalSize, int progress) {
    auto event = std::make_unique<ParseProgressEvent>();
    event->moduleName = MODULE_TIMELINE;
    event->result = true;
    event->body.fileId = fileId;
    event->body.parsedSize = parsedSize;
    event->body.totalSize = totalSize;
    event->body.progress = progress;
    SendEvent(std::move(event));
}

void ProjectParserBase::SendParseSuccessEvent(const std::string &rankId, const std::string &fileId) {
    auto event = std::make_unique<ParseSuccessEvent>();
    event->moduleName = MODULE_TIMELINE;
    event->result = true;
    event->body.unit.type = "card";
    event->body.unit.metadata.cardId = rankId;
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection. fileId:", fileId);
        return;
    }
    event->body.unit.metadata.cardAlias = database->QueryCardAlias();
    if (!database->QueryExtremumTimestamp(min, max)) {
        return;
    }
    if (min == max && max == 0) {
        event->body.startTimeUpdated = false;
    } else {
        event->body.startTimeUpdated = true;
        TraceTime::Instance().UpdateTime(min, max);
        TraceTime::Instance().UpdateCardTimeDuration(rankId, min, max);
    }
    event->body.maxTimeStamp = TraceTime::Instance().GetDuration();
    event->body.startTime = TraceTime::Instance().GetStartTime();
    event->body.offset = TraceTime::Instance().GetOffsetByFileId(rankId);
    event->body.fileId = fileId;
    event->body.isFullDb = Timeline::DataBaseManager::Instance().GetDataType(fileId) == DataType::DB;
    std::vector<std::string> taskNameList = RL::RLMstxConfigManager::Instance().GetMstxTaskNameList();
    auto mstxSliceList = FullDb::RenderEngine::Instance()->QueryMstxRLDetail(
        rankId, Timeline::DataBaseManager::Instance().GetDataType(fileId), taskNameList);
    event->body.isRl = !mstxSliceList.empty();
    event->body.rankList = TrackInfoManager::Instance().GetRankListByFileId(fileId, rankId);
    SearchMetaData(rankId, fileId, event->body.unit.children);
    SearchGroupedAscendHardwareThreads(fileId, event->body.unit, event->body.threadGroupList);
    SendEvent(std::move(event));
}

void ProjectParserBase::SendParseFailEvent(
    const std::string &rankId, const std::string &fileId, const std::string &message) {
    auto event = std::make_unique<ParseFailEvent>();
    event->moduleName = MODULE_TIMELINE;
    event->result = true;
    event->body.rankId = rankId;
    event->body.error = message;
    event->body.dbPath = fileId;
    SendEvent(std::move(event));
}

bool ProjectParserBase::IsNeedReset(const ImportActionRequest &request) {
    // 如果是切换项目，则必须重置
    if (request.params.projectAction == ProjectActionEnum::TRANSFER_PROJECT) {
        return true;
    }
    // 新增文件时，以下情况需要对当前导入内容进行重置：1.导入数据和原来数据有冲突；2.无冲突，但是当前选中项目与目标项目不一致；
    std::string curProjectName = request.projectName;
    if (request.params.isConflict || (!curProjectName.empty() && curProjectName != request.params.projectName)) {
        return true;
    }
    return false;
}

void ProjectParserBase::SearchGroupedAscendHardwareThreads(
    const std::string &fileId, const Unit &unit, std::vector<ThreadGroup> &groupedThreads) {
    const auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection. fileId:", fileId);
        return;
    }
    database->QueryGroupedAscendHardwareThreadsByModelId(groupedThreads);
    // 循环获取 ThreadGroup 的 cardId processId
    uint8_t updatedGroupNum = 0;
    for (const auto &item : unit.children) {
        if (updatedGroupNum >= groupedThreads.size()) {
            break;
        }
        std::unordered_set<std::string> threadIdSet;
        // 预分配空间，避免多次rehash，提升性能
        threadIdSet.reserve(item->children.size());
        for (const auto &threadPtr : item->children) {
            // 直接插入提取出的 threadId
            threadIdSet.insert(threadPtr->metaData.threadId);
        }

        for (auto &groupedThread : groupedThreads) {
            if (!groupedThread.processId.empty()) {
                continue;
            }
            const bool matched = std::any_of(groupedThread.threadIds.begin(), groupedThread.threadIds.end(),
                [&](const std::string &id) { return threadIdSet.find(id) != threadIdSet.end(); });
            if (matched) {
                groupedThread.processId = item->metaData.processId;
                groupedThread.cardId = unit.metadata.cardId;
                updatedGroupNum++;
                break;
            }
        }
    }
}

void ProjectParserBase::SearchMetaData(
    const std::string &rankId, const std::string &fileId, std::vector<std::unique_ptr<UnitTrack>> &metaData) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection. fileId:", fileId);
        return;
    }
    database->QueryUnitsMetadata(rankId, metaData);
    ProcessMetadata(metaData);
}

void ProjectParserBase::ProcessMetadata(std::vector<std::unique_ptr<UnitTrack>> &metaData) {
    for (const auto &item : metaData) {
        for (const auto &thread : item->children) {
            if (thread->metaData.groupNameValue.empty()) {
                continue;
            }
            // 判断是否为group内容，此处进行了一个拆分判断，是由于数据中可能存在将一个通信甬道拆分成两个的情况（mc2算子）
            // 这个场景会在真正的通信域后加上“Aicpu”进行区分，需要将该字段去掉才能与metadata中通信域的数据相匹配
            std::vector<std::string> groupNameSplit = StringUtil::Split(thread->metaData.groupNameValue, " ");
            std::string realGroupName = groupNameSplit.size() > 1 ? groupNameSplit[0] : thread->metaData.groupNameValue;
            auto groupInfoOpt = MetaDataCacheManager::Instance().GetParallelGroupInfo(realGroupName);
            if (!groupInfoOpt.has_value()) {
                continue;
            }
            thread->metaData.threadName = groupInfoOpt.value().groupName + ":" + thread->metaData.threadName;
            thread->metaData.rankList = groupInfoOpt.value().globalRanks;
        }
    }
}

std::string ProjectParserBase::GetRankIdFromPath(const std::string &filePath, const std::string &importPath) {
    std::string fileId = FileUtil::GetRankIdFromFile(filePath);
    int i = 1;
    std::string result = fileId;
    std::string parentDir = FileUtil::GetParentPath(filePath);
    std::string name = FileUtil::GetFileName(filePath);
    while (DataBaseManager::Instance().HasRankId(DatabaseType::TRACE, result)) {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(result);
        if (database == nullptr) {
            continue;
        }
        std::string dir = FileUtil::GetParentPath(database->GetDbPath());
        if (RegexUtil::RegexMatch(name, R"(^msprof_slice_[0-9_]+\.json$)") && parentDir == dir) {
            return result;
        }
        result = fileId + "_" + std::to_string(++i);
    }
    std::string dbPath = FileUtil::GetDbPath(filePath);
    if (dbPath.length() >= FileUtil::GetFilePathLengthLimit()) {
        const std::string message = dbPath + " length exceed " + std::to_string(FileUtil::GetFilePathLengthLimit());
        SendParseFailEvent("", dbPath, message);
    }
    dataPathToDbMap[importPath].push_back(dbPath);
    return result;
}

std::string ProjectParserBase::GetDbPath(const std::string &filePath, const int index) {
    std::string path(filePath);
    std::string suffix = DB_FILE_SUFFIX;
    if (index != 1) {
        suffix = "_" + std::to_string(index) + suffix;
    }
    auto pos = path.find_last_of('.');
    if (pos != std::string::npos) {
        path.replace(pos, path.size() - pos, suffix);
    } else {
        path.append(suffix);
    }
    return path;
}

bool ProjectParserBase::ParseHeatMapToCluster(const std::vector<std::shared_ptr<ParseFileInfo>> &clusterInfos) {
    bool res = false;
    // 全量db和json场景需要存储集群和单卡的映射关系
    for (const auto &cluster : clusterInfos) {
        std::vector<std::string> rankIdList;
        for (const auto &child : cluster->subParseFile) {
            rankIdList.push_back(child->rankId);
        }
        std::string errorMsg;
        bool tempRes =
            Summary::ExpertHotspotManager::UpdateHeatMapFromProfiling(errorMsg, cluster->parseFilePath, rankIdList);
        if (!tempRes) {
            ServerLog::Warn(errorMsg, " cluster path:", cluster->parseFilePath);
        }
        // 只要有一个解析成功就返回true
        res = res | tempRes;
    }
    return res;
}

void ProjectParserBase::ParsePostProcess(const std::vector<std::shared_ptr<ParseFileInfo>> &clusterInfos) {
    ParserStatusManager::Instance().WaitStartParse();
    // 全量db和json场景需要存储集群和单卡的映射关系
    for (const auto &cluster : clusterInfos) {
        for (const auto &child : cluster->subParseFile) {
            TrackInfoManager::Instance().UpdateClusterDbToFileIdMap(cluster->parseFilePath, child->fileId);
        }
    }

    // 发送解析成功消息
    SendAllParseSuccess();
    // 数据统计内容
    auto event = std::make_unique<ParseHeatmapCompletedEvent>();
    if (!clusterInfos.empty()) {
        std::string errorMsg;
        bool res = ParseHeatMapToCluster(clusterInfos);
        event->body.parseResult = res;
        event->result = true;
    } else {
        event->result = false;
        event->body.parseResult = false;
        event->body.errorMsg = "No cluster data exists.";
    }
    event->moduleName = MODULE_TIMELINE;
    SendEvent(std::move(event));
}

void ProjectParserBase::SendAllParseSuccess() {
    ParserStatusManager::Instance().WaitStartParse();
    std::string notFinishTask = "";
    const int maxSleepTimeMs = 2000;
    int sleepTime = 200;
    while (!ParserStatusManager::Instance().IsAllFinished(notFinishTask)) {
#ifdef INSIGHT_DEBUG
        ServerLog::Info("NotFinishTask is: ", notFinishTask);
#endif
        sleepTime = std::min(sleepTime * 2, maxSleepTimeMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    }
    ServerLog::Info("Send all parse finished");
    auto event = std::make_unique<AllSuccessEvent>();
    event->moduleName = MODULE_MEMORY;
    event->result = true;
    event->body.isAllPageParsed = true;
    for (const auto &item : TraceTime::Instance().ComputeCardMinTimeInfo()) {
        CardOffset cardOffset = {item.first, item.second};
        event->body.cardOffsets.emplace_back(cardOffset);
    }
    event->body.minTime = TraceTime::Instance().GetStartTime();
    Dic::Module::JsonParseMemPool::Instance().Clear();
    SendEvent(std::move(event));
}

void ProjectParserBase::SaveDbPath(
    const std::string &curProjectName, std::map<std::string, std::vector<std::string>> &dataPathToDbMap) {
    Global::ProjectExplorerManager::Instance().UpdateProjectDbPath(curProjectName, dataPathToDbMap);
}

/**
 * 校验前端是否展示集群标签
 *
 * @param action 导入动作（切换项目、导入文件）
 * @param curType 当前文件的类型
 * @param projectName 项目名
 * @return 是否要打开集群标签
 */
bool ProjectParserBase::CheckIsOpenClusterTag(
    ProjectActionEnum action, ProjectTypeEnum curType, const std::string &projectName) {
    // 如果当前类型是集群，则直接返回true
    if (curType == ProjectTypeEnum::TEXT_CLUSTER || curType == ProjectTypeEnum::DB_CLUSTER) {
        return true;
    }
    // 如果当前类型非集群，而非新增文件场景，则直接返回false
    if (action != ProjectActionEnum::ADD_FILE) {
        return false;
    }
    // 新增文件场景下，需要对该项目所有内容进行判断(由于新增导入的场景)
    std::vector<Global::ProjectExplorerInfo> projectInfo =
        Global::ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, {});
    ProjectTypeEnum projectType = Global::ProjectExplorerManager::GetProjectType(projectInfo);
    return (projectType == ProjectTypeEnum::TEXT_CLUSTER || projectType == ProjectTypeEnum::DB_CLUSTER);
}

void ProjectParserBase::BuildProjectExploreInfo(
    ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles) {
    // 默认将import path加入
    auto parseInfo = std::make_shared<ParseFileInfo>();
    parseInfo->type = ParseFileType::PROJECT;
    parseInfo->parseFilePath = projectInfo.fileName;
    parseInfo->subId = projectInfo.fileName;
    parseInfo->curDirName = FileUtil::GetFileName(projectInfo.fileName);
    parseInfo->projectType = projectInfo.projectType;
    projectInfo.projectFileTree.emplace_back(parseInfo);
    projectInfo.fileInfoMap.emplace(parseInfo->subId, parseInfo);
}

bool ProjectParserBase::IsParsedFile(const std::string &file) { return false; }

std::vector<std::string> ProjectParserBase::GetParentFileList(const std::string &prefix, const std::string &filePath) {
    std::vector<std::string> res;
    if (prefix == filePath) {
        return res;
    }
    std::string curPath = FileUtil::GetParentPath(filePath);
    while (!curPath.empty() && curPath != prefix) {
        res.emplace_back(curPath);
        curPath = FileUtil::GetParentPath(curPath);
    }
    return res;
}

std::tuple<std::string, std::string> ProjectParserBase::GetClusterInfo(const std::vector<std::string> &folders) {
    if (folders.empty()) {
        return {"", ""};
    }
    return {folders.back(), folders.back()};
}

std::vector<std::string> ProjectParserBase::SearchDeviceInfo(const std::string &searchPath) {
    // 在PROF_目录下寻找device_x
    static std::regex profRegex("^PROF_");
    static std::regex deviceRegex("^device_([0-9]+)$");
    std::string profDir;
    for (const auto &folder : FileUtil::GetSubDirs(searchPath)) {
        std::string folderName = FileUtil::GetFileName(folder);
        if (std::regex_search(folderName, profRegex)) {
            profDir = folder;
            break;
        }
    }
    std::smatch match;
    std::vector<std::string> res;
    auto subDir = profDir.empty() ? FileUtil::GetSubDirs(searchPath)
                                  : FileUtil::GetSubDirs(FileUtil::SplicePath(searchPath, profDir));
    for (const auto &dir : subDir) {
        std::string fileName = dir;
        if (!std::regex_search(fileName, match, deviceRegex)) {
            continue;
        }
        if (match.size() > 1) {
            res.emplace_back(match[1].str());
        }
    }
    std::sort(res.begin(), res.end());
    return res;
}

void ProjectParserBase::AddRankDeviceParseFileInfo(ProjectExplorerInfo &info, std::shared_ptr<ParseFileInfo> rankInfo) {
    auto deviceIdsGetFromDir = ProjectParserBase::SearchDeviceInfo(FileUtil::GetParentPath(rankInfo->parseFilePath));
    auto deviceIdsFromFile = ProjectParserBase::ParseDeviceInfo(info, rankInfo->parseFilePath);
    if (deviceIdsGetFromDir.size() < 2 || deviceIdsFromFile.size() < 2) { // deviceIds size > 2, multi device
        if (!deviceIdsFromFile.empty()) {
            rankInfo->deviceId = deviceIdsFromFile[0];
        } else if (!deviceIdsGetFromDir.empty()) {
            rankInfo->deviceId = deviceIdsGetFromDir.back();
        } else {
            rankInfo->deviceId = rankInfo->rankId;
        }
        info.AddSubParseFileInfo(rankInfo);
        return;
    }
    std::vector<std::string> deviceId = deviceIdsFromFile;
    std::for_each(deviceId.begin(), deviceId.end(), [&info, &rankInfo](const std::string &deviceId) {
        auto deviceInfo = std::make_shared<ParseFileInfo>();
        deviceInfo->subId = FileUtil::SplicePath(rankInfo->subId, deviceId);
        deviceInfo->parseFilePath = rankInfo->parseFilePath;
        deviceInfo->type = ParseFileType::DEVICE_CHIP;
        deviceInfo->rankId = deviceId;
        deviceInfo->fileId = rankInfo->fileId;
        deviceInfo->clusterId = rankInfo->clusterId;
        deviceInfo->deviceId = deviceId;
        deviceInfo->projectType = info.projectType;
        info.AddSubParseFileInfo(deviceInfo);
    });
}

bool ProjectParserBase::IsMindFormsRankData(const std::vector<std::string> &parentFolders) {
    // mindforms profiling data:   rank_xx/xxx_acend_ms
    if (parentFolders.empty()) {
        return false;
    }
    return RegexUtil::RegexSearch(FileUtil::GetFileName(parentFolders.back()), "^rank_[0-9]+$").has_value();
}

void ProjectParserBase::SendUnitFinishNotify(
    const std::string &fileId, bool res, const std::string &unitName, const std::string &error) {
    auto event = std::make_unique<ParseUnitCompletedEvent>();
    event->body.parseResult = res;
    event->body.unitName = unitName;
    event->body.errorMsg = error;
    event->body.dbId = fileId;
    event->moduleName = MODULE_TIMELINE;
    event->result = true;
    SendEvent(std::move(event));
}

std::vector<std::string> ProjectParserBase::ParseDeviceInfo(
    Dic::Module::Global::ProjectExplorerInfo &info, const std::string &searchPath) {
    if (info.projectType == static_cast<int64_t>(ProjectTypeEnum::DB_CLUSTER) ||
        info.projectType == static_cast<int64_t>(ProjectTypeEnum::DB)) {
        std::vector<std::string> dbFiles = FileUtil::FindFilesWithFilter(searchPath, std::regex{"analysis.db"});
        if (dbFiles.empty()) {
            return {};
        }
        std::set<std::string> deviceIds = ProjectParserBase::ParseDeviceIdSetFromDb(dbFiles[0]);
        return std::vector<std::string>(deviceIds.begin(), deviceIds.end());
    }
    std::vector<std::string> stepTraceTimeCsvFiles =
        FileUtil::FindFirstFileByRegex(searchPath, std::regex("^step_trace_time.csv$"));
    if (stepTraceTimeCsvFiles.empty()) {
        return {};
    }
    std::string stepTraceTimeCsvFile = stepTraceTimeCsvFiles[0];
    auto deviceIdSet = ProjectParserBase::ParseDeviceIdSetFromCsv(stepTraceTimeCsvFile);
    return std::vector<std::string>(deviceIdSet.begin(), deviceIdSet.end());
}

std::set<std::string> ProjectParserBase::ParseDeviceIdSetFromCsv(const std::string &filePath) {
    auto file = OpenReadFileSafely(filePath);
    if (!file.is_open()) {
        ServerLog::Error("Failed to open step_trace_time.csv.");
        return {};
    }
    std::string line;
    uint64_t index = std::numeric_limits<uint64_t>::max();
    getline(file, line);
    auto headerRow = StringUtil::StringSplit(line);
    auto it = std::find(headerRow.begin(), headerRow.end(), DEVICE_ID);
    if (it == headerRow.end()) {
        ServerLog::Info("No DeviceId col in step_trace_time.csv.");
        return {};
    }
    index = static_cast<uint64_t>(std::distance(headerRow.begin(), it));
    std::set<std::string> deviceIds;
    while (getline(file, line)) {
        auto dataRow = StringUtil::StringSplit(line);
        if (dataRow.size() <= index) {
            continue;
        }
        if (!dataRow[index].empty()) {
            deviceIds.insert(dataRow[index]);
        }
    }
    return deviceIds;
}

std::set<std::string> ProjectParserBase::ParseDeviceIdSetFromDb(const std::string &dbPath) {
    std::recursive_mutex mutex;
    auto database = std::make_shared<DbTraceDataBase>(mutex);
    std::set<std::string> deviceId;
    try {
        database->OpenDb(dbPath, false);
        database->QueryDeviceIdInStepTraceTime(deviceId);
    } catch (std::exception &e) {
        ServerLog::Error("Failed to query deviceId from StepTraceTimeTable.");
        return deviceId;
    }
    return deviceId;
}
void ProjectParserBase::MergeFileTree(std::vector<std::shared_ptr<ParseFileInfo>> &rootTree,
    const std::vector<std::shared_ptr<ParseFileInfo>> &childrenTree) {
    if (rootTree.empty()) {
        rootTree = childrenTree;
        return;
    }
    if (childrenTree.empty()) {
        return;
    }
    if (rootTree[0]->subId == childrenTree[0]->subId) {
        rootTree[0]->subParseFile.insert(rootTree[0]->subParseFile.begin(), childrenTree[0]->subParseFile.begin(),
            childrenTree[0]->subParseFile.end());
    } else {
        rootTree.insert(rootTree.end(), childrenTree.begin(), childrenTree.end());
    }
}

void ProjectParserBase::SendImportActionRes(std::unique_ptr<ImportActionResponse> responsePtr) {
    std::sort(responsePtr->body.result.begin(), responsePtr->body.result.end(), [](const Action &a, const Action &b) {
        if (a.cardName.size() != b.cardName.size()) {
            return a.cardName.size() < b.cardName.size();
        }

        return a.cardName < b.cardName;
    });
    SendResponse(std::move(responsePtr), true);
}

ProjectAnalyzeRegister<ProjectParserBase> pReg(ParserType::OTHER);
}
