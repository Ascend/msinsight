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
#include <gtest/gtest.h>
#include <string>
#include "TrackInfoManager.h"

using namespace Dic::Module::Timeline;
class TrackInfoManagerTest : public ::testing::Test {};

/**
 * 测试Text场景首次导入
 */
TEST_F(TrackInfoManagerTest, TestGetTrackIdFirst) {
    TrackInfoManager::Instance().Reset();
    uint64_t first = TrackInfoManager::Instance().GetTrackId("gggg", "ppp", "ttt");
    uint64_t second = TrackInfoManager::Instance().GetTrackId("gggg", "ppp", "ttt");
    TrackInfo trackInfo;
    TrackInfoManager::Instance().GetTrackInfo(second, trackInfo, "gggg");
    EXPECT_EQ(first, second);
    EXPECT_EQ(trackInfo.processId, "ppp");
    EXPECT_EQ(trackInfo.threadId, "ttt");
    EXPECT_EQ(trackInfo.host.empty(), true);
    EXPECT_EQ(trackInfo.cardId, "gggg");
    EXPECT_EQ(trackInfo.rankId, "gggg");
    EXPECT_EQ(trackInfo.deviceId, "gggg");
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试TrackId不存在
 */
TEST_F(TrackInfoManagerTest, TestTrackIdNotExist) {
    TrackInfoManager::Instance().Reset();
    TrackInfo trackInfo2;
    uint64_t notExistTrackId = 999999;
    bool result = TrackInfoManager::Instance().GetTrackInfo(notExistTrackId, trackInfo2, "");
    EXPECT_EQ(result, false);
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试Text场景二次导入
 */
TEST_F(TrackInfoManagerTest, TestSecondImportScene) {
    TrackInfoManager::Instance().Reset();
    std::map<uint64_t, std::pair<std::string, std::string>> threadMap;
    std::pair<std::string, std::string> p("kkkk", "Hello");
    uint64_t expectTrackId = 3;
    threadMap.emplace(expectTrackId, p);
    TrackInfoManager::Instance().UpdateTrackIdMap("7788", threadMap);
    TrackInfo trackInfo;
    bool result = TrackInfoManager::Instance().GetTrackInfo(expectTrackId, trackInfo, "7788");
    EXPECT_EQ(result, true);
    EXPECT_EQ(trackInfo.processId, "Hello");
    EXPECT_EQ(trackInfo.threadId, "kkkk");
    EXPECT_EQ(trackInfo.host.empty(), true);
    EXPECT_EQ(trackInfo.cardId, "7788");
    EXPECT_EQ(trackInfo.rankId, "7788");
    EXPECT_EQ(trackInfo.deviceId, "7788");
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试DB场景host存在但devicemap不存在
 */
TEST_F(TrackInfoManagerTest, TestHostIsExistAndDeviceMapNotExist) {
    TrackInfoManager::Instance().Reset();
    const std::string cardId = "hhhhhhhhhhh 9988";
    TrackInfoManager::Instance().UpdateHost(cardId, "hhhhhhhhhhh ");
    uint64_t expectTrackId = TrackInfoManager::Instance().GetTrackId(cardId, "ppp", "ttt");
    TrackInfo trackInfo;
    bool result = TrackInfoManager::Instance().GetTrackInfo(expectTrackId, trackInfo, cardId);
    EXPECT_EQ(result, true);
    EXPECT_EQ(trackInfo.processId, "ppp");
    EXPECT_EQ(trackInfo.threadId, "ttt");
    EXPECT_EQ(trackInfo.host, "hhhhhhhhhhh ");
    EXPECT_EQ(trackInfo.cardId, cardId);
    EXPECT_EQ(trackInfo.rankId, "9988");
    EXPECT_EQ(trackInfo.deviceId, "9988");
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试DB场景host和devicemap都存在
 */
TEST_F(TrackInfoManagerTest, TestHostAndDeviceMapExist) {
    TrackInfoManager::Instance().Reset();
    const std::string cardId = "hhhhhhhhhhh 9988";
    TrackInfoManager::Instance().UpdateHost(cardId, "hhhhhhhhhhh ");
    std::unordered_map<std::string, std::string> deviceMap;
    deviceMap = {{"9988", "1122"}};
    TrackInfoManager::Instance().UpdateDeviceMap(cardId, deviceMap);
    uint64_t expectTrackId = TrackInfoManager::Instance().GetTrackId(cardId, "ppp", "ttt");
    TrackInfo trackInfo;
    bool result = TrackInfoManager::Instance().GetTrackInfo(expectTrackId, trackInfo, cardId);
    EXPECT_EQ(result, true);
    EXPECT_EQ(trackInfo.processId, "ppp");
    EXPECT_EQ(trackInfo.threadId, "ttt");
    EXPECT_EQ(trackInfo.host, "hhhhhhhhhhh ");
    EXPECT_EQ(trackInfo.cardId, cardId);
    EXPECT_EQ(trackInfo.rankId, "9988");
    EXPECT_EQ(trackInfo.deviceId, "1122");
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试DB场景host和devicemap都存在,但deviceMap里rankId不存在
 */
TEST_F(TrackInfoManagerTest, TestHostAndDeviceMapExistAndRankIdNotExist) {
    TrackInfoManager::Instance().Reset();
    const std::string cardId = "hhhhhhhhhhh 9988";
    TrackInfoManager::Instance().UpdateHost(cardId, "hhhhhhhhhhh ");
    std::unordered_map<std::string, std::string> deviceMap;
    deviceMap = {{"9987", "1122"}};
    TrackInfoManager::Instance().UpdateDeviceMap(cardId, deviceMap);
    uint64_t expectTrackId = TrackInfoManager::Instance().GetTrackId(cardId, "ppp", "ttt");
    TrackInfo trackInfo;
    bool result = TrackInfoManager::Instance().GetTrackInfo(expectTrackId, trackInfo, cardId);
    EXPECT_EQ(result, true);
    EXPECT_EQ(trackInfo.processId, "ppp");
    EXPECT_EQ(trackInfo.threadId, "ttt");
    EXPECT_EQ(trackInfo.host, "hhhhhhhhhhh ");
    EXPECT_EQ(trackInfo.cardId, cardId);
    EXPECT_EQ(trackInfo.rankId, "9988");
    EXPECT_EQ(trackInfo.deviceId, "9988");
    TrackInfoManager::Instance().Reset();
}
