/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include "TimelineProtocol.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "TimelineProtocolUtil.h"

using namespace Dic;
using namespace Dic::Protocol;

TEST(PmuCounterProtocolTest, ParsesExactCounterIdentityAndRejectsInvalidRange) {
    TimelineProtocol protocol;
    protocol.Register();
    std::string error;
    document_t json;
    json.Parse(R"({"id":1,"moduleName":"timeline","type":"request","command":"unit/pmu-counter","params":{
        "dbPath":"registered.db","rankId":"rank0","counterId":"9007199254740993",
        "startTime":10,"endTime":200,"targetBuckets":512}})");
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    const auto *pmu = dynamic_cast<PmuCounterRequest *>(request.get());
    ASSERT_NE(pmu, nullptr);
    EXPECT_EQ(pmu->params.counterId, "9007199254740993");
    EXPECT_EQ(pmu->params.targetBuckets, 512u);
    json["params"]["endTime"].SetUint64(9007199254740992ULL);
    EXPECT_EQ(protocol.FromJson(json, error), nullptr);
    json["params"]["endTime"].SetUint64(200);
    json["params"]["counterId"].SetInt(1);
    EXPECT_EQ(protocol.FromJson(json, error), nullptr);
}

TEST(PmuCounterProtocolTest, RejectsOverflowAndUnboundedBudget) {
    PmuCounterParams params;
    params.dbPath = "db";
    params.rankId = "rank";
    params.counterId = "9223372036854775808";
    std::string error;
    EXPECT_FALSE(params.CheckParams(0, error));
    params.counterId = "1";
    params.targetBuckets = 4097;
    EXPECT_FALSE(params.CheckParams(0, error));
    params.targetBuckets = 1024;
    params.endTime = 10;
    EXPECT_FALSE(params.CheckParams(9223372036854775800ULL, error));
}

TEST(PmuCounterProtocolTest, SerializesOnlyIntervalValuesAndNullGaps) {
    PmuCounterResponse response;
    response.body.counterId = "9007199254740993";
    response.body.status = "ok";
    Dic::Module::Timeline::PmuCounterSegment gap;
    gap.startTime = 10;
    gap.endTime = 20;
    response.body.segments.push_back(gap);
    response.body.segments.push_back({20, 30, 0.0});
    auto json = ToResponseJson(response);
    ASSERT_TRUE(json.has_value());
    const auto &body = (*json)["body"];
    EXPECT_EQ(body.MemberCount(), 4u);
    EXPECT_FALSE(body.HasMember("data"));
    EXPECT_STREQ(body["counterId"].GetString(), "9007199254740993");
    EXPECT_TRUE(body["segments"][0]["value"].IsNull());
    EXPECT_EQ(body["segments"][0].MemberCount(), 3u);
    EXPECT_DOUBLE_EQ(body["segments"][1]["value"].GetDouble(), 0.0);
    EXPECT_FALSE(body.HasMember("mode"));
    EXPECT_FALSE(body.HasMember("points"));
    EXPECT_FALSE(body.HasMember("warnings"));
}

TEST(PmuCounterProtocolTest, SerializesDedicatedMetadataWithoutChangingLegacyCounter) {
    UnitTrack legacy;
    legacy.type = "counter";
    legacy.metaData.metaType = "CPU_USAGE";
    ParseSuccessEvent event;
    event.body.unit.children.emplace_back(std::make_unique<UnitTrack>(std::move(legacy)));
    auto oldJson = ToEventJson(event);
    ASSERT_TRUE(oldJson.has_value());
    EXPECT_FALSE((*oldJson)["body"]["unit"]["children"][0]["metadata"].HasMember("pmu"));
    UnitTrack pmu;
    pmu.type = "counter";
    pmu.metaData.metaType = "PMU_COUNTER";
    pmu.metaData.dbPath = "source.db";
    pmu.metaData.pmu.emplace();
    auto &info = *pmu.metaData.pmu;
    info.kind = "event";
    info.sourceKey = "source.db";
    info.nodeKey = "source.db:pmu:counter:1";
    info.counterId = "1";
    info.eventId = "2";
    info.cpu = 63;
    info.unit = "unknown";
    info.intervalNs = "1000000";
    info.hasSamples = false;
    event.body.unit.children.clear();
    event.body.unit.children.emplace_back(std::make_unique<UnitTrack>(std::move(pmu)));
    auto json = ToEventJson(event);
    ASSERT_TRUE(json.has_value());
    const auto &data = (*json)["body"]["unit"]["children"][0]["metadata"]["pmu"];
    EXPECT_STREQ(data["counterId"].GetString(), "1");
    EXPECT_EQ(data["cpu"].GetInt(), 63);
    EXPECT_FALSE(data["hasSamples"].GetBool());
    EXPECT_FALSE(data.HasMember("supportedModes"));
}
