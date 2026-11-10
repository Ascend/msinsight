/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "document.h"
#include "MsprofFtraceBuilder.h"
#include "MsprofFtraceWriter.h"
#include "TextTraceDatabase.h"

namespace Dic::Module::FullDb {
namespace {
constexpr uint64_t EPOCH = 1793654244074239046;

class CapturingFtraceSink : public MsprofFtraceSink {
  public:
    std::vector<MsprofFtraceSlice> slices;
    bool fail = false;
    bool AddSlice(const MsprofFtraceSlice &slice, std::string &error) override {
        if (fail) {
            error = "test write failure";
            return false;
        }
        slices.emplace_back(slice);
        return true;
    }
};

class MsprofFtraceBuilderTest : public testing::Test {
  protected:
    MsprofFtraceEvent Event(const std::string &name, uint64_t time, std::optional<uint32_t> cpu = 0) {
        MsprofFtraceEvent event;
        event.eventId = ++nextId;
        event.timestampNs = EPOCH + time;
        event.rawTimestamp = 5000 + time;
        event.sourceId = name.rfind("irq:", 0) == 0 ? 200 : 100;
        event.typeId = event.sourceId + nextId * 13;
        event.name = name;
        event.cpu = cpu;
        event.fields["common_pid"] = int64_t{42};
        return event;
    }
    MsprofFtraceEvent Switch(uint64_t time, uint32_t previous, uint32_t next, uint64_t state = 0, uint32_t cpu = 0,
        const std::string &prevComm = "worker_thread_1", const std::string &nextComm = "worker_thread_1") {
        auto event = Event("sched:sched_switch", time, cpu);
        event.fields["common_pid"] = static_cast<int64_t>(previous);
        event.fields["prev_pid"] = static_cast<int64_t>(previous);
        event.fields["next_pid"] = static_cast<int64_t>(next);
        event.fields["prev_state"] = state;
        event.fields["prev_comm"] = previous == 0 ? std::string("swapper") : prevComm;
        event.fields["next_comm"] = next == 0 ? std::string("swapper") : nextComm;
        event.fields["prev_prio"] = int64_t{120};
        event.fields["next_prio"] = int64_t{120};
        return event;
    }
    MsprofFtraceEvent Wakeup(uint64_t time, uint32_t target = 42, uint32_t targetCpu = 0) {
        auto event = Event("sched:sched_wakeup", time);
        event.fields["pid"] = static_cast<int64_t>(target);
        event.fields["comm"] = std::string("worker_thread_1");
        event.fields["target_cpu"] = static_cast<int64_t>(targetCpu);
        return event;
    }
    void Consume(const MsprofFtraceEvent &event) { ASSERT_TRUE(builder.Consume(event, error)) << error; }
    static std::string Arg(const MsprofFtraceSlice &slice, const char *key) {
        rapidjson::Document json;
        json.Parse(slice.args.c_str());
        if (json.HasParseError() || !json.IsObject() || !json.HasMember(key) || !json[key].IsString()) {
            return "";
        }
        return json[key].GetString();
    }
    std::vector<MsprofFtraceSlice> TaskSlices(uint32_t tid, const std::string &name = "") const {
        std::vector<MsprofFtraceSlice> result;
        for (const auto &slice : sink.slices) {
            if (slice.process == "Process Scheduling" && Arg(slice, "tid") == std::to_string(tid) &&
                (name.empty() || slice.name == name)) {
                result.emplace_back(slice);
            }
        }
        return result;
    }
    std::vector<MsprofFtraceSlice> CpuTaskSlices(uint32_t tid) const {
        std::vector<MsprofFtraceSlice> result;
        for (const auto &slice : sink.slices) {
            if (slice.process == "CPU Scheduling" && slice.name != "sched_switch" &&
                Arg(slice, "eventName") == "sched:sched_switch" && Arg(slice, "tid") == std::to_string(tid)) {
                result.emplace_back(slice);
            }
        }
        return result;
    }
    CapturingFtraceSink sink;
    MsprofFtraceBuilder builder{sink};
    std::string error;
    uint64_t nextId = 0;
};

TEST_F(MsprofFtraceBuilderTest, EpochNsRunningIncludesSoftirqAndExitWaitsForFinalSwitch) {
    Consume(Switch(0, 0, 42));
    auto entry = Event("irq:softirq_entry", 3);
    entry.fields["vec"] = uint64_t{3};
    entry.flags = 1;
    Consume(entry);
    auto exit = Event("irq:softirq_exit", 5);
    exit.fields["vec"] = uint64_t{3};
    Consume(exit);
    auto taskExit = Event("sched:sched_process_exit", 9);
    taskExit.fields["pid"] = int64_t{42};
    taskExit.fields["comm"] = std::string("worker_thread_1");
    Consume(taskExit);
    Consume(Switch(10, 42, 0));
    ASSERT_TRUE(builder.Finish(error));
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(Arg(running[0], "comm"), "worker_thread_1");
    const auto cpuRunning = CpuTaskSlices(42);
    ASSERT_EQ(cpuRunning.size(), 1u);
    EXPECT_EQ(cpuRunning[0].timestampNs, running[0].timestampNs);
    EXPECT_EQ(cpuRunning[0].durationNs, running[0].durationNs);
    const auto interrupt =
        std::find_if(sink.slices.begin(), sink.slices.end(), [](const auto &slice) { return slice.name == "softirq"; });
    ASSERT_NE(interrupt, sink.slices.end());
    EXPECT_EQ(interrupt->durationNs, 2u);
    EXPECT_EQ(Arg(*interrupt, "action"), "NET_RX");
    EXPECT_EQ(Arg(*interrupt, "task"), "worker_thread_1:42");
    EXPECT_EQ(Arg(*interrupt, "flags"), "1");
    EXPECT_TRUE(builder.GetSummary().hasScheduling);
    EXPECT_TRUE(builder.GetSummary().hasSoftirq);
    EXPECT_FALSE(builder.GetSummary().hasHardirq);
    EXPECT_EQ(builder.GetSummary().eventCount, 5u);
}

TEST_F(MsprofFtraceBuilderTest, HardirqOnlyPublishesCapabilityAfterCompletedInterval) {
    auto entry = Event("irq:irq_handler_entry", 3);
    entry.fields["irq"] = uint64_t{27};
    entry.fields["name"] = std::string("arch_timer");
    Consume(entry);
    EXPECT_FALSE(builder.GetSummary().hasHardirq);
    auto exit = Event("irq:irq_handler_exit", 8);
    exit.fields["irq"] = uint64_t{27};
    Consume(exit);
    ASSERT_TRUE(builder.Finish(error));
    ASSERT_EQ(sink.slices.size(), 1u);
    EXPECT_EQ(sink.slices[0].process, "CPU Scheduling");
    EXPECT_EQ(sink.slices[0].name, "irq");
    EXPECT_EQ(sink.slices[0].durationNs, 5u);
    EXPECT_EQ(Arg(sink.slices[0], "name"), "arch_timer");
    EXPECT_TRUE(builder.GetSummary().hasHardirq);
    EXPECT_FALSE(builder.GetSummary().hasScheduling);
    EXPECT_FALSE(builder.GetSummary().hasWakeup);
    EXPECT_FALSE(builder.GetSummary().hasSoftirq);
}

TEST_F(MsprofFtraceBuilderTest, UnpairedHardirqDoesNotPublishCapability) {
    auto entry = Event("irq:irq_handler_entry", 3);
    entry.fields["irq"] = uint64_t{27};
    Consume(entry);
    auto mismatch = Event("irq:irq_handler_exit", 8);
    mismatch.fields["irq"] = uint64_t{28};
    Consume(mismatch);
    auto unclosed = Event("irq:irq_handler_entry", 10);
    unclosed.fields["irq"] = uint64_t{27};
    Consume(unclosed);
    ASSERT_TRUE(builder.Finish(error));
    EXPECT_TRUE(sink.slices.empty());
    EXPECT_FALSE(builder.GetSummary().hasHardirq);
    EXPECT_FALSE(builder.GetSummary().hasSoftirq);
}

TEST_F(MsprofFtraceBuilderTest, FailedHardirqEmissionDoesNotPublishCapability) {
    auto entry = Event("irq:irq_handler_entry", 3);
    entry.fields["irq"] = uint64_t{27};
    Consume(entry);
    sink.fail = true;
    auto exit = Event("irq:irq_handler_exit", 8);
    exit.fields["irq"] = uint64_t{27};
    EXPECT_FALSE(builder.Consume(exit, error));
    EXPECT_EQ(error, "test write failure");
    EXPECT_TRUE(sink.slices.empty());
    EXPECT_FALSE(builder.GetSummary().hasHardirq);
}

TEST_F(MsprofFtraceBuilderTest, DuplicateWakeupDoesNotTruncateRunnableOrResetRunning) {
    Consume(Switch(0, 0, 42));
    Consume(Wakeup(3));
    Consume(Switch(10, 42, 0, 1));
    Consume(Wakeup(12));
    Consume(Wakeup(13));
    Consume(Switch(20, 0, 42));
    Consume(Switch(30, 42, 0));
    ASSERT_TRUE(builder.Finish(error));
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 2u);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(running[1].durationNs, 10u);
    const auto sleeping = TaskSlices(42, "Sleeping");
    ASSERT_EQ(sleeping.size(), 1u);
    EXPECT_EQ(sleeping[0].durationNs, 2u);
    const auto runnable = TaskSlices(42, "Runnable");
    ASSERT_EQ(runnable.size(), 1u);
    EXPECT_EQ(runnable[0].durationNs, 8u);
}

TEST_F(MsprofFtraceBuilderTest, MissingWakeupDoesNotTurnUnknownWaitingIntoSleeping) {
    Consume(Switch(0, 0, 42));
    Consume(Switch(10, 42, 0, 1));
    Consume(Switch(20, 0, 42));
    Consume(Switch(30, 42, 0));
    EXPECT_TRUE(TaskSlices(42, "Sleeping").empty());
    EXPECT_TRUE(TaskSlices(42, "Runnable").empty());
    EXPECT_EQ(TaskSlices(42, "Running").size(), 2u);
}

TEST_F(MsprofFtraceBuilderTest, MissingSwitchDoesNotEmitTheDisplacedTasksStaleRunningInterval) {
    Consume(Switch(10, 0, 42));
    Consume(Switch(20, 43, 44));
    Consume(Switch(30, 42, 45));
    ASSERT_TRUE(builder.Finish(error)) << error;
    EXPECT_TRUE(TaskSlices(42, "Running").empty());
    EXPECT_TRUE(CpuTaskSlices(42).empty());
    EXPECT_TRUE(TaskSlices(44, "Running").empty());
    EXPECT_TRUE(CpuTaskSlices(44).empty());
    EXPECT_EQ(builder.GetSummary().eventCount, 3u);
    EXPECT_TRUE(builder.GetSummary().hasScheduling);
}

TEST_F(MsprofFtraceBuilderTest, MissingSwitchAllowsALaterWakeupToEstablishRunnable) {
    Consume(Switch(10, 0, 42));
    Consume(Switch(20, 43, 44));
    Consume(Wakeup(25, 42, 1));
    Consume(Switch(30, 0, 42, 0, 1));
    Consume(Switch(40, 42, 0, 0, 1));
    ASSERT_TRUE(builder.Finish(error)) << error;
    const auto runnable = TaskSlices(42, "Runnable");
    ASSERT_EQ(runnable.size(), 1u);
    EXPECT_EQ(runnable[0].timestampNs, EPOCH + 25);
    EXPECT_EQ(runnable[0].durationNs, 5u);
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH + 30);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(running[0].threadKey, runnable[0].threadKey);
    EXPECT_EQ(running[0].threadKey, "tid:42:1");
}

TEST_F(MsprofFtraceBuilderTest, MissingSwitchRecoversFromAnExplicitSwitchInOnTheSameCpu) {
    Consume(Switch(10, 0, 42));
    Consume(Switch(20, 43, 44));
    Consume(Switch(30, 44, 42));
    Consume(Switch(40, 42, 0));
    ASSERT_TRUE(builder.Finish(error)) << error;
    const auto running = TaskSlices(42, "Running");
    const auto cpuRunning = CpuTaskSlices(42);
    ASSERT_EQ(running.size(), 1u);
    ASSERT_EQ(cpuRunning.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH + 30);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(cpuRunning[0].timestampNs, running[0].timestampNs);
    EXPECT_EQ(cpuRunning[0].durationNs, running[0].durationNs);
    const auto unaffected = TaskSlices(44, "Running");
    ASSERT_EQ(unaffected.size(), 1u);
    EXPECT_EQ(unaffected[0].timestampNs, EPOCH + 20);
    EXPECT_EQ(unaffected[0].durationNs, 10u);
}

TEST_F(MsprofFtraceBuilderTest, MissingSwitchRecoversOnAnotherCpuWithoutDiscardingOtherValidRunning) {
    Consume(Switch(10, 0, 42));
    Consume(Switch(20, 43, 44));
    Consume(Switch(30, 0, 42, 0, 1));
    Consume(Switch(40, 42, 0, 0, 1));
    Consume(Switch(50, 44, 0));
    ASSERT_TRUE(builder.Finish(error)) << error;
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH + 30);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(Arg(running[0], "cpu"), "1");
    const auto unaffected = TaskSlices(44, "Running");
    ASSERT_EQ(unaffected.size(), 1u);
    EXPECT_EQ(unaffected[0].timestampNs, EPOCH + 20);
    EXPECT_EQ(unaffected[0].durationNs, 30u);
}

TEST_F(MsprofFtraceBuilderTest, AMissingCpuOpenerDoesNotInvalidateATaskAlreadyRunningOnAnotherCpu) {
    Consume(Switch(10, 0, 42));
    Consume(Switch(15, 0, 42, 0, 1));
    Consume(Switch(20, 43, 44));
    Consume(Switch(30, 42, 0, 0, 1));
    ASSERT_TRUE(builder.Finish(error)) << error;
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH + 15);
    EXPECT_EQ(running[0].durationNs, 15u);
    EXPECT_EQ(Arg(running[0], "cpu"), "1");
}

TEST_F(MsprofFtraceBuilderTest, StaleCpuPendingDoesNotInvalidateANewerTaskLifetimeOnAnotherCpu) {
    Consume(Switch(10, 0, 42));
    auto exit = Event("sched:sched_process_exit", 11, 1);
    exit.fields["pid"] = int64_t{42};
    Consume(exit);
    Consume(Switch(12, 42, 43, 0, 1));
    Consume(Switch(13, 43, 42, 0, 1));
    Consume(Switch(20, 43, 44));
    Consume(Switch(30, 42, 0, 0, 1));
    ASSERT_TRUE(builder.Finish(error)) << error;
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH + 13);
    EXPECT_EQ(running[0].durationNs, 17u);
    EXPECT_EQ(running[0].threadKey, "tid:42:2");
    EXPECT_EQ(Arg(running[0], "cpu"), "1");
    const auto cpuRunning = CpuTaskSlices(42);
    ASSERT_EQ(cpuRunning.size(), 1u);
    EXPECT_EQ(cpuRunning[0].threadKey, "cpu:1");
}

TEST_F(MsprofFtraceBuilderTest, MissingSwitchPreservesAKnownExitUntilItsTaskLifetimeRetires) {
    Consume(Switch(10, 0, 42));
    auto exit = Event("sched:sched_process_exit", 11);
    exit.fields["pid"] = int64_t{42};
    Consume(exit);
    Consume(Switch(20, 43, 44));
    Consume(Wakeup(25));
    Consume(Switch(30, 42, 0));
    Consume(Switch(40, 0, 42));
    Consume(Switch(50, 42, 0));
    ASSERT_TRUE(builder.Finish(error)) << error;
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].timestampNs, EPOCH + 40);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(running[0].threadKey, "tid:42:2");
    EXPECT_TRUE(TaskSlices(42, "Runnable").empty());
}

TEST_F(MsprofFtraceBuilderTest, ThreadRenameKeepsIdentityAndExitStartsANewLifetime) {
    Consume(Switch(0, 0, 42, 0, 0, "", "old_worker"));
    Consume(Switch(10, 42, 0, 0, 0, "new_worker", ""));
    Consume(Switch(20, 0, 42, 0, 0, "", "new_worker"));
    auto exit = Event("sched:sched_process_exit", 29);
    exit.fields["pid"] = int64_t{42};
    exit.fields["comm"] = std::string("new_worker");
    Consume(exit);
    Consume(Switch(30, 42, 0));
    Consume(Switch(40, 0, 42));
    Consume(Switch(50, 42, 0));
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 3u);
    EXPECT_EQ(running[0].threadKey, running[1].threadKey);
    EXPECT_NE(running[1].threadKey, running[2].threadKey);
    EXPECT_EQ(Arg(running[0], "comm"), "old_worker");
    EXPECT_EQ(Arg(running[1], "comm"), "new_worker");
}

TEST_F(MsprofFtraceBuilderTest, CpuIdleIdentityAndInterruptMatchingAreSeparatedByCpu) {
    Consume(Switch(0, 0, 42, 0, 0));
    Consume(Switch(1, 0, 43, 0, 1));
    auto first = Event("irq:softirq_entry", 3, 0);
    first.fields["vec"] = uint64_t{3};
    Consume(first);
    auto second = Event("irq:softirq_entry", 4, 1);
    second.fields["common_pid"] = int64_t{43};
    second.fields["vec"] = uint64_t{3};
    Consume(second);
    auto endFirst = Event("irq:softirq_exit", 5, 0);
    endFirst.fields["vec"] = uint64_t{3};
    Consume(endFirst);
    auto endSecond = Event("irq:softirq_exit", 8, 1);
    endSecond.fields["vec"] = uint64_t{3};
    Consume(endSecond);
    Consume(Switch(10, 42, 0, 0, 0));
    Consume(Switch(11, 43, 0, 0, 1));
    const auto idle = TaskSlices(0, "Runnable");
    ASSERT_EQ(idle.size(), 2u);
    EXPECT_NE(idle[0].threadKey, idle[1].threadKey);
    std::vector<uint64_t> durations;
    for (const auto &slice : sink.slices) {
        if (slice.name == "softirq") {
            durations.push_back(slice.durationNs);
        }
    }
    EXPECT_EQ(durations, (std::vector<uint64_t>{2, 4}));
}

TEST_F(MsprofFtraceBuilderTest, NullCpuDoesNotInventCpuZeroOrAStandaloneTrack) {
    auto event = Event("sched:sched_switch", 0, std::nullopt);
    event.fields["cpu"] = uint64_t{99};
    Consume(event);
    ASSERT_TRUE(builder.Finish(error));
    EXPECT_TRUE(sink.slices.empty());
    EXPECT_FALSE(builder.GetSummary().hasScheduling);
    EXPECT_EQ(builder.GetSummary().eventCount, 1u);
    EXPECT_EQ(builder.GetSummary().minTimestampNs, EPOCH);
    EXPECT_EQ(builder.GetSummary().maxTimestampNs, EPOCH);
}

TEST_F(MsprofFtraceBuilderTest, CpuLabelsPadToThreeDigitsWithoutChangingIdentityOrDetails) {
    const std::vector<std::pair<uint32_t, std::string>> cpus = {{0, "CPU 000"}, {1, "CPU 001"}, {2, "CPU 002"},
        {10, "CPU 010"}, {11, "CPU 011"}, {100, "CPU 100"}, {1000, "CPU 1000"}};
    uint64_t time = 0;
    for (const auto &[cpu, label] : cpus) {
        const auto firstSlice = sink.slices.size();
        Consume(Switch(time++, 0, 42, 0, cpu));
        auto entry = Event("irq:softirq_entry", time++, cpu);
        entry.fields["vec"] = uint64_t{3};
        Consume(entry);
        auto exit = Event("irq:softirq_exit", time++, cpu);
        exit.fields["vec"] = uint64_t{3};
        Consume(exit);
        Consume(Switch(time++, 42, 0, 0, cpu));
        std::vector<std::string> cpuSlices;
        for (size_t index = firstSlice; index < sink.slices.size(); ++index) {
            const auto &slice = sink.slices[index];
            if (slice.process != "CPU Scheduling") {
                continue;
            }
            EXPECT_EQ(slice.threadName, label);
            EXPECT_EQ(slice.threadKey, "cpu:" + std::to_string(cpu));
            EXPECT_EQ(Arg(slice, "cpu"), std::to_string(cpu));
            cpuSlices.push_back(slice.name);
        }
        EXPECT_EQ(
            cpuSlices, (std::vector<std::string>{"sched_switch", "softirq", "sched_switch", "worker_thread_1:42"}));
    }
    ASSERT_TRUE(builder.Finish(error)) << error;
}

TEST_F(MsprofFtraceBuilderTest, SupportedSliceDetailsRetainCompleteFieldsAndPreciseIntegers) {
    auto event = Switch(0, 0, 42, 0, 8);
    event.fields["quoted_name"] = std::string("worker\"\\\n");
    event.fields["empty"] = std::string("");
    event.fields["missing"] = std::monostate{};
    event.fields["cpu"] = uint64_t{99};
    event.fields["large_unsigned"] = std::numeric_limits<uint64_t>::max();
    event.fields["negative_signed"] = std::numeric_limits<int64_t>::min();
    Consume(event);
    ASSERT_EQ(sink.slices.size(), 1u);
    EXPECT_EQ(sink.slices[0].process, "CPU Scheduling");
    rapidjson::Document args;
    args.Parse(sink.slices[0].args.c_str());
    ASSERT_FALSE(args.HasParseError());
    EXPECT_STREQ(args["cpu"].GetString(), "8");
    EXPECT_TRUE(args["fields"]["missing"].IsNull());
    EXPECT_STREQ(args["fields"]["cpu"].GetString(), "99");
    EXPECT_STREQ(args["fields"]["empty"].GetString(), "");
    EXPECT_STREQ(args["fields"]["quoted_name"].GetString(), "worker\"\\\n");
    EXPECT_STREQ(args["fields"]["large_unsigned"].GetString(), "18446744073709551615");
    EXPECT_STREQ(args["fields"]["negative_signed"].GetString(), "-9223372036854775808");
    EXPECT_STREQ(args["rawTimestamp"].GetString(), "5000");
}

TEST_F(MsprofFtraceBuilderTest, SupportedFloatDetailsPreserveRoundTripPrecisionAndSmallValues) {
    auto event = Switch(0, 0, 42);
    event.fields["precise"] = 1.1234567891234567;
    event.fields["tiny"] = 1e-20;
    Consume(event);
    ASSERT_EQ(sink.slices.size(), 1u);
    rapidjson::Document args;
    args.Parse(sink.slices[0].args.c_str());
    ASSERT_FALSE(args.HasParseError());
    EXPECT_DOUBLE_EQ(std::stod(args["precise"].GetString()), 1.1234567891234567);
    EXPECT_DOUBLE_EQ(std::stod(args["tiny"].GetString()), 1e-20);
    EXPECT_DOUBLE_EQ(args["fields"]["precise"].GetDouble(), 1.1234567891234567);
    EXPECT_DOUBLE_EQ(args["fields"]["tiny"].GetDouble(), 1e-20);
    EXPECT_NE(std::string(args["tiny"].GetString()), "0.000000");
}

TEST_F(MsprofFtraceBuilderTest, PreemptMaskIsReadAsANumericFormatNotRawTaskStateFlags) {
    Consume(Switch(0, 0, 42));
    auto event = Switch(10, 42, 0, 256);
    auto type = std::make_shared<MsprofFtraceType>();
    type->format = "print fmt: __print_flags(REC->prev_state & (256 - 1), \"|\", {1, \"S\"})";
    event.type = type;
    Consume(event);
    auto wake = Switch(20, 0, 42);
    Consume(wake);
    const auto runnable = TaskSlices(42, "Runnable");
    ASSERT_EQ(runnable.size(), 1u);
    EXPECT_EQ(runnable[0].durationNs, 10u);
    const auto found = std::find_if(sink.slices.begin(), sink.slices.end(),
        [&](const auto &slice) { return slice.name == "sched_switch" && slice.timestampNs == EPOCH + 10; });
    ASSERT_NE(found, sink.slices.end());
    EXPECT_EQ(Arg(*found, "prev_state"), "R+");
    rapidjson::Document args;
    args.Parse(found->args.c_str());
    ASSERT_TRUE(args.HasMember("preempted"));
    EXPECT_TRUE(args["preempted"].GetBool());
}

TEST_F(MsprofFtraceBuilderTest, FailedZeroPlaceholdersNeverCreateIdleOrStatisticsCapabilities) {
    for (const auto flags : {uint32_t{2}, uint32_t{3}, uint32_t{0x102}}) {
        auto failed = Switch(nextId, 0, 0);
        failed.flags = flags;
        Consume(failed);
    }
    ASSERT_TRUE(builder.Finish(error));
    EXPECT_TRUE(sink.slices.empty());
    const auto &summary = builder.GetSummary();
    EXPECT_EQ(summary.eventCount, 3u);
    EXPECT_EQ(summary.trustedEventCount, 0u);
    EXPECT_EQ(summary.failedEventCount, 3u);
    EXPECT_FALSE(summary.hasScheduling);
    EXPECT_FALSE(summary.hasWakeup);
    EXPECT_FALSE(summary.hasSoftirq);
    EXPECT_FALSE(summary.hasHardirq);
}

TEST_F(MsprofFtraceBuilderTest, QualityGapDiscardsOpenIntervalsAndKeepsTheExistingTaskIdentity) {
    Consume(Switch(0, 0, 42));
    Consume(Switch(10, 42, 0));
    Consume(Switch(20, 0, 42));
    auto failed = Switch(25, 0, 0);
    failed.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
    Consume(failed);
    Consume(Switch(30, 42, 0));
    Consume(Switch(40, 0, 42));
    Consume(Switch(50, 42, 0));
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 2u);
    EXPECT_EQ(running[0].timestampNs, EPOCH);
    EXPECT_EQ(running[1].timestampNs, EPOCH + 40);
    EXPECT_EQ(running[0].durationNs, 10u);
    EXPECT_EQ(running[1].durationNs, 10u);
    EXPECT_EQ(running[0].threadKey, running[1].threadKey);
    EXPECT_EQ(builder.GetSummary().trustedEventCount, 6u);
    EXPECT_EQ(builder.GetSummary().failedEventCount, 1u);
    EXPECT_TRUE(std::none_of(sink.slices.begin(), sink.slices.end(), [](const auto &slice) {
        return slice.timestampNs < EPOCH + 25 && slice.timestampNs + slice.durationNs > EPOCH + 25;
    }));
}

TEST_F(MsprofFtraceBuilderTest, FailedWakeupInvalidatesAWaitingTaskOnAnotherCpu) {
    Consume(Switch(0, 0, 42, 0, 1));
    Consume(Switch(10, 42, 0, 1, 1));
    auto failed = Wakeup(20, 0, 0);
    failed.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
    Consume(failed);
    Consume(Wakeup(30, 42, 1));
    Consume(Switch(40, 0, 42, 0, 1));
    Consume(Switch(50, 42, 0, 0, 1));
    EXPECT_TRUE(TaskSlices(42, "Sleeping").empty());
    const auto runnable = TaskSlices(42, "Runnable");
    ASSERT_EQ(runnable.size(), 1u);
    EXPECT_EQ(runnable[0].timestampNs, EPOCH + 30);
    EXPECT_EQ(runnable[0].durationNs, 10u);
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 2u);
    EXPECT_EQ(running[0].threadKey, running[1].threadKey);
}

TEST_F(MsprofFtraceBuilderTest, FailedExitDoesNotRetireIdleOrInventAnotherTaskLifetime) {
    Consume(Switch(0, 0, 42));
    Consume(Switch(10, 42, 0));
    auto failed = Event("sched:sched_process_exit", 15);
    failed.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
    failed.fields["pid"] = int64_t{0};
    Consume(failed);
    Consume(Switch(20, 0, 42));
    Consume(Switch(30, 42, 0));
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 2u);
    EXPECT_EQ(running[0].threadKey, running[1].threadKey);
    const auto idle = TaskSlices(0);
    EXPECT_TRUE(std::all_of(idle.begin(), idle.end(), [](const auto &slice) { return slice.threadKey == "idle:0:1"; }));
}

TEST_F(MsprofFtraceBuilderTest, ATrustedExitBeforeTheGapStillRetiresItsKnownLifetime) {
    Consume(Switch(0, 0, 42));
    Consume(Switch(10, 42, 0));
    Consume(Switch(20, 0, 42));
    auto exit = Event("sched:sched_process_exit", 21);
    exit.fields["pid"] = int64_t{42};
    Consume(exit);
    auto failed = Event("irq:softirq_exit", 22);
    failed.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
    Consume(failed);
    Consume(Switch(30, 42, 0));
    Consume(Switch(40, 0, 42));
    Consume(Switch(50, 42, 0));
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 2u);
    EXPECT_NE(running[0].threadKey, running[1].threadKey);
}

TEST_F(MsprofFtraceBuilderTest, FailedIrqEntryAndExitCannotPairWithEventsAcrossTheGap) {
    uint64_t time = 0;
    for (const bool soft : {true, false}) {
        const auto entryName = soft ? "irq:softirq_entry" : "irq:irq_handler_entry";
        const auto exitName = soft ? "irq:softirq_exit" : "irq:irq_handler_exit";
        const auto field = soft ? "vec" : "irq";
        auto entry = Event(entryName, time);
        entry.fields[field] = uint64_t{3};
        Consume(entry);
        auto failedExit = Event(exitName, time + 5);
        failedExit.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
        Consume(failedExit);
        auto exit = Event(exitName, time + 10);
        exit.fields[field] = uint64_t{3};
        Consume(exit);
        auto failedEntry = Event(entryName, time + 11);
        failedEntry.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
        Consume(failedEntry);
        exit = Event(exitName, time + 12);
        exit.fields[field] = uint64_t{3};
        Consume(exit);
        entry = Event(entryName, time + 20);
        entry.fields[field] = uint64_t{3};
        Consume(entry);
        exit = Event(exitName, time + 25);
        exit.fields[field] = uint64_t{3};
        Consume(exit);
        time += 30;
    }
    ASSERT_EQ(sink.slices.size(), 2u);
    EXPECT_EQ(sink.slices[0].name, "softirq");
    EXPECT_EQ(sink.slices[1].name, "irq");
    EXPECT_EQ(sink.slices[0].durationNs, 5u);
    EXPECT_EQ(sink.slices[1].durationNs, 5u);
    EXPECT_TRUE(builder.GetSummary().hasSoftirq);
    EXPECT_TRUE(builder.GetSummary().hasHardirq);
    EXPECT_EQ(builder.GetSummary().failedEventCount, 4u);
}

TEST_F(MsprofFtraceBuilderTest, InterruptAttributionDoesNotUseCpuRunningFromBeforeTheGap) {
    Consume(Switch(0, 0, 42));
    auto failed = Wakeup(1);
    failed.flags = MSPROF_FTRACE_FLAG_FIELD_FAILED;
    Consume(failed);
    auto entry = Event("irq:softirq_entry", 2);
    entry.fields.erase("common_pid");
    entry.fields["vec"] = uint64_t{3};
    Consume(entry);
    auto exit = Event("irq:softirq_exit", 5);
    exit.fields["vec"] = uint64_t{3};
    Consume(exit);
    const auto irq =
        std::find_if(sink.slices.begin(), sink.slices.end(), [](const auto &slice) { return slice.name == "softirq"; });
    ASSERT_NE(irq, sink.slices.end());
    EXPECT_EQ(Arg(*irq, "task"), "<unknown>");
    EXPECT_EQ(Arg(*irq, "tid"), "");
}

TEST_F(MsprofFtraceBuilderTest, IncompleteCpuAndCommonFlagsDoNotDiscardLegitimateZeroValues) {
    uint64_t time = 0;
    for (const auto flags : {uint32_t{0}, uint32_t{1}, uint32_t{0x100}}) {
        auto first = Switch(time, 0, 42);
        first.flags = flags;
        Consume(first);
        auto last = Switch(time + 10, 42, 0);
        last.flags = flags;
        Consume(last);
        time += 20;
    }
    EXPECT_EQ(TaskSlices(42, "Running").size(), 3u);
    EXPECT_EQ(builder.GetSummary().trustedEventCount, 6u);
    EXPECT_EQ(builder.GetSummary().failedEventCount, 0u);
    const auto switches = std::count_if(
        sink.slices.begin(), sink.slices.end(), [](const auto &slice) { return slice.name == "sched_switch"; });
    EXPECT_EQ(switches, 6);
}

TEST_F(MsprofFtraceBuilderTest, ExpandedKernelPreemptExpressionProducesPreemptedRunnable) {
    const std::string mask = "(((0x0000 | 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0040) + 1) << 1)";
    auto type = std::make_shared<MsprofFtraceType>();
    type->format = "print fmt: __print_flags(REC->prev_state & (" + mask +
        " - 1), \"|\", {1,\"S\"}), REC->prev_state & " + mask + " ? \"+\" : \"\"";
    Consume(Switch(0, 0, 42));
    auto event = Switch(10, 42, 0, 256);
    event.type = type;
    Consume(event);
    Consume(Switch(20, 0, 42));
    const auto runnable = TaskSlices(42, "Runnable");
    ASSERT_EQ(runnable.size(), 1u);
    EXPECT_EQ(runnable[0].durationNs, 10u);
    const auto found = std::find_if(sink.slices.begin(), sink.slices.end(),
        [](const auto &slice) { return slice.name == "sched_switch" && slice.timestampNs == EPOCH + 10; });
    ASSERT_NE(found, sink.slices.end());
    EXPECT_EQ(Arg(*found, "displayCode"), "R+");
    rapidjson::Document args;
    args.Parse(found->args.c_str());
    ASSERT_TRUE(args.HasMember("preempted"));
    EXPECT_TRUE(args["preempted"].GetBool());
}

TEST_F(MsprofFtraceBuilderTest, InvalidOrConflictingConstantMasksRemainUnknown) {
    const std::vector<std::string> expressions = {"(1 << 64)", "(18446744073709551615 + 1)", "(0 - 1)", "(3)",
        "(system(1))", std::string(20, '(') + "256" + std::string(20, ')'), "(256) - 1), REC->prev_state & (1024 - 1",
        "(1 || 256)", "(0x100UU)"};
    for (const auto &expression : expressions) {
        CapturingFtraceSink captured;
        MsprofFtraceBuilder isolated(captured);
        auto type = std::make_shared<MsprofFtraceType>();
        type->format = "print fmt: __print_flags(REC->prev_state & (" + expression + " - 1), \"|\")";
        auto event = Switch(0, 42, 0, 256);
        event.type = type;
        ASSERT_TRUE(isolated.Consume(event, error));
        ASSERT_EQ(captured.slices.size(), 1u);
        EXPECT_EQ(Arg(captured.slices[0], "displayCode"), "Unknown") << expression;
        rapidjson::Document args;
        args.Parse(captured.slices[0].args.c_str());
        EXPECT_FALSE(args.HasMember("preempted")) << expression;
    }
}

TEST_F(MsprofFtraceBuilderTest, AParenthesizedMaskPrefixDoesNotHideAdditionalOperators) {
    for (const auto *format : {"print fmt: REC->prev_state & (256 - 1) << 1",
             "print fmt: REC->prev_state & (256 - 1) + 1", "print fmt: REC->prev_state & (256 - 1) | 0"}) {
        CapturingFtraceSink captured;
        MsprofFtraceBuilder isolated(captured);
        auto type = std::make_shared<MsprofFtraceType>();
        type->format = format;
        auto event = Switch(0, 42, 0, 256);
        event.type = type;
        ASSERT_TRUE(isolated.Consume(event, error));
        ASSERT_EQ(captured.slices.size(), 1u);
        EXPECT_EQ(Arg(captured.slices[0], "displayCode"), "Unknown");
    }
}

TEST_F(MsprofFtraceBuilderTest, ConstantExpressionHonorsPrecedenceAndBoundedInput) {
    for (const auto &expression :
        {std::string("(1 << 7 + 1)"), std::string("(0x100ULL)"), std::string("((0x200 >> 1) & 0x1ff)")}) {
        CapturingFtraceSink captured;
        MsprofFtraceBuilder isolated(captured);
        auto type = std::make_shared<MsprofFtraceType>();
        type->format = "print fmt: __print_flags(REC->prev_state & (" + expression + " - 1), \"|\")";
        auto event = Switch(0, 42, 0, 256);
        event.type = type;
        ASSERT_TRUE(isolated.Consume(event, error));
        ASSERT_EQ(captured.slices.size(), 1u);
        EXPECT_EQ(Arg(captured.slices[0], "displayCode"), "R+") << expression;
    }
    auto type = std::make_shared<MsprofFtraceType>();
    type->format = std::string(1024 * 1024 + 1, 'x') + " REC->prev_state & (256 - 1)";
    auto event = Switch(0, 42, 0, 256);
    event.type = type;
    Consume(event);
    ASSERT_EQ(sink.slices.size(), 1u);
    EXPECT_EQ(Arg(sink.slices[0], "displayCode"), "Unknown");
}

TEST_F(MsprofFtraceBuilderTest, UnclosedIntervalsAreNotExtendedAtFinish) {
    Consume(Switch(0, 0, 42));
    auto entry = Event("irq:softirq_entry", 3);
    entry.fields["vec"] = uint64_t{3};
    Consume(entry);
    ASSERT_TRUE(builder.Finish(error));
    EXPECT_TRUE(TaskSlices(42, "Running").empty());
    EXPECT_EQ(std::count_if(
                  sink.slices.begin(), sink.slices.end(), [](const auto &slice) { return slice.name == "softirq"; }),
        0);
}

TEST_F(MsprofFtraceBuilderTest, InterruptsWithoutTaskContextRemainVisibleAndNeverBecomeIdleTidZero) {
    auto entry = Event("irq:softirq_entry", 0);
    entry.fields.erase("common_pid");
    entry.fields["vec"] = uint64_t{3};
    Consume(entry);
    auto exit = Event("irq:softirq_exit", 10);
    exit.fields.erase("common_pid");
    exit.fields["vec"] = uint64_t{3};
    Consume(exit);
    const auto irq =
        std::find_if(sink.slices.begin(), sink.slices.end(), [](const auto &slice) { return slice.name == "softirq"; });
    ASSERT_NE(irq, sink.slices.end());
    rapidjson::Document args;
    args.Parse(irq->args.c_str());
    EXPECT_FALSE(args.HasMember("tid"));
    EXPECT_STREQ(args["task"].GetString(), "<unknown>");
    EXPECT_STREQ(args["comm"].GetString(), "<unknown>");
    EXPECT_EQ(irq->durationNs, 10u);
}

TEST_F(MsprofFtraceBuilderTest, InterruptMismatchesAndOverlongIntervalsUseExistingDiscardBoundaries) {
    auto entry = Event("irq:softirq_entry", 0);
    entry.fields["vec"] = uint64_t{3};
    Consume(entry);
    auto wrong = Event("irq:softirq_exit", 10);
    wrong.fields["vec"] = uint64_t{3};
    wrong.sourceId = entry.sourceId + 1;
    Consume(wrong);
    auto later = Event("irq:softirq_entry", 20);
    later.fields["vec"] = uint64_t{3};
    Consume(later);
    auto end = Event("irq:softirq_exit", 1000000021);
    end.fields["vec"] = uint64_t{3};
    Consume(end);
    EXPECT_EQ(std::count_if(
                  sink.slices.begin(), sink.slices.end(), [](const auto &slice) { return slice.name == "softirq"; }),
        0);
    EXPECT_TRUE(sink.slices.empty());
}

TEST_F(MsprofFtraceBuilderTest, GlobalTidFallbackDoesNotInventProcessIdentityForNextScheduledTask) {
    auto first = Switch(0, 0, 42);
    first.globalTid = uint64_t{0};
    Consume(first);
    auto second = Switch(10, 42, 0);
    second.globalTid = (uint64_t{42} << 32) | 42;
    Consume(second);
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    rapidjson::Document args;
    args.Parse(running[0].args.c_str());
    EXPECT_FALSE(args.HasMember("tgid"));

    CapturingFtraceSink other;
    MsprofFtraceBuilder trusted(other, {{42, 7}});
    ASSERT_TRUE(trusted.Consume(first, error));
    ASSERT_TRUE(trusted.Consume(second, error));
    const auto match = std::find_if(other.slices.begin(), other.slices.end(),
        [](const auto &slice) { return slice.process == "Process Scheduling" && slice.name == "Running"; });
    ASSERT_NE(match, other.slices.end());
    EXPECT_EQ(Arg(*match, "tgid"), "7");
}

TEST_F(MsprofFtraceBuilderTest, PowerAndUnknownEventsAreCountedWithoutAdditionalTracks) {
    auto entry = Event("power:cpu_idle", 0);
    entry.fields["cpu_id"] = uint64_t{8};
    entry.fields["state"] = uint64_t{1};
    Consume(entry);
    auto exit = Event("power:cpu_idle", 10);
    exit.fields["cpu_id"] = uint64_t{8};
    exit.fields["state"] = uint64_t{0xffffffff};
    Consume(exit);
    auto frequency = Event("power:cpu_frequency", 11);
    frequency.fields["cpu_id"] = uint64_t{8};
    frequency.fields["state"] = uint64_t{2400000};
    Consume(frequency);
    Consume(Event("custom:event", 20));
    ASSERT_TRUE(builder.Finish(error));
    EXPECT_TRUE(sink.slices.empty());
    EXPECT_EQ(builder.GetSummary().eventCount, 4u);
    EXPECT_EQ(builder.GetSummary().sliceCount, 0u);
    EXPECT_EQ(builder.GetSummary().minTimestampNs, EPOCH);
    EXPECT_EQ(builder.GetSummary().maxTimestampNs, EPOCH + 20);
    EXPECT_FALSE(builder.GetSummary().hasScheduling);
    EXPECT_FALSE(builder.GetSummary().hasWakeup);
    EXPECT_FALSE(builder.GetSummary().hasSoftirq);
    EXPECT_FALSE(builder.GetSummary().hasHardirq);
}

TEST_F(MsprofFtraceBuilderTest, MixedEventsOnlyEmitLegacySchedulingGroups) {
    Consume(Event("custom:begin", 0));
    Consume(Switch(1, 0, 42));
    auto idle = Event("power:cpu_idle", 2);
    idle.fields["cpu_id"] = uint64_t{0};
    idle.fields["state"] = uint64_t{1};
    Consume(idle);
    auto entry = Event("irq:softirq_entry", 3);
    entry.fields["vec"] = uint64_t{3};
    Consume(entry);
    auto exit = Event("irq:softirq_exit", 5);
    exit.fields["vec"] = uint64_t{3};
    Consume(exit);
    auto hardEntry = Event("irq:irq_handler_entry", 6);
    hardEntry.fields["irq"] = uint64_t{27};
    hardEntry.fields["name"] = std::string("arch_timer");
    Consume(hardEntry);
    auto hardExit = Event("irq:irq_handler_exit", 7);
    hardExit.fields["irq"] = uint64_t{27};
    Consume(hardExit);
    auto idleExit = Event("power:cpu_idle", 8);
    idleExit.fields["cpu_id"] = uint64_t{0};
    idleExit.fields["state"] = uint64_t{0xffffffff};
    Consume(idleExit);
    Consume(Switch(10, 42, 0));
    Consume(Event("custom:end", 20));
    ASSERT_TRUE(builder.Finish(error));
    ASSERT_FALSE(sink.slices.empty());
    EXPECT_TRUE(std::all_of(sink.slices.begin(), sink.slices.end(),
        [](const auto &slice) { return slice.process == "CPU Scheduling" || slice.process == "Process Scheduling"; }));
    EXPECT_EQ(builder.GetSummary().eventCount, 10u);
    EXPECT_EQ(builder.GetSummary().sliceCount, sink.slices.size());
    EXPECT_EQ(builder.GetSummary().minTimestampNs, EPOCH);
    EXPECT_EQ(builder.GetSummary().maxTimestampNs, EPOCH + 20);
    const auto running = TaskSlices(42, "Running");
    ASSERT_EQ(running.size(), 1u);
    EXPECT_EQ(running[0].durationNs, 9u);
    EXPECT_TRUE(builder.GetSummary().hasScheduling);
    EXPECT_TRUE(builder.GetSummary().hasSoftirq);
    EXPECT_TRUE(builder.GetSummary().hasHardirq);
}

TEST_F(MsprofFtraceBuilderTest, UnorderedInputAndSinkFailurePreventPublication) {
    Consume(Event("custom:event", 10));
    EXPECT_FALSE(builder.Consume(Event("custom:event", 9), error));
    EXPECT_NE(error.find("ordered"), std::string::npos);
    CapturingFtraceSink failed;
    failed.fail = true;
    MsprofFtraceBuilder failing(failed);
    EXPECT_FALSE(failing.Consume(Switch(20, 0, 42), error));
    EXPECT_EQ(error, "test write failure");
}

class WriterMemoryDatabase : public Timeline::TextTraceDatabase {
  public:
    explicit WriterMemoryDatabase(std::recursive_mutex &mutex) : TextTraceDatabase(mutex) {}
    void Attach(sqlite3 *raw) {
        db = raw;
        isOpen = true;
        path = ":memory:";
    }
};

TEST(MsprofFtraceWriterTest, WritesCompleteNamesAndTypedTracksWithoutCounters) {
    sqlite3 *raw = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &raw), SQLITE_OK);
    std::recursive_mutex mutex;
    auto database = std::make_shared<WriterMemoryDatabase>(mutex);
    database->Attach(raw);
    MsprofFtraceWriter writer(database);
    std::string error;
    ASSERT_TRUE(writer.Begin(error)) << error;
    MsprofFtraceSlice slice;
    slice.process = "Process Scheduling";
    slice.threadKey = "tid:42:1";
    slice.threadName = "worker_thread_1:42";
    slice.name = "Running";
    slice.timestampNs = EPOCH;
    slice.durationNs = 10;
    slice.args = R"({"source":"msprof_ftrace","tid":"42","cpu":"8"})";
    ASSERT_TRUE(writer.AddSlice(slice, error)) << error;
    ASSERT_TRUE(writer.Finish(error)) << error;
    sqlite3_stmt *query = nullptr;
    ASSERT_EQ(
        sqlite3_prepare_v2(raw, "SELECT timestamp,duration,end_time,thread_name FROM slice JOIN thread USING(track_id)",
            -1, &query, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_step(query), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int64(query, 0), static_cast<int64_t>(EPOCH));
    EXPECT_EQ(sqlite3_column_int64(query, 1), 10);
    EXPECT_EQ(sqlite3_column_int64(query, 2), static_cast<int64_t>(EPOCH + 10));
    EXPECT_STREQ(reinterpret_cast<const char *>(sqlite3_column_text(query, 3)), "worker_thread_1:42");
    sqlite3_finalize(query);
    ASSERT_EQ(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM counter", -1, &query, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(query), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(query, 0), 0);
    sqlite3_finalize(query);
}

TEST(MsprofFtraceWriterTest, FlushesStreamingBatchesAndRejectsOverflowingTimestamps) {
    sqlite3 *raw = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &raw), SQLITE_OK);
    std::recursive_mutex mutex;
    auto database = std::make_shared<WriterMemoryDatabase>(mutex);
    database->Attach(raw);
    MsprofFtraceWriter writer(database);
    std::string error;
    ASSERT_TRUE(writer.Begin(error)) << error;
    MsprofFtraceSlice slice;
    slice.process = "CPU Scheduling";
    slice.threadKey = "cpu:8";
    slice.threadName = "CPU 8";
    slice.name = "sched_switch";
    slice.args = "{}";
    for (uint64_t index = 0; index < 600; ++index) {
        slice.timestampNs = EPOCH + index;
        ASSERT_TRUE(writer.AddSlice(slice, error)) << error;
    }
    slice.timestampNs = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    slice.durationNs = 1;
    EXPECT_FALSE(writer.AddSlice(slice, error));
    EXPECT_NE(error.find("timestamp range"), std::string::npos);
    ASSERT_TRUE(writer.Finish(error)) << error;
    sqlite3_stmt *query = nullptr;
    ASSERT_EQ(
        sqlite3_prepare_v2(raw, "SELECT COUNT(*),COUNT(DISTINCT track_id),MAX(timestamp)-MIN(timestamp) FROM slice", -1,
            &query, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_step(query), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(query, 0), 600);
    EXPECT_EQ(sqlite3_column_int(query, 1), 1);
    EXPECT_EQ(sqlite3_column_int64(query, 2), 599);
    sqlite3_finalize(query);
    ASSERT_EQ(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM counter", -1, &query, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(query), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(query, 0), 0);
    sqlite3_finalize(query);
}

TEST(MsprofFtraceWriterTest, DepthScansUseCoveringIndexWithoutReadingSlicePayload) {
    sqlite3 *raw = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &raw), SQLITE_OK);
    std::recursive_mutex mutex;
    auto database = std::make_shared<WriterMemoryDatabase>(mutex);
    database->Attach(raw);
    MsprofFtraceWriter writer(database);
    std::string error;
    ASSERT_TRUE(writer.Begin(error)) << error;
    MsprofFtraceSlice slice;
    slice.process = "CPU Scheduling";
    slice.name = "irq";
    slice.durationNs = 5;
    slice.args = R"({"source":"msprof_ftrace","name":"arch_timer"})";
    for (uint64_t index = 0; index < 12; ++index) {
        slice.threadKey = "cpu:" + std::to_string(index % 3);
        slice.threadName = "CPU " + std::to_string(index % 3);
        slice.timestampNs = EPOCH + index * 10;
        ASSERT_TRUE(writer.AddSlice(slice, error)) << error;
    }
    ASSERT_TRUE(writer.Finish(error)) << error;
    // Exercise the actual depth-reader projections and ordering without forcing an index.
    for (const std::string filter : {"(cat IS NULL OR cat != 'python_function')", "cat = 'python_function'"}) {
        SCOPED_TRACE(filter);
        const std::string sql = "EXPLAIN QUERY PLAN SELECT track_id,id,timestamp,end_time,COALESCE(group_id,'') "
                                "FROM slice WHERE " +
            filter + " ORDER BY track_id,timestamp,id";
        sqlite3_stmt *query = nullptr;
        ASSERT_EQ(sqlite3_prepare_v2(raw, sql.c_str(), -1, &query, nullptr), SQLITE_OK);
        std::string plan;
        int result = SQLITE_OK;
        while ((result = sqlite3_step(query)) == SQLITE_ROW) {
            const auto detail = sqlite3_column_text(query, 3);
            if (detail != nullptr) {
                plan += reinterpret_cast<const char *>(detail);
            }
        }
        EXPECT_EQ(result, SQLITE_DONE);
        EXPECT_NE(plan.find("USING COVERING INDEX msprof_ftrace_slice_track_time"), std::string::npos) << plan;
        EXPECT_EQ(plan.find("USE TEMP B-TREE"), std::string::npos) << plan;
        sqlite3_finalize(query);
    }
}

TEST(MsprofFtraceWriterTest, SameDisplayNameInDifferentTaskLifetimesKeepsDistinctTextThreadIdentity) {
    sqlite3 *raw = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &raw), SQLITE_OK);
    std::recursive_mutex mutex;
    auto database = std::make_shared<WriterMemoryDatabase>(mutex);
    database->Attach(raw);
    MsprofFtraceWriter writer(database);
    std::string error;
    ASSERT_TRUE(writer.Begin(error));
    MsprofFtraceSlice slice;
    slice.process = "Process Scheduling";
    slice.threadName = "worker:42";
    slice.name = "Running";
    slice.timestampNs = EPOCH;
    slice.durationNs = 10;
    slice.args = "{}";
    slice.threadKey = "tid:42:1";
    ASSERT_TRUE(writer.AddSlice(slice, error));
    slice.threadKey = "tid:42:2";
    slice.timestampNs += 20;
    ASSERT_TRUE(writer.AddSlice(slice, error));
    ASSERT_TRUE(writer.Finish(error)) << error;
    sqlite3_stmt *query = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(raw,
                  "SELECT COUNT(*),COUNT(DISTINCT tid),COUNT(DISTINCT thread_name) FROM thread "
                  "WHERE pid='Process Scheduling'",
                  -1, &query, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_step(query), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(query, 0), 2);
    EXPECT_EQ(sqlite3_column_int(query, 1), 2);
    EXPECT_EQ(sqlite3_column_int(query, 2), 1);
    sqlite3_finalize(query);
}

}
}
