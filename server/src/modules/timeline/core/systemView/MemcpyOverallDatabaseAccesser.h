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

#ifndef PROFILER_SERVER_MEMCPYOVERALLDATABASEACCESSER_H
#define PROFILER_SERVER_MEMCPYOVERALLDATABASEACCESSER_H
#include <memory>

#include "DataBaseManager.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {
struct MemcpyRecord {
    std::string threadId; // threadId 可能是字符串
    std::string threadName;
    std::string memcpyType;
    uint64_t size{};
    double duration;
    uint64_t startTime{};
    uint64_t endTime{};
};

struct MemcpyDetailRecord {
    uint64_t timestamp{};
    uint64_t duration{};
    uint64_t size{};
    std::string id;
    std::string name;
};

class MemcpyOverallDatabaseAccesser {
  public:
    explicit MemcpyOverallDatabaseAccesser(
        const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &fileId)
        : database_(database), fileId_(fileId) {}

    /**
         * @brief 获取指定时间范围内的Memcpy记录
         * @param startTime 起始时间戳（相对时间）
         * @param endTime 结束时间戳（相对时间）
         * @param records 输出的Memcpy记录列表
         * @return 是否成功获取数据。当 startTime == endTime 时，查找全部数据
         */
    bool GetMemcpyRecords(uint64_t startTime, uint64_t endTime, std::vector<MemcpyRecord> &records) const;

    /**
         * @brief 获取指定条件的Memcpy详细记录（用于详情列表展示）
         * @param startTime 起始时间戳（相对时间）
         * @param endTime 结束时间戳（相对时间）
         * @param tid 线程ID（字符串，空表示不过滤）
         * @param memcpyType 拷贝类型（如"H2D", "D2H"，无值表示不过滤）
         * @param current: 页码（从1开始）
         * @param pageSize: 每页大小
         * @param orderParam:
         *      orderParam.orderBy: 按什么值排序，可选：timestamp/duration/size
         *      orderParam.orderType: 排序方式 ASC/DESC
         * @param[out] records: 当前页的Memcpy详细记录列表
         * @param[out] total 总记录数
         * @return 是否成功获取数据。当 startTime == endTime 时，查找全部数据
         */
    bool GetMemcpyDetailRecordsPaged(uint64_t startTime, uint64_t endTime, const std::string &tid,
        const std::optional<std::string> &memcpyType, uint32_t current, uint32_t pageSize, const OrderParam &orderParam,
        std::vector<MemcpyDetailRecord> &records, uint64_t &total) const;

  private:
    // ====== 排序枚举（类作用域内，私有封装） ======
    enum class SortField { TIMESTAMP, SIZE, DURATION };
    enum class SortDirection { ASC, DESC };

    // ====== 内联辅助函数 ======
    [[nodiscard]] std::pair<SortField, SortDirection> ParseSortParams(
        const std::string &orderBy, const std::string &order) const {
        // 字段白名单校验
        SortField field = SortField::TIMESTAMP;
        if (orderBy == "size") {
            field = SortField::SIZE;
        } else if (orderBy == "duration") {
            field = SortField::DURATION;
        } else if (!orderBy.empty() && orderBy != "timestamp") {
            Server::ServerLog::Warn("Invalid orderBy field: " + orderBy + ", using default 'timestamp'");
        }

        // 方向校验
        SortDirection dir = (order == "DESC" || order == "desc") ? SortDirection::DESC : SortDirection::ASC;
        if (order != "ASC" && order != "asc" && order != "DESC" && order != "desc") {
            Server::ServerLog::Warn("Invalid order direction: " + order + ", using default 'ASC'");
        }
        return {field, dir};
    }

    // 内存排序
    void SortRecordsInMemory(std::vector<MemcpyDetailRecord> &records, SortField field, SortDirection dir) const {
        if (records.empty()) {
            return;
        }

        auto getComparator = [field, dir]() {
            switch (field) {
            case SortField::SIZE:
                return dir == SortDirection::ASC
                    ? [](const MemcpyDetailRecord &a, const MemcpyDetailRecord &b) { return a.size < b.size; }
                    : [](const MemcpyDetailRecord &a, const MemcpyDetailRecord &b) { return a.size > b.size; };
            case SortField::DURATION:
                return dir == SortDirection::ASC
                    ? [](const MemcpyDetailRecord &a, const MemcpyDetailRecord &b) { return a.duration < b.duration; }
                    : [](const MemcpyDetailRecord &a, const MemcpyDetailRecord &b) { return a.duration > b.duration; };
            case SortField::TIMESTAMP:
            default:
                return dir == SortDirection::ASC
                    ? [](const MemcpyDetailRecord &a, const MemcpyDetailRecord &b) { return a.timestamp < b.timestamp; }
                    : [](const MemcpyDetailRecord &a, const MemcpyDetailRecord &b) {
                          return a.timestamp > b.timestamp;
                      };
            }
        };

        std::sort(records.begin(), records.end(), getComparator());
    }

    /**
         * @brief 通过DataBaseManager获取当前数据库的数据类型
         * @return 数据库类型
         */
    DataType GetDatabaseType() const;

    /**
         * @brief 从Text数据库获取Memcpy记录
         * @param startTime 起始时间戳（绝对时间）
         * @param endTime 结束时间戳（绝对时间）
         * @param records 输出的Memcpy记录列表
         * @return 是否成功获取数据
         */
    bool GetMemcpyRecordsFromText(uint64_t startTime, uint64_t endTime, std::vector<MemcpyRecord> &records) const;

    /**
         * @brief 从Db数据库获取Memcpy记录
         * @param startTime 起始时间戳（绝对时间）
         * @param endTime 结束时间戳（绝对时间）
         * @param records 输出的Memcpy记录列表
         * @return 是否成功获取数据
         */
    bool GetMemcpyRecordsFromDb(uint64_t startTime, uint64_t endTime, std::vector<MemcpyRecord> &records) const;

    /**
         * @brief 从Text数据库获取Memcpy详细记录
         * @param startTime 起始时间戳（绝对时间）
         * @param endTime 结束时间戳（绝对时间）
         * @param tid 线程ID（字符串，空表示不过滤）
         * @param memcpyType 拷贝类型（如"H2D", "D2H"，无值表示不过滤）
         * @param current: 页码（从1开始）
         * @param pageSize: 每页大小（建议上限1000）
         * @param orderByField: 排序属性
         * @param orderDir: 排序方向
         * @param[out] records: 当前页的Memcpy详细记录列表
         * @param[out] total 总记录数
         * @return 是否成功获取数据
         */
    bool GetMemcpyDetailRecordsPagedFromText(uint64_t startTime, uint64_t endTime, const std::string &tid,
        const std::optional<std::string> &memcpyType, uint32_t current, uint32_t pageSize, SortField orderByField,
        SortDirection orderDir, std::vector<MemcpyDetailRecord> &records, uint64_t &total) const;

    /**
         * @brief 从Db数据库获取Memcpy详细记录
         * @param startTime 起始时间戳（绝对时间）
         * @param endTime 结束时间戳（绝对时间）
         * @param tid 线程ID（字符串，空表示不过滤）
         * @param memcpyType 拷贝类型（如"H2D", "D2H"，无值表示不过滤）
         * @param current: 页码（从1开始）
         * @param pageSize: 每页大小（建议上限1000）
         * @param orderSql: 排序语句
         * @return 是否成功获取数据
         */
    bool GetMemcpyDetailRecordsPagedFromDb(uint64_t startTime, uint64_t endTime, const std::string &tid,
        const std::optional<std::string> &memcpyType, uint32_t current, uint32_t pageSize, const std::string &orderSql,
        std::vector<MemcpyDetailRecord> &records, uint64_t &total) const;

    /**
         * @brief 从Db数据库获取Memcpy详细记录的总数
         * @param baseQuery 基础查询
         * @param baseParams 基础查询参数
         * @param[out] total 记录的总数
         */
    void GetMemcpyDetailTotalFromDb(
        const std::string &baseQuery, const std::vector<std::string> &baseParams, uint64_t &total) const;
    // ====== CTE分页核心：基础查询构建器（私有辅助函数） ======
    /**
         * @brief 构建DB数据库基础查询（用于CTE分页）
         * @param startTime 起始时间（绝对时间，ns）
         * @param endTime   结束时间（绝对时间，ns）
         * @param tidFilter TID过滤条件（空字符串表示不过滤）
         * @param memcpyTypeFilter memcpy操作类型过滤（如"H2D", "D2H"，无值表示不过滤）
         * @return pair.first: 完整WHERE条件的SELECT语句（不含ORDER/LIMIT）
         *         pair.second: 与SQL占位符顺序严格对应的参数列表
         * @note 返回的SQL可直接嵌入 WITH filtered AS (...) 使用
         * @warning 参数顺序必须与SQL中?占位符顺序完全一致！
         */
    [[nodiscard]] static std::pair<std::string, std::vector<std::string>> BuildMemcpyDetailBaseQueryDb(
        uint64_t startTime, uint64_t endTime, const std::string &tidFilter,
        const std::optional<std::string> &memcpyTypeFilter);

    /**
         * @brief 构建TEXT数据库基础查询（用于CTE分页，依赖JSON1扩展）
         * @note 与BuildMemcpyBaseQueryDb接口基本对称。缺少 memcpyType，因为无法在 SQL 侧过滤，TEXT库专用
         * @see BuildMemcpyDetailBaseQueryDb 参数说明
         */
    [[nodiscard]] static std::pair<std::string, std::vector<std::string>> BuildMemcpyDetailBaseQueryText(
        uint64_t startTime, uint64_t endTime, const std::string &tidFilter);

    /**
         * @brief 解析JSON字符串获取operation,size字段
         * @param jsonStr JSON字符串
         * @return operation字段值, size:单位B
         */
    static std::pair<std::string, uint64_t> ParseOperationAndSizeFromJson(const std::string &jsonStr);

    /**
         * @brief 安全加法辅助函数
         * @param a 加数
         * @param b 加数
         * @param result 结果
         * @return 溢出返回 false，正常加法返回 true
         */
    static bool SafeAddUint64(uint64_t a, uint64_t b, uint64_t &result) {
        if (a > std::numeric_limits<uint64_t>::max() - b) {
            return false; // 溢出
        }
        result = a + b;
        return true;
    }

    std::shared_ptr<VirtualTraceDatabase> database_;
    std::string fileId_;
};
}

#endif //PROFILER_SERVER_MEMCPYOVERALLDATABASEACCESSER_H
