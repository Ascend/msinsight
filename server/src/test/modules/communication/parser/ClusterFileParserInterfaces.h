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

#ifndef PROFILER_SERVER_CLUSTERFILEPARSERINTERFACES_H
#define PROFILER_SERVER_CLUSTERFILEPARSERINTERFACES_H

// 依赖注入接口定义

#pragma once

#include <string>
#include <vector>
#include <regex>
#include <memory>
#include <cstdint>

namespace Dic {
namespace Module {
namespace Timeline {

// 前向声明
enum class ParserStatus;
enum class DataType;

//=============================================================================
// 文件工具接口
//=============================================================================
class IFileUtil {
  public:
    virtual ~IFileUtil() = default;
    virtual bool IsFolder(const std::string &path) = 0;
    virtual std::string GetParentPath(const std::string &filePath) = 0;
    virtual std::vector<std::string> FindFilesWithFilter(const std::string &path, const std::regex &fileRegex) = 0;
};

//=============================================================================
// 解析状态管理器接口
//=============================================================================
class IParserStatusManager {
  public:
    virtual ~IParserStatusManager() = default;
    virtual void SetClusterParseStatus(const std::string &uniqueKey, ParserStatus status) = 0;
    virtual ParserStatus GetClusterParseStatus(const std::string &uniqueKey) = 0;
};

//=============================================================================
// 数据库接口（用于测试）- 独立接口，不继承实际类
//=============================================================================
class IDbClusterDataBase {
  public:
    virtual ~IDbClusterDataBase() = default;

    // Database 基类方法
    virtual bool IsDatabaseVersionChange() const = 0;
    virtual bool SetDataBaseVersion(const std::string &targetVersion = "") = 0;

    // DbClusterDataBase 方法
    virtual bool QueryExtremumTimestamp(uint64_t &min, uint64_t &max) = 0;
    virtual bool CreateTable() = 0;
    virtual bool DropTable() = 0;

    // 解析状态相关
    virtual bool HasFinishedParseLastTime() = 0;
    virtual bool UpdatesClusterParseStatus(const std::string &status) = 0;
    virtual void InsertClusterBaseInfo(ClusterBaseInfo &info) = 0;
};

//=============================================================================
// 数据库管理器接口
//=============================================================================
class IDataBaseManager {
  public:
    virtual ~IDataBaseManager() = default;
    virtual void CreateClusterConnectionPool(
        const std::string &projectPath, const std::string &dbPath, DataType type) = 0;
    virtual std::shared_ptr<IDbClusterDataBase> GetClusterDatabase(const std::string &uniqueKey) = 0;
};

//=============================================================================
// TraceTime 接口
//=============================================================================
class ITraceTime {
  public:
    virtual ~ITraceTime() = default;
    virtual void UpdateTime(uint64_t min, uint64_t max) = 0;
};

//=============================================================================
// 默认实现（包装原有静态方法/单例）
//=============================================================================

class FileUtilWrapper : public IFileUtil {
  public:
    bool IsFolder(const std::string &path) override { return FileUtil::IsFolder(path); }

    std::string GetParentPath(const std::string &filePath) override { return FileUtil::GetParentPath(filePath); }

    std::vector<std::string> FindFilesWithFilter(const std::string &path, const std::regex &fileRegex) override {
        return FileUtil::FindFilesWithFilter(path, fileRegex);
    }
};

class ParserStatusManagerWrapper : public IParserStatusManager {
  public:
    static ParserStatusManagerWrapper &Instance() {
        static ParserStatusManagerWrapper instance;
        return instance;
    }

    void SetClusterParseStatus(const std::string &uniqueKey, ParserStatus status) override {
        ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, status);
    }

    ParserStatus GetClusterParseStatus(const std::string &uniqueKey) override {
        return ParserStatusManager::Instance().GetParserStatus(uniqueKey);
    }
};

class DataBaseManagerWrapper : public IDataBaseManager {
  public:
    static DataBaseManagerWrapper &Instance() {
        static DataBaseManagerWrapper instance;
        return instance;
    }

    void CreateClusterConnectionPool(
        const std::string &projectPath, const std::string &dbPath, DataType type) override {
        DataBaseManager::Instance().CreateClusterConnectionPool(projectPath, dbPath, type);
    }

    std::shared_ptr<IDbClusterDataBase> GetClusterDatabase(const std::string &uniqueKey) override {
        // 实际代码中需要 dynamic_cast，这里假设返回的是 DbClusterDataBase
        return std::dynamic_pointer_cast<IDbClusterDataBase>(DataBaseManager::Instance().GetClusterDatabase(uniqueKey));
    }
};

class TraceTimeWrapper : public ITraceTime {
  public:
    static TraceTimeWrapper &Instance() {
        static TraceTimeWrapper instance;
        return instance;
    }

    void UpdateTime(uint64_t min, uint64_t max) override { Timeline::TraceTime::Instance().UpdateTime(min, max); }
};

} // namespace Timeline
} // namespace Module
} // namespace Dic

#endif //PROFILER_SERVER_CLUSTERFILEPARSERINTERFACES_H
