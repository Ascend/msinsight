/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

#include <gtest/gtest.h>

#include <tuple>
#include <vector>

#include "NumaMetricConverter.h"

namespace Dic::Module::Numa {
namespace {
NumaMetricRecord Record(const std::string &name, const std::string &unit = "GOps/s") {
    return {0, 0, "", "", name, name + " description", unit, true, 2, 2.5, 0, 1000000000};
}
} // namespace

TEST(NumaMetricConverterTest, MapsOnlyExactDatabaseMetricNames) {
    const std::vector<std::tuple<std::string, std::string, std::string>> cases = {
        {"External Traffic Impact", "externalImpact", "External Traffic Impact"},
        {"Total External Traffic Impact", "totalExternalImpact", "Total External Traffic Impact"},
        {"Cross Socket Read Bandwidth", "crossSocketRead", "Cross Socket Read Traffic"},
        {"Cross SCCL Read Bandwidth", "crossScclRead", "Cross SCCL Read Traffic"},
        {"Total Read Bandwidth", "totalRead", "Total Read Traffic"},
        {"Inner Read Bandwidth", "innerRead", "Inner Read Traffic"},
        {"DRAM Read Bandwidth", "dramRead", "DRAM Read Traffic"},
        {"DRAM Write Bandwidth", "dramWrite", "DRAM Write Traffic"},
        {"LLC Bandwidth", "llcTraffic", "LLC Traffic"},
    };
    for (const auto &[name, key, label] : cases) {
        const auto metric = ConvertNumaMetric(Record(name));
        EXPECT_EQ(metric.key, key);
        EXPECT_EQ(metric.label, label);
        EXPECT_EQ(metric.unit, "GOps");
    }

    EXPECT_EQ(ConvertNumaMetric(Record("Cross Die Read")).key, "crossdieread");
}

TEST(NumaMetricConverterTest, IntegratesRateSamplesAndRejectsUnavailableOnes) {
    auto record = Record("DRAM Read Bandwidth", "GB/s");
    record.value = 6.0;
    record.sampleCount = 3;
    record.minTimestamp = 100000000;
    record.maxTimestamp = 300000000;
    const auto integrated = ConvertNumaMetric(record);
    EXPECT_EQ(integrated.unit, "GB");
    EXPECT_DOUBLE_EQ(integrated.value, 0.6);

    record.hasValue = false;
    EXPECT_FALSE(ConvertNumaMetric(record).hasValue);
    record.hasValue = true;
    record.sampleCount = 1;
    record.minTimestamp = record.maxTimestamp;
    const auto singleSample = ConvertNumaMetric(record);
    EXPECT_FALSE(singleSample.hasValue);
    EXPECT_DOUBLE_EQ(singleSample.value, 0.0);
}

} // namespace Dic::Module::Numa
