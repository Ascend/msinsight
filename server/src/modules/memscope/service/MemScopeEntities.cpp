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
#include "MemScopeEntities.h"
namespace Dic::Module::MemScope {
// [PYTHON_TRACE]
static uint32_t SMALL_RATIO = 10000;
static std::string DEFAULT_MERGED_SEP = " -> ";
bool MemScopePythonTrace::Empty() const { return slices.empty(); }

void MemScopePythonTrace::Trim(const PythonTrimCompressStrategy &strategy) {
    if (maxTimestamp <= minTimestamp) {
        Server::ServerLog::Warn("Failed to compress python trace, the minTimestamp is greater than maxTimestamp.");
        return;
    }
    bool needCompress = strategy == PythonTrimCompressStrategy::COMPRESS_SMALL_FUNCTIONS ||
        strategy == PythonTrimCompressStrategy::COMPRESS_AND_FILTER_SMALL_FUNCTIONS;
    bool needFilterOut = strategy == PythonTrimCompressStrategy::ONLY_FILTER_OUT_SMALL_FUNCTIONS ||
        strategy == PythonTrimCompressStrategy::COMPRESS_AND_FILTER_SMALL_FUNCTIONS;
    // 先筛选出small slices
    std::vector<PythonTraceSlice> smallSlices;
    auto new_end =
        std::remove_if(slices.begin(), slices.end(), [this, &smallSlices, needCompress](const PythonTraceSlice &slice) {
            if (IsSmallSlice(slice)) {
                // 如果需要压缩, 缓存到smallSlices数组
                if (needCompress) {
                    smallSlices.push_back(slice);
                }
                return true;
            }
            return false;
        });
    slices.erase(new_end, slices.end());
    if (needCompress) {
        DoCompress(smallSlices, needFilterOut);
    }
}

bool MemScopePythonTrace::IsSmallSlice(const PythonTraceSlice &slice) const {
    if (slice.endTimestamp < slice.startTimestamp) {
        return true;
    }

    uint64_t duration = slice.endTimestamp - slice.startTimestamp;
    return duration < (maxTimestamp - minTimestamp) / SMALL_RATIO;
}

void MemScopePythonTrace::DoCompress(
    std::vector<PythonTraceSlice> &waitForCompressSlices, const bool filterOutSmallFunc) {
    // 1. 按照开始时间进行排序
    std::sort(waitForCompressSlices.begin(), waitForCompressSlices.end(),
        [](const PythonTraceSlice &a, const PythonTraceSlice &b) { return a.startTimestamp < b.startTimestamp; });
    // 2. 按照深度分组
    std::vector<std::vector<PythonTraceSlice>> slicesByDepth(maxDepth + 1);
    for (const auto &slice : waitForCompressSlices) {
        if (slice.depth <= maxDepth) {
            slicesByDepth[slice.depth].push_back(slice);
        }
    }
    // 3. 分深度进行合并
    for (int depth = 0; depth <= maxDepth; ++depth) {
        if (slicesByDepth[depth].empty()) {
            continue;
        }
        DoCompressByDepth(slicesByDepth[depth], filterOutSmallFunc);
    }
}

void MemScopePythonTrace::DoCompressByDepth(std::vector<PythonTraceSlice> &depthSlices, const bool filterOutSmallFunc) {
    // 遍历当前深度的调用序列, 尝试合并
    for (size_t i = 0; i < depthSlices.size();) {
        size_t startIdx = i;
        int depth = depthSlices[i].depth;
        uint64_t minStart = depthSlices[i].startTimestamp;
        uint64_t maxEnd = depthSlices[i].endTimestamp;
        std::string mergedFuncName = StringUtil::StrJoin("Merged: ", depthSlices[i].func);
        // 尝试从i开始，尽可能多的合并后续调用
        while (i + 1 < depthSlices.size()) {
            uint64_t nextSliceStart = depthSlices[i + 1].startTimestamp;
            uint64_t gap = nextSliceStart - maxEnd;
            // 检查时间间隔(前序已校验max>min, 此处无回绕风险)
            if (gap < ((maxTimestamp - minTimestamp) / SMALL_RATIO)) {
                // 间隔很小 可以合并
                i++;
                mergedFuncName.append(StringUtil::StrJoin(DEFAULT_MERGED_SEP, depthSlices[i].func));
                maxEnd = std::max(maxEnd, depthSlices[i].endTimestamp);
                continue;
            }
            // 不满足间隔时间小，停止合并
            break;
        }
        // 从 startIdx 到 i (包含) 的调用可以合并为一个
        if (i > startIdx) {
            PythonTraceSlice merged(mergedFuncName, minStart, maxEnd, depth);
            slices.push_back(merged);
            i++; // 移动到下一组的开始
            continue;
        }
        // 无可合并内容, 根据策略决定是丢弃还是放回
        if (!filterOutSmallFunc) {
            slices.push_back(depthSlices[i]);
        }
        i++;
    }
}

// [PYTHON_TRACE]

// [EVENT]
void MemoryEventBaseAttrs::SetByJson(const json_t &json) {
    size = NumberUtil::StringToLongLong(JsonUtil::GetString(json, BLOCK_EVENT_ATTR_SIZE_FIELD));
    groupId = NumberUtil::StringToUnsignedLongLong(JsonUtil::GetString(json, BLOCK_EVENT_ATTR_GROUP_ID_FIELD));
}

void MallocFreeEventAttrs::SetByJson(const json_t &json) {
    MemoryEventBaseAttrs::SetByJson(json);
    total = NumberUtil::StringToUnsignedLongLong(JsonUtil::GetString(json, BLOCK_EVENT_ATTR_TOTAL_FIELD));
    used = NumberUtil::StringToUnsignedLongLong(JsonUtil::GetString(json, BLOCK_EVENT_ATTR_USED_FIELD));
    owner = JsonUtil::GetString(json, BLOCK_EVENT_ATTR_OWNER_FIELD);
}

void AccessEventAttrs::SetByJson(const json_t &json) {
    MemoryEventBaseAttrs::SetByJson(json);
    type = JsonUtil::GetString(json, ACCESS_EVENT_ATTR_TYPE);
    dtype = JsonUtil::GetString(json, ACCESS_EVENT_ATTR_DTYPE);
    shape = JsonUtil::GetString(json, ACCESS_EVENT_ATTR_SHAPE);
}

EventGroup::EventGroup(std::vector<MemScopeEvent> &sortedEvents) {
    for (auto &event : sortedEvents) {
        AddEvent(event);
    }
}

void EventGroup::AddEvent(const MemScopeEvent &event) {
    if (mallocEvent.has_value() && event.ptr != mallocEvent->ptr) {
        Server::ServerLog::Warn(
            "The event with empty ptr will be discarded: eventId=%, processId=%", event.id, event.processId);
        return;
    }

    if (event.event == MEM_SCOPE_DUMP_EVENT::MALLOC) {
        if (mallocEvent.has_value()) {
            Server::ServerLog::Warn("Duplicated malloc events in the same group.");
            return;
        }
        mallocEvent = std::make_optional(event);
        return;
    }
    if (event.event == MEM_SCOPE_DUMP_EVENT::FREE) {
        if (freeEvent.has_value()) {
            Server::ServerLog::Warn("Duplicated free events in the same group.");
            return;
        }
        freeEvent = std::make_optional(event);
        return;
    }
    if (event.event == MEM_SCOPE_DUMP_EVENT::ACCESS) {
        if (accessEvents.empty() || event.timestamp > accessEvents.back().timestamp) {
            accessEvents.push_back(event);
        }
    }
}
// [EVENT]

// [BLOCK]
std::string MemoryBlockAttrs::ToJsonString() {
    document_t blockAttrJson(kObjectType);
    auto &allocator = blockAttrJson.GetAllocator();
    JsonUtil::AddMember(blockAttrJson, BLOCK_EVENT_ATTR_GROUP_ID_FIELD, groupId, allocator);
    for (auto &attrPair : extendAttrs) {
        JsonUtil::AddMember(blockAttrJson, attrPair.first, attrPair.second, allocator);
    }
    return JsonUtil::JsonDump(blockAttrJson);
}

std::optional<MemoryBlockAttrs> MemoryBlockAttrs::FromJson(std::string jsonString) {
    MemoryBlockAttrs attrs;
    std::string error;
    auto attrsJsonDoc = JsonUtil::TryParse(jsonString, error);
    if (!error.empty() || !attrsJsonDoc.has_value()) {
        Server::ServerLog::Warn("Failed to parse block attrs: ", error);
        return std::nullopt;
    }
    auto &attrsJson = attrsJsonDoc.value();
    JsonUtil::SetByJsonKeyValue(attrs.groupId, attrsJson, "allocation_id");
    for (auto member = attrsJson.MemberBegin(); member != attrsJson.MemberEnd(); member++) {
        auto &key = member->name;
        auto &value = member->value;
        if (!key.IsString() || !value.IsString()) {
            Server::ServerLog::Warn("Attr json member key/value must be string.");
            continue;
        }
        std::string keyStr = key.GetString();
        if (keyStr.empty() || keyStr == BLOCK_EVENT_ATTR_GROUP_ID_FIELD) {
            continue;
        }
        attrs.extendAttrs[keyStr] = value.GetString();
    }
    return attrs;
}
// [BLOCK]
} // Dic::Module::MemScope
