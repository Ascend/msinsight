/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#include <gtest/gtest.h>
#include <array>
#include "DbTraceDataBase.h"
#include "TextTraceDatabase.h"
#include "TraceTime.h"
#include "../../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::Module::Timeline;

namespace {
template <typename Base> class FrequencyDatabase : public Base {
  public:
    explicit FrequencyDatabase(std::recursive_mutex &mutex) : Base(mutex) {}
    void SetDb(sqlite3 *value) {
        this->db = value;
        this->isOpen = true;
        this->path = ":memory:";
    }
};

// 同一组原始样本和断言同时验证DB、TEXT，避免两套夹具逐项复制。
class AICoreFrequencyStatisticsTest : public ::testing::TestWithParam<bool> {
  protected:
    std::recursive_mutex mutex;
    std::unique_ptr<VirtualTraceDatabase> database;
    sqlite3 *source = nullptr;
    uint64_t shift = 1786610846958523000; // 除零基准用例外，全部DB/TEXT查询同时看护UTC精度。
    struct Sample {
        uint64_t time, frequency;
        int device = 0, die = 0;
    };
    void SetUp() override {
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(1000 + shift, 2000 + shift);
    }
    void TearDown() override { TraceTime::Instance().Reset(); }
    void Execute(const std::string &sql) { DatabaseTestCaseMockUtil::InsertData(source, sql); }
    void Prepare(std::initializer_list<Sample> samples, bool legacy = false) {
        DatabaseTestCaseMockUtil::OpenDB(source);
        if (GetParam()) {
            auto db = std::make_unique<FrequencyDatabase<TextTraceDatabase>>(mutex);
            db->SetDb(source);
            database = std::move(db);
        } else {
            auto db = std::make_unique<FrequencyDatabase<Dic::Module::FullDb::DbTraceDataBase>>(mutex);
            db->SetDb(source);
            database = std::move(db);
        }
        const bool withDie = GetParam() || !legacy;
        DatabaseTestCaseMockUtil::CreateTable(source,
            "CREATE TABLE AICORE_FREQ(timestampNs INTEGER,freq INTEGER,deviceId INTEGER" +
                std::string(withDie ? ",dieId INTEGER);" : ");"));
        for (const auto &sample : samples) {
            Execute("INSERT INTO AICORE_FREQ VALUES(" + std::to_string(sample.time + shift) + "," +
                std::to_string(sample.frequency) + "," + std::to_string(sample.device) +
                (withDie ? "," + std::to_string(sample.die) : "") + ");");
        }
        if (GetParam()) {
            Execute("CREATE TABLE counter(timestamp INTEGER,args TEXT,pid TEXT,name TEXT);"
                    "INSERT INTO counter SELECT timestampNs,'{\"MHz\":\"'||freq||'\"}',32*(dieId+1)+deviceId,'AI Core "
                    "Freq' FROM AICORE_FREQ;"
                    "CREATE TABLE process AS SELECT DISTINCT pid FROM counter;"
                    "CREATE TABLE thread AS SELECT pid,'AI Core Freq' AS tid FROM process;DROP TABLE AICORE_FREQ;");
        }
    }
    Dic::Protocol::SystemViewAICoreFreqParams Lane(int die = 0, int device = 0, bool legacy = false) {
        Dic::Protocol::SystemViewAICoreFreqParams params;
        params.statisticsOnly = true;
        params.deviceId = std::to_string(device);
        params.laneName = GetParam() || legacy ? "AI Core Freq" : "AI Core Freq Die " + std::to_string(die);
        params.lanePid = std::to_string(32 * (die + 1) + device);
        return params;
    }
    void Check(const Dic::Protocol::SystemViewAICoreFreqParams &params, uint64_t count, double mean, uint64_t low,
        uint64_t high, uint64_t first = 1000, uint64_t last = 1030) {
        Dic::Protocol::ExpAnaAICoreFreqBody body;
        std::vector<std::pair<uint64_t, uint64_t>> points;
        uint64_t maximum = 0, minimum = UINT64_MAX;
        ASSERT_TRUE(database->QueryExpAnaAICoreFreqData(params, body, points, maximum, minimum));
        EXPECT_EQ(body.sampleCount, count);
        EXPECT_DOUBLE_EQ(body.avgFreq, mean);
        EXPECT_EQ(body.minFreq, low);
        EXPECT_EQ(body.maxFreq, high);
        if (params.statisticsOnly) {
            EXPECT_TRUE(points.empty());
        } else {
            ASSERT_EQ(points.size(), count);
            EXPECT_EQ(points.front().first, first + shift);
            EXPECT_EQ(points.back().first, last + shift);
            EXPECT_EQ(maximum, high);
            EXPECT_EQ(minimum, low);
        }
    }
};

TEST_P(AICoreFrequencyStatisticsTest, FullRangeIdentityAndLegacy) {
    Prepare({{1000, 500}, {1010, 1000}, {1020, 1500, 0, 1}, {1030, 2000, 0, 1}, {1015, 9000, 1}});
    Dic::Protocol::SystemViewAICoreFreqParams p;
    p.deviceId = "0";
    p.statisticsOnly = p.hasTimeRange = true;
    p.startTime = 10;
    p.endTime = 20;
    Check(p, 1, 1000, 1000, 1500);
    p.startTime = 40;
    p.endTime = 50;
    Check(p, 0, 0, 0, 0);
    p.startTime = p.endTime = 0;
    Check(p, 0, 0, 0, 0); // 多点块状数据的零时长区间无有效持续时间。
    p.hasTimeRange = false;
    Check(p, 3, 1000, 500, 2000);
    p.statisticsOnly = false;
    Check(p, 4, 1250, 500, 2000);
    Check(Lane(), 1, 500, 500, 1000);
    Check(Lane(1), 1, 1500, 1500, 2000);
    if (GetParam()) {
        Execute("INSERT INTO thread VALUES('32','another');INSERT INTO counter "
                "VALUES(1015,'{\"MHz\":\"8000\"}','32','AI Core Freq Die 9');");
        p = Lane();
        p.deviceId = "1"; // TEXT精确进程查询不受工程级设备号影响，也不因线程表重复而增样。
        Check(p, 1, 500, 500, 1000);
    }
}

TEST_P(AICoreFrequencyStatisticsTest, SampleSpansAndPixelBoundaries) {
    Prepare({{1020, 1500, 0, 1}, {1030, 2000, 0, 1}, {1025, 9000}});
    auto p = Lane(1);
    p.hasTimeRange = true;
    p.viewStartTime = 1;
    p.viewEndTime = 101;
    p.chartWidth = 20;
    struct Case {
        uint64_t start, end, count;
        double mean;
        uint64_t low, high;
    };
    for (const auto &c : std::vector<Case>{{16, 19, 0, 0, 0, 0}, {21, 25, 1, 1500, 1500, 1500},
             {25, 30, 1, 1500, 1500, 2000}, {30, 30, 0, 0, 0, 0}, {27, 29, 1, 1500, 1500, 1500}, {31, 31, 0, 0, 0, 0},
             {32, 34, 0, 0, 0, 0}, {1, 5, 0, 0, 0, 0}}) {
        SCOPED_TRACE(std::to_string(c.start) + "-" + std::to_string(c.end));
        p.startTime = c.start;
        p.endTime = c.end;
        Check(p, c.count, c.mean, c.low, c.high);
    }
}

TEST_P(AICoreFrequencyStatisticsTest, DenseTailPreservesRawSamples) {
    Prepare({{1092, 1800}, {1098, 1000}, {1100, 2500}, {1095, 9000, 1}});
    auto p = Lane();
    p.hasTimeRange = p.hasCounterRange = true;
    p.counterStartTime = 90;
    p.counterEndTime = 100;
    p.viewStartTime = 0;
    p.viewEndTime = 1000;
    p.chartWidth = 100;
    p.startTime = 91;
    p.endTime = 99;
    Check(p, 2, 11800.0 / 7, 1000, 1800); // 仅覆盖92—99：1800持续6，1000持续1。
    p.endTime = 93;
    Check(p, 1, 1800, 1800, 1800);
}

TEST_P(AICoreFrequencyStatisticsTest, InvisibleSourcePointsAndZeroOrigin) {
    Prepare({{990, 1650, 1}, {1200, 1650, 1}, {1200, 1650, 1}, {1600, 1800, 1}}, true);
    auto p = Lane(0, 1, true);
    p.hasCounterRange = true;
    p.counterEndTime = 500;
    p.viewStartTime = 0;
    p.viewEndTime = 1000;
    p.chartWidth = 100;
    Check(p, 1, 1650, 1650, 1650);
    p.hasTimeRange = true;
    for (const auto &c : std::vector<std::array<uint64_t, 3>>{{50, 100, 0}, {195, 199, 1}, {201, 205, 0}}) {
        p.startTime = c[0];
        p.endTime = c[1];
        Check(p, c[2], c[2] ? 1650 : 0, c[2] ? 1650 : 0, c[2] ? 1650 : 0);
    }
    p.statisticsOnly = p.hasTimeRange = false;
    p.startTime = p.endTime = 0;
    p.laneName.clear();
    Check(p, 4, 1687.5, 1650, 1800, 990, 1600); // 旧专家路径保留重复记录和原算术字段。
}

TEST_P(AICoreFrequencyStatisticsTest, WeightsDurationsAndExcludesBlankTime) {
    Prepare({{1010, 1800}, {1020, 800}, {1020, 800}, {1050, 1800}, {1010, 0, 1}, {1020, 1800, 1}});
    auto p = Lane();
    Check(p, 2, 1050, 800, 1800); // 10×1800+30×800，末点无后继不补时长。
    p.hasTimeRange = true;
    p.startTime = 0;
    p.endTime = 100;
    Check(p, 2, 1050, 800, 1800); // 前10、后50的空白剔除，重复时间戳不重复计时。
    p.viewStartTime = 1;
    p.viewEndTime = 101;
    p.chartWidth = 20;
    Check(p, 2, 1050, 800, 1800); // 多点时间加权不受像素宽度影响。
    p.startTime = 15;
    p.endTime = 40;
    Check(p, 2, 1000, 800, 1800);
    p.startTime = 21;
    p.endTime = 39;
    Check(p, 1, 800, 800, 800);
    p.startTime = 0;
    p.endTime = 9;
    Check(p, 0, 0, 0, 0);
    p.startTime = 51;
    p.endTime = 100;
    Check(p, 0, 0, 0, 0);
    Check(Lane(0, 1), 1, 0, 0, 1800); // 有效0MHz不是空白；极值仍含原范围终点样本。
}

TEST_P(AICoreFrequencyStatisticsTest, ZeroTimestampWithValidTraceTime) {
    shift = 0;
    TraceTime::Instance().Reset();
    TraceTime::Instance().UpdateCardTimeDuration("aicore-freq-zero", 0, 10);
    ASSERT_EQ(TraceTime::Instance().GetStartTime(), 0);
    Prepare({{0, 500}, {10, 1000}}, true);
    auto p = Lane(0, 0, true);
    p.hasTimeRange = true;
    p.startTime = 1;
    p.endTime = 9;
    Check(p, 1, 500, 500, 500);
}

INSTANTIATE_TEST_SUITE_P(Formats, AICoreFrequencyStatisticsTest, ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<bool> &info) { return info.param ? "Text" : "Db"; });

}
