/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include "MsprofFtraceBuilder.h"
#include "MsprofFtraceFormat.h"
#include <array>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>
#include <vector>
#include "document.h"
#include "stringbuffer.h"
#include "writer.h"

namespace Dic::Module::FullDb {
namespace {
using Json = rapidjson::Document;
constexpr uint64_t MAX_INTERRUPT_DURATION_NS = 1000000000;
constexpr const char *CPU_PROCESS = "CPU Scheduling";
constexpr const char *TASK_PROCESS = "Process Scheduling";

std::string CpuThreadName(uint32_t cpu) {
    auto number = std::to_string(cpu);
    constexpr size_t MIN_CPU_DIGITS = 3;
    if (number.size() < MIN_CPU_DIGITS) {
        number.insert(0, MIN_CPU_DIGITS - number.size(), '0');
    }
    return "CPU " + number;
}

void Put(Json &json, const std::string &name, const std::string &value) {
    auto &allocator = json.GetAllocator();
    json.RemoveMember(name.c_str());
    rapidjson::Value key(name.c_str(), static_cast<rapidjson::SizeType>(name.size()), allocator);
    rapidjson::Value item(value.c_str(), static_cast<rapidjson::SizeType>(value.size()), allocator);
    json.AddMember(key, item, allocator);
}

void PutBool(Json &json, const std::string &name, bool value) {
    auto &allocator = json.GetAllocator();
    json.RemoveMember(name.c_str());
    rapidjson::Value key(name.c_str(), static_cast<rapidjson::SizeType>(name.size()), allocator);
    json.AddMember(key, value, allocator);
}

std::string ValueText(const MsprofFtraceValue &value) {
    if (const auto *text = std::get_if<std::string>(&value)) {
        return *text;
    }
    if (const auto *number = std::get_if<int64_t>(&value)) {
        return std::to_string(*number);
    }
    if (const auto *number = std::get_if<uint64_t>(&value)) {
        return std::to_string(*number);
    }
    if (const auto *number = std::get_if<double>(&value)) {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::setprecision(std::numeric_limits<double>::max_digits10) << *number;
        return text.str();
    }
    return "";
}

std::optional<uint64_t> UnsignedField(const MsprofFtraceEvent &event, const std::string &name) {
    const auto found = event.fields.find(name);
    if (found == event.fields.end()) {
        return std::nullopt;
    }
    if (const auto *number = std::get_if<uint64_t>(&found->second)) {
        return *number;
    }
    if (const auto *number = std::get_if<int64_t>(&found->second); number != nullptr && *number >= 0) {
        return static_cast<uint64_t>(*number);
    }
    return std::nullopt;
}

std::optional<uint32_t> TidField(const MsprofFtraceEvent &event, const std::string &name) {
    const auto number = UnsignedField(event, name);
    if (!number.has_value() || *number > std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(*number);
}

std::string StringField(const MsprofFtraceEvent &event, const std::string &name) {
    const auto found = event.fields.find(name);
    if (found == event.fields.end()) {
        return "";
    }
    const auto *text = std::get_if<std::string>(&found->second);
    return text == nullptr ? "" : *text;
}

std::string Serialize(const Json &json) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

Json EventArgs(const MsprofFtraceEvent &event) {
    Json json(rapidjson::kObjectType);
    auto &allocator = json.GetAllocator();
    rapidjson::Value fields(rapidjson::kObjectType);
    for (const auto &field : event.fields) {
        rapidjson::Value name(field.first.c_str(), static_cast<rapidjson::SizeType>(field.first.size()), allocator);
        rapidjson::Value value;
        if (std::holds_alternative<std::monostate>(field.second)) {
            value.SetNull();
        } else if (const auto *floating = std::get_if<double>(&field.second)) {
            value.SetDouble(*floating);
            Put(json, field.first, ValueText(field.second));
        } else {
            const auto text = ValueText(field.second);
            value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
            Put(json, field.first, text);
        }
        fields.AddMember(name, value, allocator);
    }
    json.RemoveMember("fields");
    json.AddMember("fields", fields, allocator);
    Put(json, "source", "msprof_ftrace");
    Put(json, "eventName", event.name);
    Put(json, "genericEventId", std::to_string(event.eventId));
    Put(json, "sourceId", std::to_string(event.sourceId));
    Put(json, "typeId", std::to_string(event.typeId));
    Put(json, "timestampNs", std::to_string(event.timestampNs));
    Put(json, "rawTimestamp", std::to_string(event.rawTimestamp));
    Put(json, "flags", std::to_string(event.flags));
    if (event.cpu.has_value()) {
        Put(json, "cpu", std::to_string(*event.cpu));
    } else {
        json.RemoveMember("cpu");
    }
    if (event.globalTid.has_value()) {
        Put(json, "globalTid", std::to_string(*event.globalTid));
    }
    return json;
}

enum class TaskPhase { UNKNOWN, RUNNING, RUNNABLE, SLEEPING };

struct DecodedState {
    std::string code = "Unknown";
    TaskPhase phase = TaskPhase::UNKNOWN;
    std::optional<bool> preempted;
    bool exited = false;
};

DecodedState DecodeState(const MsprofFtraceEvent &event, std::optional<uint64_t> mask) {
    DecodedState result;
    const auto raw = UnsignedField(event, "prev_state");
    if (!raw.has_value()) {
        return result;
    }
    uint64_t base = *raw;
    if (mask.has_value()) {
        result.preempted = (base & *mask) != 0;
        base &= *mask - 1;
    }
    if (base == 0) {
        result.code = "R";
        result.phase = TaskPhase::RUNNABLE;
    } else if ((base & ~uint64_t{255}) == 0) {
        const std::array<std::pair<uint64_t, const char *>, 8> flags = {
            {{1, "S"}, {2, "D"}, {4, "T"}, {8, "t"}, {16, "X"}, {32, "Z"}, {64, "P"}, {128, "I"}}};
        result.code.clear();
        for (const auto &flag : flags) {
            if ((base & flag.first) != 0) {
                if (!result.code.empty()) {
                    result.code += "|";
                }
                result.code += flag.second;
            }
        }
        result.exited = (base & (16 | 32)) != 0;
        if ((base & (1 | 2)) != 0 && !result.exited && (base & (4 | 8 | 64 | 128)) == 0) {
            result.phase = TaskPhase::SLEEPING;
        }
    }
    if (result.preempted.value_or(false)) {
        result.code += "+";
        result.phase = TaskPhase::RUNNABLE;
    }
    return result;
}

std::string PhaseName(TaskPhase phase) {
    switch (phase) {
    case TaskPhase::RUNNING:
        return "Running";
    case TaskPhase::RUNNABLE:
        return "Runnable";
    case TaskPhase::SLEEPING:
        return "Sleeping";
    default:
        return "Unknown";
    }
}

std::string SoftirqAction(uint64_t vector) {
    static const std::array<const char *, 10> actions = {
        "HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "IRQ_POLL", "TASKLET", "SCHED", "HRTIMER", "RCU"};
    return vector < actions.size() ? actions[vector] : "vec " + std::to_string(vector);
}
}

class MsprofFtraceBuilder::Impl {
  public:
    struct Task {
        uint32_t tid = 0;
        std::string key;
        std::string comm;
        std::string stateComm;
        std::optional<uint32_t> tgid;
        TaskPhase phase = TaskPhase::UNKNOWN;
        uint64_t start = 0;
        std::optional<uint32_t> cpu;
        bool exiting = false;
        uint64_t qualityEpoch = 0;
        MsprofFtraceEvent startEvent;
    };
    struct CpuRunning {
        std::string taskKey;
        std::string comm;
        uint32_t tid = 0;
        uint64_t start = 0;
        uint64_t qualityEpoch = 0;
        MsprofFtraceEvent startEvent;
    };
    struct PendingInterrupt {
        std::string kind;
        uint64_t vector = 0;
        std::string action;
        std::optional<uint32_t> tid;
        std::string taskKey;
        std::string comm;
        MsprofFtraceEvent entry;
    };
    struct InterruptStack {
        uint64_t qualityEpoch = 0;
        std::vector<PendingInterrupt> entries;
    };

    Impl(MsprofFtraceSink &target, const std::map<uint32_t, uint32_t> &tgids) : sink(target), knownTgids(tgids) {}
    bool Consume(const MsprofFtraceEvent &event, std::string &error);
    bool Finish(std::string &error);
    Task &GetTask(uint32_t tid, const std::string &comm, uint32_t cpu, const MsprofFtraceEvent &event);
    void ClearTaskState(Task &task);
    void RefreshTask(Task &task);
    std::map<uint32_t, CpuRunning>::iterator CurrentRunning(uint32_t cpu);
    std::vector<PendingInterrupt> &CurrentInterrupts(uint32_t cpu);
    std::optional<uint64_t> StateMask(const std::shared_ptr<const MsprofFtraceType> &type);
    void AddTaskArgs(Json &args, const Task &task, const std::string &comm = "") const;
    bool Emit(MsprofFtraceSlice slice, std::string &error);
    bool CloseTask(Task &task, const MsprofFtraceEvent &closing, std::string &error);
    bool Switch(const MsprofFtraceEvent &event, std::string &error);
    bool Wakeup(const MsprofFtraceEvent &event, std::string &error);
    void Exit(const MsprofFtraceEvent &event);
    bool Interrupt(const MsprofFtraceEvent &event, std::string &error);

    MsprofFtraceSink &sink;
    bool finished = false;
    std::map<uint32_t, uint32_t> knownTgids;
    std::map<std::string, uint64_t> epochs;
    std::map<std::string, std::string> currentTasks;
    std::map<std::string, Task> tasks;
    std::map<uint32_t, CpuRunning> cpuRunning;
    std::map<uint32_t, InterruptStack> interrupts;
    uint64_t qualityEpoch = 1;
    std::map<std::shared_ptr<const MsprofFtraceType>, std::optional<uint64_t>,
        std::owner_less<std::shared_ptr<const MsprofFtraceType>>>
        stateMasks;
    std::optional<std::pair<uint64_t, uint64_t>> previousOrder;
    MsprofFtraceBuildSummary summary;
};

void MsprofFtraceBuilder::Impl::ClearTaskState(Task &task) {
    task.phase = TaskPhase::UNKNOWN;
    task.start = 0;
    task.cpu.reset();
    task.stateComm.clear();
    task.startEvent = {};
}

void MsprofFtraceBuilder::Impl::RefreshTask(Task &task) {
    if (task.qualityEpoch == qualityEpoch) {
        return;
    }
    // A quality gap is not a new task lifetime: retain identity, name, TGID and exit facts.
    task.qualityEpoch = qualityEpoch;
    ClearTaskState(task);
}

std::map<uint32_t, MsprofFtraceBuilder::Impl::CpuRunning>::iterator MsprofFtraceBuilder::Impl::CurrentRunning(
    uint32_t cpu) {
    const auto found = cpuRunning.find(cpu);
    if (found != cpuRunning.end() && found->second.qualityEpoch != qualityEpoch) {
        cpuRunning.erase(found);
        return cpuRunning.end();
    }
    return found;
}

std::vector<MsprofFtraceBuilder::Impl::PendingInterrupt> &MsprofFtraceBuilder::Impl::CurrentInterrupts(uint32_t cpu) {
    auto &stack = interrupts[cpu];
    if (stack.qualityEpoch != qualityEpoch) {
        stack.entries.clear();
        stack.qualityEpoch = qualityEpoch;
    }
    return stack.entries;
}

std::optional<uint64_t> MsprofFtraceBuilder::Impl::StateMask(const std::shared_ptr<const MsprofFtraceType> &type) {
    if (type == nullptr) {
        return std::nullopt;
    }
    const auto found = stateMasks.find(type);
    if (found != stateMasks.end()) {
        return found->second;
    }
    const auto mask = FtraceFormat::PreemptMask(type->format);
    stateMasks.emplace(type, mask);
    return mask;
}

MsprofFtraceBuilder::Impl::Task &MsprofFtraceBuilder::Impl::GetTask(
    uint32_t tid, const std::string &comm, uint32_t cpu, const MsprofFtraceEvent &event) {
    const std::string identity = tid == 0 ? "idle:" + std::to_string(cpu) : "tid:" + std::to_string(tid);
    auto current = currentTasks.find(identity);
    if (current == currentTasks.end()) {
        Task task;
        task.tid = tid;
        task.key = identity + ":" + std::to_string(++epochs[identity]);
        task.comm = comm.empty() ? (tid == 0 ? "swapper" : "Task " + std::to_string(tid)) : comm;
        const auto tgid = knownTgids.find(tid);
        if (tgid != knownTgids.end()) {
            task.tgid = tgid->second;
        }
        const auto key = task.key;
        tasks.emplace(key, std::move(task));
        current = currentTasks.emplace(identity, key).first;
    }
    Task &task = tasks.at(current->second);
    RefreshTask(task);
    if (!comm.empty()) {
        task.comm = comm;
    }
    const auto commonTid = TidField(event, "common_pid");
    if (!task.tgid.has_value() && commonTid == tid && event.globalTid.has_value() &&
        static_cast<uint32_t>(*event.globalTid) == tid) {
        const uint32_t tgid = static_cast<uint32_t>(*event.globalTid >> 32);
        if (tgid != 0 && tgid != tid) {
            task.tgid = tgid;
        }
    }
    return task;
}

void MsprofFtraceBuilder::Impl::AddTaskArgs(Json &args, const Task &task, const std::string &comm) const {
    Put(args, "tid", std::to_string(task.tid));
    Put(args, "comm", comm.empty() ? task.comm : comm);
    Put(args, "taskKey", task.key);
    if (task.tgid.has_value()) {
        Put(args, "tgid", std::to_string(*task.tgid));
    }
}

bool MsprofFtraceBuilder::Impl::Emit(MsprofFtraceSlice slice, std::string &error) {
    if (!sink.AddSlice(slice, error)) {
        return false;
    }
    ++summary.sliceCount;
    return true;
}

bool MsprofFtraceBuilder::Impl::CloseTask(Task &task, const MsprofFtraceEvent &closing, std::string &error) {
    RefreshTask(task);
    if (task.phase == TaskPhase::UNKNOWN || closing.timestampNs <= task.start) {
        return true;
    }
    MsprofFtraceSlice slice;
    slice.process = TASK_PROCESS;
    slice.threadKey = task.key;
    slice.threadName = task.stateComm + ":" + std::to_string(task.tid);
    slice.name = PhaseName(task.phase);
    slice.timestampNs = task.start;
    slice.durationNs = closing.timestampNs - task.start;
    auto args = EventArgs(task.startEvent);
    AddTaskArgs(args, task, task.stateComm);
    if (task.cpu.has_value()) {
        Put(args, "cpu", std::to_string(*task.cpu));
    } else {
        args.RemoveMember("cpu");
    }
    Put(args, "endGenericEventId", std::to_string(closing.eventId));
    slice.args = Serialize(args);
    return Emit(std::move(slice), error);
}

bool MsprofFtraceBuilder::Impl::Switch(const MsprofFtraceEvent &event, std::string &error) {
    const auto previousTid = TidField(event, "prev_pid");
    const auto nextTid = TidField(event, "next_pid");
    const auto state = event.fields.find("prev_state");
    if (!previousTid.has_value() || !nextTid.has_value() || state == event.fields.end()) {
        return true;
    }
    const uint32_t cpu = *event.cpu;
    Task &previous = GetTask(*previousTid, StringField(event, "prev_comm"), cpu, event);
    const auto decoded = DecodeState(event, StateMask(event.type));
    auto switchArgs = EventArgs(event);
    AddTaskArgs(switchArgs, previous);
    Put(switchArgs, "prev_state", decoded.code);
    Put(switchArgs, "displayCode", decoded.code);
    Put(switchArgs, "baseState",
        decoded.code.back() == '+' ? decoded.code.substr(0, decoded.code.size() - 1) : decoded.code);
    Put(switchArgs, "rawPrevState", ValueText(state->second));
    if (decoded.preempted.has_value()) {
        PutBool(switchArgs, "preempted", *decoded.preempted);
    }
    MsprofFtraceSlice instant;
    instant.process = CPU_PROCESS;
    instant.threadKey = "cpu:" + std::to_string(cpu);
    instant.threadName = CpuThreadName(cpu);
    instant.name = "sched_switch";
    instant.args = Serialize(switchArgs);
    instant.timestampNs = event.timestampNs;
    if (!Emit(std::move(instant), error)) {
        return false;
    }
    summary.hasScheduling = true;

    const auto running = CurrentRunning(cpu);
    bool matchesRunning = false;
    if (running != cpuRunning.end()) {
        matchesRunning = running->second.tid == *previousTid && running->second.taskKey == previous.key &&
            previous.phase == TaskPhase::RUNNING && previous.cpu == event.cpu &&
            previous.start == running->second.start &&
            previous.startEvent.eventId == running->second.startEvent.eventId;
        const auto pending = std::move(running->second);
        cpuRunning.erase(running);
        if (!matchesRunning) {
            // This switch breaks the old CPU/task pairing. Discard only that open state,
            // not a task that has since migrated or started another lifetime.
            const auto displaced = tasks.find(pending.taskKey);
            if (displaced != tasks.end() && displaced->second.phase == TaskPhase::RUNNING &&
                displaced->second.cpu == event.cpu && displaced->second.start == pending.start &&
                displaced->second.startEvent.eventId == pending.startEvent.eventId) {
                ClearTaskState(displaced->second);
            }
        }
        if (matchesRunning && pending.start < event.timestampNs) {
            MsprofFtraceSlice slice;
            slice.process = CPU_PROCESS;
            slice.threadKey = "cpu:" + std::to_string(cpu);
            slice.threadName = CpuThreadName(cpu);
            slice.name = pending.comm + ":" + std::to_string(pending.tid);
            slice.timestampNs = pending.start;
            slice.durationNs = event.timestampNs - pending.start;
            auto args = EventArgs(pending.startEvent);
            Put(args, "tid", std::to_string(pending.tid));
            Put(args, "comm", pending.comm);
            Put(args, "taskKey", pending.taskKey);
            Put(args, "endGenericEventId", std::to_string(event.eventId));
            Put(args, "end_state", decoded.code);
            slice.args = Serialize(args);
            if (!Emit(std::move(slice), error)) {
                return false;
            }
        }
    }
    if (matchesRunning && !CloseTask(previous, event, error)) {
        return false;
    }
    previous.phase = previous.exiting || decoded.exited ? TaskPhase::UNKNOWN : decoded.phase;
    previous.start = event.timestampNs;
    previous.stateComm = previous.comm;
    previous.cpu = cpu;
    previous.startEvent = event;
    const bool retirePrevious = previous.exiting || decoded.exited;
    const std::string previousKey = previous.key;
    if (retirePrevious) {
        const std::string identity =
            previous.tid == 0 ? "idle:" + std::to_string(cpu) : "tid:" + std::to_string(previous.tid);
        currentTasks.erase(identity);
    }

    Task &next = GetTask(*nextTid, StringField(event, "next_comm"), cpu, event);
    if (next.phase == TaskPhase::RUNNABLE && !CloseTask(next, event, error)) {
        return false;
    }
    // A missing wakeup leaves the Sleeping/Runnable boundary unknown; do not count all of it as sleep.
    if (next.phase == TaskPhase::RUNNING && next.cpu.has_value() && next.cpu != event.cpu) {
        auto oldCpu = CurrentRunning(*next.cpu);
        if (oldCpu != cpuRunning.end() && oldCpu->second.taskKey == next.key) {
            cpuRunning.erase(oldCpu);
        }
    }
    next.phase = TaskPhase::RUNNING;
    next.start = event.timestampNs;
    next.stateComm = next.comm;
    next.cpu = cpu;
    next.startEvent = event;
    CpuRunning opened;
    opened.taskKey = next.key;
    opened.comm = next.comm;
    opened.tid = next.tid;
    opened.start = event.timestampNs;
    opened.qualityEpoch = qualityEpoch;
    opened.startEvent = event;
    cpuRunning[cpu] = std::move(opened);
    if (retirePrevious) {
        tasks.erase(previousKey);
    }
    return true;
}

bool MsprofFtraceBuilder::Impl::Wakeup(const MsprofFtraceEvent &event, std::string &error) {
    const auto tid = TidField(event, "pid");
    if (!tid.has_value()) {
        return true;
    }
    const auto success = UnsignedField(event, "success");
    if (success.has_value() && *success == 0) {
        return true;
    }
    Task &task = GetTask(*tid, StringField(event, "comm"), *event.cpu, event);
    summary.hasWakeup = true;
    if (task.exiting || task.phase == TaskPhase::RUNNING || task.phase == TaskPhase::RUNNABLE) {
        return true;
    }
    if (task.phase == TaskPhase::SLEEPING && !CloseTask(task, event, error)) {
        return false;
    }
    task.phase = TaskPhase::RUNNABLE;
    task.start = event.timestampNs;
    task.stateComm = task.comm;
    task.cpu = TidField(event, "target_cpu");
    task.startEvent = event;
    return true;
}

void MsprofFtraceBuilder::Impl::Exit(const MsprofFtraceEvent &event) {
    auto tid = TidField(event, "pid");
    if (!tid.has_value()) {
        tid = TidField(event, "common_pid");
    }
    if (!tid.has_value()) {
        return;
    }
    Task &task = GetTask(*tid, StringField(event, "comm"), *event.cpu, event);
    task.exiting = true;
    // The final sched_switch still closes this lifetime's CPU/task Running interval.
}

bool MsprofFtraceBuilder::Impl::Interrupt(const MsprofFtraceEvent &event, std::string &error) {
    const bool soft = event.name == "irq:softirq_entry" || event.name == "irq:softirq_exit";
    const bool entry = event.name == "irq:softirq_entry" || event.name == "irq:irq_handler_entry";
    const auto vector = UnsignedField(event, soft ? "vec" : "irq");
    if (!vector.has_value()) {
        return true;
    }
    const uint32_t cpu = *event.cpu;
    const std::string kind = soft ? "softirq" : "irq";
    auto &stack = CurrentInterrupts(cpu);
    if (entry) {
        PendingInterrupt pending;
        pending.kind = kind;
        pending.vector = *vector;
        pending.action = soft ? SoftirqAction(*vector) : StringField(event, "name");
        if (pending.action.empty()) {
            pending.action = "IRQ " + std::to_string(*vector);
        }
        pending.tid = TidField(event, "common_pid");
        const auto running = CurrentRunning(cpu);
        if (pending.tid.has_value()) {
            std::string comm;
            if (running != cpuRunning.end() && running->second.tid == *pending.tid) {
                comm = running->second.comm;
            }
            Task &task = GetTask(*pending.tid, comm, cpu, event);
            pending.comm = task.comm;
            pending.taskKey = task.key;
        } else if (running != cpuRunning.end()) {
            pending.tid = running->second.tid;
            pending.comm = running->second.comm;
            pending.taskKey = running->second.taskKey;
        } else {
            pending.comm = "<unknown>";
        }
        pending.entry = event;
        stack.emplace_back(std::move(pending));
        return true;
    }
    if (stack.empty()) {
        return true;
    }
    // Match the same top-of-stack policy and maximum duration used by ftrace_tools.
    const auto pending = std::move(stack.back());
    stack.pop_back();
    if (pending.kind != kind || pending.vector != *vector || pending.entry.sourceId != event.sourceId ||
        event.timestampNs < pending.entry.timestampNs ||
        event.timestampNs - pending.entry.timestampNs > MAX_INTERRUPT_DURATION_NS) {
        return true;
    }
    MsprofFtraceSlice slice;
    slice.process = CPU_PROCESS;
    slice.threadKey = "cpu:" + std::to_string(cpu);
    slice.threadName = CpuThreadName(cpu);
    slice.name = kind;
    slice.timestampNs = pending.entry.timestampNs;
    slice.durationNs = event.timestampNs - pending.entry.timestampNs;
    auto args = EventArgs(pending.entry);
    Put(args, soft ? "action" : "name", pending.action);
    Put(args, "comm", pending.comm);
    Put(args, "taskKey", pending.taskKey);
    if (pending.tid.has_value()) {
        Put(args, "tid", std::to_string(*pending.tid));
        Put(args, "task", pending.comm + ":" + std::to_string(*pending.tid));
    } else {
        args.RemoveMember("tid");
        Put(args, "task", "<unknown>");
    }
    Put(args, "endGenericEventId", std::to_string(event.eventId));
    slice.args = Serialize(args);
    if (!Emit(std::move(slice), error)) {
        return false;
    }
    summary.hasSoftirq = summary.hasSoftirq || soft;
    summary.hasHardirq = summary.hasHardirq || !soft;
    return true;
}

bool MsprofFtraceBuilder::Impl::Consume(const MsprofFtraceEvent &event, std::string &error) {
    if (finished) {
        error = "Cannot consume ftrace events after finishing the builder";
        return false;
    }
    const auto order = std::make_pair(event.timestampNs, event.eventId);
    if (previousOrder.has_value() && order < *previousOrder) {
        error = "The ftrace builder requires timestamp/event-ID ordered input";
        return false;
    }
    previousOrder = order;
    ++summary.eventCount;
    if (!summary.minTimestampNs.has_value()) {
        summary.minTimestampNs = event.timestampNs;
    }
    summary.maxTimestampNs = event.timestampNs;
    if ((event.flags & MSPROF_FTRACE_FLAG_FIELD_FAILED) != 0) {
        ++summary.failedEventCount;
        // The failed target identity may be on another CPU. Invalidate lazily and globally.
        ++qualityEpoch;
        return true;
    }
    ++summary.trustedEventCount;
    if (!event.cpu.has_value()) {
        return true;
    }
    if (event.name == "sched:sched_switch") {
        return Switch(event, error);
    }
    if (event.name == "sched:sched_wakeup" || event.name == "sched:sched_wakeup_new") {
        return Wakeup(event, error);
    }
    if (event.name == "sched:sched_process_exit") {
        Exit(event);
        return true;
    }
    if (event.name == "irq:softirq_entry" || event.name == "irq:softirq_exit" ||
        event.name == "irq:irq_handler_entry" || event.name == "irq:irq_handler_exit") {
        return Interrupt(event, error);
    }
    return true;
}

bool MsprofFtraceBuilder::Impl::Finish(std::string &error) {
    (void)error;
    // Discard open intervals exactly as the existing converter does; do not infer missing events.
    cpuRunning.clear();
    interrupts.clear();
    tasks.clear();
    currentTasks.clear();
    finished = true;
    return true;
}

MsprofFtraceBuilder::MsprofFtraceBuilder(MsprofFtraceSink &sink, const std::map<uint32_t, uint32_t> &knownTgids)
    : impl(std::make_unique<Impl>(sink, knownTgids)) {}

MsprofFtraceBuilder::~MsprofFtraceBuilder() = default;

bool MsprofFtraceBuilder::Consume(const MsprofFtraceEvent &event, std::string &error) {
    return impl->Consume(event, error);
}

bool MsprofFtraceBuilder::Finish(std::string &error) { return impl->Finish(error); }

const MsprofFtraceBuildSummary &MsprofFtraceBuilder::GetSummary() const { return impl->summary; }

}
