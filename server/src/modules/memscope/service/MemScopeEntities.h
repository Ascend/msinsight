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

#ifndef PROFILER_SERVER_MEM_SCOPE_ENTITIES_H
#define PROFILER_SERVER_MEM_SCOPE_ENTITIES_H

#include "pch.h"
#include "MemScopeDefs.h"

namespace Dic::Module::MemScope {
// [PYTHON_TRACE] 内存调用栈火焰图相关实体
struct PythonTraceSlice {
    int64_t id{0};
    std::string func{"UNKNOWN"};
    uint64_t startTimestamp{0};
    uint64_t endTimestamp{0};
    int depth{-1};
    uint64_t processId{0};
    uint64_t threadId{0};

    PythonTraceSlice() = default;
    PythonTraceSlice(std::string func, uint64_t startTimestamp, uint64_t endTimestamp, int depth)
        : func(std::move(func)), startTimestamp(startTimestamp), endTimestamp(endTimestamp), depth(depth) {}
};
/***
 * 调用栈消减策略:
 * FILTER_OUT_SMALL_FUNCTIONS: 过滤掉小函数(基于duration比例)
 * COMPRESS_SMALL_FUNCTIONS: 压缩同一层级的小函数(同层级小函数 且 间隔较短，则合并成同一个块)
 * COMPRESS_AND_FILTER: 两个策略同时应用
 */
enum class PythonTrimCompressStrategy {
    ONLY_FILTER_OUT_SMALL_FUNCTIONS,
    COMPRESS_SMALL_FUNCTIONS,
    COMPRESS_AND_FILTER_SMALL_FUNCTIONS
};

struct MemScopePythonTrace {
    uint64_t maxTimestamp{};
    uint64_t minTimestamp{INT64_MAX};
    uint64_t threadId{};
    std::vector<PythonTraceSlice> slices;
    int maxDepth{};

    [[nodiscard]] bool Empty() const;

    /***
     * 基于策略进行压缩
     * @param strategy 压缩策略
     */
    void Trim(const PythonTrimCompressStrategy &strategy);

  private:
    [[nodiscard]] bool IsSmallSlice(const PythonTraceSlice &slice) const;
    void DoCompress(std::vector<PythonTraceSlice> &waitForCompressSlices, const bool filterOutSmallFunc);
    void DoCompressByDepth(std::vector<PythonTraceSlice> &depthSlices, const bool filterOutSmallFunc);
};
// [PYTHON_TRACE]

// [EVENT] 事件/内存块扩展属性
inline const std::string BLOCK_EVENT_ATTR_SIZE_FIELD = "size";
inline const std::string BLOCK_EVENT_ATTR_OWNER_FIELD = "owner";
inline const std::string BLOCK_EVENT_ATTR_TOTAL_FIELD = "total";
inline const std::string BLOCK_EVENT_ATTR_USED_FIELD = "used";
inline const std::string BLOCK_EVENT_ATTR_GROUP_ID_FIELD = "allocation_id";
inline const std::string ACCESS_EVENT_ATTR_TYPE = "type";
inline const std::string ACCESS_EVENT_ATTR_DTYPE = "dtype";
inline const std::string ACCESS_EVENT_ATTR_SHAPE = "shape";
struct MemoryEventBaseAttrs {
    virtual ~MemoryEventBaseAttrs() = default;
    int64_t size{0}; // 对应内存事件涉及的内存大小, 如申请、释放大小；访问时tensor大小
    uint64_t groupId{0};

    virtual void SetByJson(const json_t &json);
};

struct MallocFreeEventAttrs : public MemoryEventBaseAttrs {
    uint64_t total{0};
    uint64_t used{0};
    std::string owner;

    void SetByJson(const json_t &json) override;
};

struct AccessEventAttrs : public MemoryEventBaseAttrs {
    std::string type;
    std::string dtype;
    std::string shape;

    void SetByJson(const json_t &json) override;
};

/***
 * 从json字符串构建EventAttrs
 * @tparam T eventAttr的派生类，包括MallocFreeEventAttr和AccessEventAttr或基类BaseAttr
 * @param jsonString
 * @return
 */
template <typename T> std::optional<T> BuildEventAttrsFromJson(const std::string &jsonString) {
    static_assert(std::is_base_of_v<MemoryEventBaseAttrs, T>,
        "T must be derived from MemoryEventBaseAttrs or be MemoryEventBaseAttrs itself");
    if (jsonString.empty()) {
        Server::ServerLog::Warn("Invalid json string: empty string.");
        return std::nullopt;
    }
    std::string parseError;
    auto jsonDoc = JsonUtil::TryParse(jsonString, parseError);
    if (!parseError.empty()) {
        Server::ServerLog::Warn("Parse json string to event attrs failed: ", parseError);
        return std::nullopt;
    }
    if (!jsonDoc.has_value()) {
        Server::ServerLog::Warn("Parse json string to event attrs failed: empty json object");
        return std::nullopt;
    }
    auto eventAttrs = std::make_optional<T>();
    auto &json = jsonDoc.value();
    eventAttrs->SetByJson(json);
    return eventAttrs;
}

struct MemoryBlockAttrs {
    uint64_t groupId{0}; // 组id
    std::map<std::string, std::string> extendAttrs;

    /***
     * 将内存块attr转化为json
     * @return
     */
    std::string ToJsonString();

    /***
     * 从json字符串构建BlockAttrs
     * @param jsonString
     * @return
     */
    static std::optional<MemoryBlockAttrs> FromJson(std::string jsonString);
};
/***
 * 内存事件组
 */
struct EventGroup {
    std::optional<MemScopeEvent> mallocEvent{std::nullopt};
    std::optional<MemScopeEvent> freeEvent{std::nullopt};
    std::vector<MemScopeEvent> accessEvents;
    int64_t groupId{-1};

    EventGroup() = default;
    /***
     * 通过events快速构建一个eventGroup, 注意需要events已根据timestamp升序排序
     * @param sortedEvents 事件数组，要求已经过排序
     */
    explicit EventGroup(std::vector<MemScopeEvent> &sortedEvents);

    void AddEvent(const MemScopeEvent &event);
};
// [EVENT]
} // Dic::Module::MemScope

#endif // PROFILER_SERVER_MEM_SCOPE_ENTITIES_H
