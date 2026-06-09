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

#ifndef PROFILER_SERVER_FILEMENUDATABASEDEF_H
#define PROFILER_SERVER_FILEMENUDATABASEDEF_H

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <stack>
#include <queue>
#include <cstdint>
#include <set>
#include "FileUtil.h"
#include "JsonUtil.h"
namespace Dic::Module::Global {
using namespace Dic;

enum ParseFileType : int64_t {
    PROJECT = 0, // 项目
    CLUSTER = 1, // 集群
    DATA_FILE = 2, // 大于DATA_FILE的都是实际的数据
    RANK = 3, // 卡
    COMPUTE = 4, // 算子bin文件
    IPYNB = 5, // jupyterlab文件
    DEVICE_CHIP = 6, //  区别于rank，DEVICE_CHIP是硬件上的概念
};

inline std::string CastParseFileTypeToStr(ParseFileType type) {
    switch (type) {
    case ParseFileType::PROJECT:
        return "PROJECT";
    case ParseFileType::CLUSTER:
        return "CLUSTER";
    case ParseFileType::DATA_FILE:
        return "DATA_FILE";
    case ParseFileType::DEVICE_CHIP:
    case ParseFileType::RANK:
        return "RANK";
    case ParseFileType::COMPUTE:
        return "COMPUTE";
    case ParseFileType::IPYNB:
        return "IPYNB";
    }
    return "";
}

struct ParseFileInfo {
    int64_t id{-1};
    int64_t projectExplorerId{-1};
    std::string clusterId;
    std::string parseFilePath;
    std::string fileId;
    std::string rankId;
    std::string deviceId;
    std::string host;
    std::string subId; // 用于下一级的parseFileInfo去寻找自己的上一级目录
    std::string curDirName; // 当前目录名
    ParseFileType type;
    int64_t projectType;
    std::vector<std::shared_ptr<ParseFileInfo>> subParseFile;
    inline json_t SerializeToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json_t res(rapidjson::kObjectType);
        JsonUtil::AddMember(res, "clusterPath", clusterId, allocator);
        JsonUtil::AddMember(res, "rankId", rankId, allocator);
        JsonUtil::AddMember(res, "host", host, allocator);
        JsonUtil::AddMember(res, "filePath", parseFilePath, allocator);
        JsonUtil::AddMember(res, "type", CastParseFileTypeToStr(type), allocator);
        JsonUtil::AddMember(res, "deviceId", deviceId, allocator);
        JsonUtil::AddMember(res, "fileDir", curDirName, allocator);
        JsonUtil::AddMember(res, "projectType", projectType, allocator);
        json_t children(rapidjson::kArrayType);
        std::for_each(subParseFile.begin(), subParseFile.end(), [&children, &allocator](const auto &fileInfo) {
            children.PushBack(fileInfo->SerializeToJson(allocator), allocator);
        });
        JsonUtil::AddMember(res, "children", children, allocator);
        return res;
    }

    std::vector<std::shared_ptr<ParseFileInfo>> GetChildren() {
        std::queue<std::shared_ptr<ParseFileInfo>> que;
        std::for_each(subParseFile.begin(), subParseFile.end(), [&que](const auto &item) { que.push(item); });
        std::vector<std::shared_ptr<ParseFileInfo>> res;
        while (!que.empty()) {
            auto item = que.front();
            que.pop();
            res.push_back(item);
            std::for_each(
                item->subParseFile.begin(), item->subParseFile.end(), [&que](const auto &it) { que.push(it); });
        }
        return res;
    }
};

struct ProjectExplorerInfo {
    int64_t id = -1;
    std::string projectName;
    std::string fileName;
    std::vector<std::shared_ptr<ParseFileInfo>> subParseFileInfo; // 项目下待解析的文件路径,不包含非数据文件类型
    std::vector<std::shared_ptr<ParseFileInfo>> projectFileTree; // 项目下文件树，包含非数据类型和数据文件类型
    // subId->ParseFileInfo导入单文件时subId会有重复
    std::unordered_multimap<std::string, std::shared_ptr<ParseFileInfo>> fileInfoMap;
    std::string importType;
    std::vector<std::string> dbPath;
    std::string accessTime;
    int64_t projectType;

    inline void AddSubParseFileInfo(
        const std::string &subId, ParseFileType type, const std::shared_ptr<ParseFileInfo> &fileInfo) {
        auto [begin, end] = fileInfoMap.equal_range(subId);
        for (; begin != end; begin++) {
            if (begin->second->type != type) {
                continue;
            }
            begin->second->subParseFile.emplace_back(fileInfo);
            fileInfoMap.emplace(fileInfo->subId, fileInfo);
            if (fileInfo->type > ParseFileType::DATA_FILE) {
                subParseFileInfo.push_back(fileInfo);
            }
            break;
        }
    }

    inline void AddSubParseFileInfo(const std::shared_ptr<ParseFileInfo> &fileInfo) {
        if (fileInfo->type == ParseFileType::PROJECT) {
            projectFileTree.emplace_back(fileInfo);
            fileInfoMap.emplace(fileInfo->subId, fileInfo);
            return;
        }
        // tree search
        auto curLevel = &projectFileTree;
        while (!curLevel->empty()) {
            if (curLevel->at(0)->type == fileInfo->type) {
                break;
            }
            bool flag = true;
            for (const auto &file : *curLevel) {
                if (IsSubFile(file, fileInfo)) { // contains
                    curLevel = &file->subParseFile;
                    flag = false;
                    break;
                }
            }
            if (flag) {
                break;
            }
        }
        curLevel->emplace_back(fileInfo);
        fileInfoMap.emplace(fileInfo->subId, fileInfo);
        if (fileInfo->type > ParseFileType::DATA_FILE) {
            subParseFileInfo.emplace_back(fileInfo);
        }
    }

    inline std::shared_ptr<ParseFileInfo> GetSubParseFileInfo(const std::string &subId, ParseFileType type) {
        auto [begin, end] = fileInfoMap.equal_range(subId);
        for (; begin != end; begin++) {
            if (begin->second->type == type) {
                return begin->second;
            }
        }
        return nullptr;
    }

    inline void MergeProjectExploreInfo(const ProjectExplorerInfo &projectInfo) {
        if (projectInfo.projectName != projectName) {
            return;
        }
        std::copy(projectInfo.subParseFileInfo.begin(), projectInfo.subParseFileInfo.end(),
            std::back_inserter(subParseFileInfo));
        std::copy(projectInfo.projectFileTree.begin(), projectInfo.projectFileTree.end(),
            std::back_inserter(projectFileTree));
        std::for_each(projectInfo.fileInfoMap.begin(), projectInfo.fileInfoMap.end(),
            [this](const auto &item) { fileInfoMap.emplace(item.first, item.second); });
    }

    std::vector<int64_t> GetFileIdsToDelete(std::shared_ptr<ParseFileInfo> fileInfo) {
        std::set<int64_t> deleteFileId{fileInfo->id};
        std::stack<std::shared_ptr<ParseFileInfo>> stack; // record the path
        // tree search
        auto curLevel = &projectFileTree;
        while (!curLevel->empty()) {
            bool flag = false;
            for (const auto &file : *curLevel) {
                if (fileInfo->subId == file->subId) {
                    DeleteFileChildren(file, deleteFileId);
                    flag = true;
                    break;
                }
                if (IsSubFile(file, fileInfo)) {
                    curLevel = &file->subParseFile;
                    stack.push(file); // record the path node
                }
            }
            if (flag) {
                break;
            }
        }
        while (!stack.empty()) {
            auto cur = stack.top();
            stack.pop();
            cur->subParseFile.erase(
                std::remove_if(cur->subParseFile.begin(), cur->subParseFile.end(),
                    [&deleteFileId](const auto &item) { return deleteFileId.count(item->id) != 0; }),
                cur->subParseFile.end());
            if (cur->subParseFile.empty()) {
                deleteFileId.insert(cur->id);
            }
        }
        return std::vector<int64_t>{deleteFileId.begin(), deleteFileId.end()};
    }

    static bool IsSubFile(std::shared_ptr<ParseFileInfo> parent, std::shared_ptr<ParseFileInfo> children) {
        if (parent->subId == children->subId) {
            return true;
        }
        return FileUtil::IsSubDir(parent->subId, children->subId);
    }

    void DeleteFile(std::shared_ptr<ParseFileInfo> file) {
        auto it = std::find(subParseFileInfo.begin(), subParseFileInfo.end(), file);
        if (it != subParseFileInfo.end()) {
            subParseFileInfo.erase(it);
        }
        auto it2 = std::find(projectFileTree.begin(), projectFileTree.end(), file);
        if (it2 != projectFileTree.end()) {
            projectFileTree.erase(it2);
        }
        auto [begin, end] = fileInfoMap.equal_range(file->subId);
        for (; begin != end; begin++) {
            if (begin->second->type == file->type) {
                fileInfoMap.erase(begin);
                return;
            }
        }
    }

    void DeleteFileChildren(std::shared_ptr<ParseFileInfo> file, std::set<int64_t> &deleteFileIds) {
        auto children = file->GetChildren();
        std::for_each(
            children.begin(), children.end(), [&deleteFileIds](const auto &item) { deleteFileIds.insert(item->id); });
    }

    inline std::vector<std::shared_ptr<ParseFileInfo>> GetClusterInfos() const {
        if (projectFileTree.empty()) {
            return {};
        }
        std::queue<std::shared_ptr<ParseFileInfo>> que;
        std::for_each(projectFileTree.begin(), projectFileTree.end(), [&que](const auto &item) { que.push(item); });
        std::vector<std::shared_ptr<ParseFileInfo>> res;
        while (!que.empty()) {
            auto fileInfo = que.front();
            que.pop();
            if (fileInfo->type < ParseFileType::CLUSTER) {
                std::for_each(fileInfo->subParseFile.begin(), fileInfo->subParseFile.end(),
                    [&que](const auto &item) { que.push(item); });
                continue;
            }
            if (fileInfo->type == ParseFileType::CLUSTER) {
                res.emplace_back(fileInfo);
                continue;
            }
        }
        return res;
    }

    std::vector<std::shared_ptr<ParseFileInfo>> GetDeviceInfos() {
        if (projectFileTree.empty()) {
            return {};
        }
        std::queue<std::shared_ptr<ParseFileInfo>> que;
        std::for_each(projectFileTree.begin(), projectFileTree.end(), [&que](const auto &item) { que.push(item); });
        std::vector<std::shared_ptr<ParseFileInfo>> res;
        while (!que.empty()) {
            auto fileInfo = que.front();
            que.pop();
            if (fileInfo->type < ParseFileType::DEVICE_CHIP) {
                std::for_each(fileInfo->subParseFile.begin(), fileInfo->subParseFile.end(),
                    [&que](const auto &item) { que.push(item); });
                continue;
            }
            if (fileInfo->type == ParseFileType::DEVICE_CHIP) {
                res.emplace_back(fileInfo);
                continue;
            }
        }
        return res;
    }
};

struct BaselineInfo {
    std::string host;
    std::string rankId;
    std::string cardName;
    std::string errorMessage;
    bool isCluster = false;
    bool isFtrace = false;
    std::string clusterBaseLine;
    std::string fileId;
    std::string parsedFilePath;
};
}
#endif // PROFILER_SERVER_FILEMENUDATABASEDEF_H
