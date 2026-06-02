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
#include "PacketAnalyzer.h"
#include "ByteAlignmentAnalyzer.h"
#include "BandwidthContentionAnalyzer.h"
#include "RetransmissionAnalyzer.h"

namespace Dic {
namespace Module {
namespace Communication {
class PacketAnalyzerInheritance : public PacketAnalyzer {
  public:
    void SetData(const std::vector<PacketAnalyzerData> &inputData) { data = inputData; }
    void SetStatistics(const PacketAnalyzerStatistics &inputStatistics) {
        statistics.smallSdmaProportion = inputStatistics.smallSdmaProportion;
        statistics.smallSdmaDuration = inputStatistics.smallSdmaDuration;
        statistics.sdmaIssue = inputStatistics.sdmaIssue;
        statistics.smallRdmaProportion = inputStatistics.smallRdmaProportion;
        statistics.smallRdmaDuration = inputStatistics.smallRdmaDuration;
        statistics.rdmaIssue = inputStatistics.rdmaIssue;
    }
    PacketAnalyzerStatistics GetStatistics() { return statistics; }
};

class ByteAlignmentAnalyzerInheritance : public ByteAlignmentAnalyzer {
  public:
    void SetData(const std::map<std::string, std::vector<CommunicationLargeOperatorInfo>> &inputData) {
        data = inputData;
    }
    void SetStatistics(const std::vector<ByteAlignmentAnalyzerStatistics> &inputStatistics) {
        statistics = inputStatistics;
    }
    std::vector<ByteAlignmentAnalyzerStatistics> GetStatistics() { return statistics; }
};

class BandwidthContentionAnalyzerInheritance : public BandwidthContentionAnalyzer {
  public:
    void SetData(const BandwidthContentionData &inputData) { data = inputData; }
    void SetStatistics(const std::vector<BandwidthContentionAnalyzerStatistics> &inputStatistics) {
        statistics = inputStatistics;
    }
    std::vector<BandwidthContentionAnalyzerStatistics> GetStatistics() { return statistics; }
};

class RetransmissionAnalyzerInheritance : public RetransmissionAnalyzer {
  public:
    void SetData(const std::vector<RetransmissionClassificationInfo> &inputData) { data = inputData; }
    void SetStatistics(const std::vector<RetransmissionAnalyzerStatistics> &inputStatistics) {
        statistics = inputStatistics;
    }
    std::vector<RetransmissionAnalyzerStatistics> GetStatistics() { return statistics; }
};

class CommunicationAnalyzerTest : public testing::Test {};

TEST_F(CommunicationAnalyzerTest, PacketAnalyzerComputeStatisticsTest) {
    PacketAnalyzerInheritance analyzer;
    const std::vector<PacketAnalyzerData> inputData = {{"SDMA", 15.0, 2.0}, {"SDMA", 17.0, 1.0}, {"SDMA", 1.0, 5.0},
        {"SDMA", 100.0, 3.0}, {"RDMA", 0.2, 2.0}, {"RDMA", 1.2, 2.0}, {"RDMA", 0.9, 5.0}, {"RDMA", 100.1, 10.0}};
    analyzer.SetData(inputData);
    analyzer.ComputeStatistics();
    PacketAnalyzerStatistics statistics = analyzer.GetStatistics();
    EXPECT_EQ(statistics.sdmaCount, 4); // 4
    EXPECT_EQ(statistics.smallSdmaCount, 2); // 2
    EXPECT_EQ(statistics.smallSdmaProportion, 0.5); // 0.5
    EXPECT_EQ(statistics.smallSdmaDuration, 7.0); // 7.0
    EXPECT_EQ(statistics.sdmaIssue, true);
    EXPECT_EQ(statistics.rdmaCount, 4); // 4
    EXPECT_EQ(statistics.smallRdmaCount, 2); // 2
    EXPECT_EQ(statistics.smallRdmaProportion, 0.5); // 0.5
    EXPECT_EQ(statistics.smallRdmaDuration, 7.0); // 7.0
    EXPECT_EQ(statistics.rdmaIssue, true);
}

TEST_F(CommunicationAnalyzerTest, PacketAnalyzerAssembleAdvisorTest) {
    PacketAnalyzerInheritance analyzer;
    PacketAnalyzerStatistics inputStatistics;
    inputStatistics.smallSdmaProportion = 0.5; // 0.5
    inputStatistics.smallSdmaDuration = 23.0; // 23.0
    inputStatistics.sdmaIssue = true;
    inputStatistics.smallRdmaProportion = 0.1; // 0.1
    inputStatistics.smallRdmaDuration = 1.2; // 1.2
    inputStatistics.rdmaIssue = false;
    analyzer.SetStatistics(inputStatistics);
    CommunicationAdvisorInfo advisor;
    analyzer.AssembleAdvisor(advisor);
    EXPECT_EQ(advisor.name, "Packet Analysis");
    ASSERT_EQ(advisor.statistics["Category"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["Category"][0], "SDMA");
    EXPECT_EQ(advisor.statistics["Category"][1], "RDMA");
    ASSERT_EQ(advisor.statistics["Small Size Standard(MB)"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["Small Size Standard(MB)"][0], "16.000000");
    EXPECT_EQ(advisor.statistics["Small Size Standard(MB)"][1], "1.000000");
    ASSERT_EQ(advisor.statistics["Small Size Proportion Standard(%)"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["Small Size Proportion Standard(%)"][0], "20.000000");
    EXPECT_EQ(advisor.statistics["Small Size Proportion Standard(%)"][1], "20.000000");
    ASSERT_EQ(advisor.statistics["Small Size Proportion(%)"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["Small Size Proportion(%)"][0], "50.000000");
    EXPECT_EQ(advisor.statistics["Small Size Proportion(%)"][1], "10.000000");
    ASSERT_EQ(advisor.statistics["Small Size Duration(ms)"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["Small Size Duration(ms)"][0], "23.000000");
    EXPECT_EQ(advisor.statistics["Small Size Duration(ms)"][1], "1.200000");
    ASSERT_EQ(advisor.statistics["Issue"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["Issue"][0], "Yes");
    EXPECT_EQ(advisor.statistics["Issue"][1], "No");
}

TEST_F(CommunicationAnalyzerTest, ByteAlignmentAnalyzerComputeStatisticsTest) {
    ByteAlignmentAnalyzerInheritance analyzer;
    CommunicationLargeOperatorInfo info1{"hcom_allGather_1",
        {{790, "RDMA", "HCCS"}, {9247, "SDMA", "ON_CHIP"}, {256, "SDMA", "HCCS"}, {1024, "SDMA", "HCCS"},
            {1026, "SDMA", "HCCS"}},
        {{790, "RDMA", "HCCS"}, {9247, "SDMA", "ON_CHIP"}, {256, "SDMA", "HCCS"}, {1024, "SDMA", "HCCS"},
            {1026, "SDMA", "HCCS"}}};
    CommunicationLargeOperatorInfo info2{"hcom_allReduce_1",
        {{790, "RDMA", "HCCS"}, {9247, "SDMA", "ON_CHIP"}, {256, "SDMA", "HCCS"}, {1024, "SDMA", "HCCS"}},
        {{790, "RDMA", "HCCS"}, {9247, "SDMA", "ON_CHIP"}, {256, "SDMA", "HCCS"}, {1024, "SDMA", "HCCS"}}};
    CommunicationLargeOperatorInfo info3{"hcom_allGather_2",
        {{790, "RDMA", "HCCS"}, {9247, "SDMA", "ON_CHIP"}, {256, "SDMA", "HCCS"}, {1024, "SDMA", "HCCS"},
            {1026, "SDMA", "HCCS"}},
        {{790, "RDMA", "HCCS"}, {9247, "SDMA", "ON_CHIP"}, {256, "SDMA", "HCCS"}, {1024, "SDMA", "HCCS"}}};
    std::map<std::string, std::vector<CommunicationLargeOperatorInfo>> inputData;
    inputData["0"].emplace_back(info1);
    inputData["0"].emplace_back(info2);
    inputData["0"].emplace_back(info3);
    analyzer.SetData(inputData);
    analyzer.ComputeStatistics();
    std::vector<ByteAlignmentAnalyzerStatistics> statistics = analyzer.GetStatistics();
    ASSERT_EQ(statistics.size(), 2); // 2
    EXPECT_EQ(statistics[0].name, "hcom_allGather_1");
    EXPECT_EQ(statistics[1].name, "hcom_allGather_2");
}

TEST_F(CommunicationAnalyzerTest, ByteAlignmentAnalyzerAssembleAdvisorTest) {
    ByteAlignmentAnalyzerInheritance analyzer;
    std::vector<ByteAlignmentAnalyzerStatistics> inputStatistics;
    analyzer.SetStatistics(inputStatistics);
    CommunicationAdvisorInfo advisor;
    analyzer.AssembleAdvisor(advisor);
    EXPECT_EQ(advisor.name, "Byte Alignment Analysis");
    ASSERT_NE(advisor.statistics.find("name"), advisor.statistics.end());
    ASSERT_NE(advisor.statistics.find("rankId"), advisor.statistics.end());

    ByteAlignmentAnalyzerStatistics item1{"0", "hcom_allReduce_1"};
    ByteAlignmentAnalyzerStatistics item2{"1", "hcom_allReduce_15"};
    inputStatistics = {item1, item2};
    analyzer.SetStatistics(inputStatistics);
    advisor.name.clear();
    advisor.statistics.clear();
    analyzer.AssembleAdvisor(advisor);
    EXPECT_EQ(advisor.name, "Byte Alignment Analysis");
    ASSERT_EQ(advisor.statistics["rankId"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["rankId"][0], "0");
    EXPECT_EQ(advisor.statistics["rankId"][1], "1");
    ASSERT_EQ(advisor.statistics["name"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["name"][0], "hcom_allReduce_1");
    EXPECT_EQ(advisor.statistics["name"][1], "hcom_allReduce_15");
}

TEST_F(CommunicationAnalyzerTest, BandwidthContentionAnalyzerComputeStatisticsTest) {
    BandwidthContentionAnalyzerInheritance analyzer;
    BandwidthContentionMatMulInfo matmul1{"Matmul1", 100.0, 24.0};
    BandwidthContentionMatMulInfo matmul2{"Matmul2", 120.0, 20.0};
    BandwidthContentionMatMulInfo matmul3{"Matmul3", 160.0, 30.0};
    BandwidthContentionMatMulInfo matmul4{"Matmul4", 200.0, 1.0};
    BandwidthContentionMatMulInfo matmul5{"Matmul5", 250.0, 19.0};
    BandwidthContentionSDMAInfo hccl1{"hcom_allGather_1", 95.0, 6.0, 15.5};
    BandwidthContentionSDMAInfo hccl2{"hcom_allGather_2", 121.0, 3.0, 14.2};
    BandwidthContentionSDMAInfo hccl3{"hcom_allGather_3", 145.0, 11.0, 10.0};
    BandwidthContentionSDMAInfo hccl4{"hcom_allGather_4", 200.5, 0.5, 18.0};
    BandwidthContentionSDMAInfo hccl5{"hcom_allGather_5", 240.0, 12.0, 13.8};
    BandwidthContentionData inputData;
    inputData.matMulData["0"].emplace_back(matmul1);
    inputData.SDMAData["0"].emplace_back(hccl1);
    analyzer.SetData(inputData);
    analyzer.ComputeStatistics();
    std::vector<BandwidthContentionAnalyzerStatistics> statistics = analyzer.GetStatistics();
    ASSERT_EQ(statistics.size(), 0);

    inputData.matMulData["0"].clear();
    inputData.SDMAData["0"].clear();
    inputData.matMulData["0"].emplace_back(matmul1);
    inputData.matMulData["0"].emplace_back(matmul2);
    inputData.matMulData["0"].emplace_back(matmul3);
    inputData.matMulData["0"].emplace_back(matmul4);
    inputData.matMulData["0"].emplace_back(matmul5);
    inputData.SDMAData["0"].emplace_back(hccl1);
    inputData.SDMAData["0"].emplace_back(hccl2);
    inputData.SDMAData["0"].emplace_back(hccl3);
    inputData.SDMAData["0"].emplace_back(hccl4);
    inputData.SDMAData["0"].emplace_back(hccl5);
    analyzer.SetData(inputData);
    analyzer.ComputeStatistics();
    statistics = analyzer.GetStatistics();
    ASSERT_EQ(statistics.size(), 2); // 2
    EXPECT_EQ(statistics[0].rankId, "0");
    EXPECT_EQ(statistics[1].rankId, "0");
    EXPECT_EQ(statistics[0].name, "hcom_allGather_2");
    EXPECT_EQ(statistics[1].name, "hcom_allGather_5");
}

TEST_F(CommunicationAnalyzerTest, BandwidthContentionAnalyzerAssembleAdvisorTest) {
    BandwidthContentionAnalyzerInheritance analyzer;
    std::vector<BandwidthContentionAnalyzerStatistics> inputStatistics;
    analyzer.SetStatistics(inputStatistics);
    CommunicationAdvisorInfo advisor;
    analyzer.AssembleAdvisor(advisor);
    EXPECT_EQ(advisor.name, "Bandwidth Contention Analysis");
    ASSERT_NE(advisor.statistics.find("name"), advisor.statistics.end());
    ASSERT_NE(advisor.statistics.find("duration(us)"), advisor.statistics.end());
    ASSERT_NE(advisor.statistics.find("bandwidth(GB/s)"), advisor.statistics.end());

    BandwidthContentionAnalyzerStatistics item1{"0", "hcom_allReduce_1", 2.0, 16.0};
    BandwidthContentionAnalyzerStatistics item2{"0", "hcom_allReduce_2", 19.0, 25.0};
    inputStatistics = {item1, item2};
    analyzer.SetStatistics(inputStatistics);
    advisor.name.clear();
    advisor.statistics.clear();
    analyzer.AssembleAdvisor(advisor);
    EXPECT_EQ(advisor.name, "Bandwidth Contention Analysis");
    ASSERT_EQ(advisor.statistics["rankId"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["rankId"][0], "0");
    EXPECT_EQ(advisor.statistics["rankId"][1], "0");
    ASSERT_EQ(advisor.statistics["name"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["name"][0], "hcom_allReduce_1");
    EXPECT_EQ(advisor.statistics["name"][1], "hcom_allReduce_2");
    ASSERT_EQ(advisor.statistics["duration(us)"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["duration(us)"][0], "2.000000");
    EXPECT_EQ(advisor.statistics["duration(us)"][1], "19.000000");
    ASSERT_EQ(advisor.statistics["bandwidth(GB/s)"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["bandwidth(GB/s)"][0], "16.000000");
    EXPECT_EQ(advisor.statistics["bandwidth(GB/s)"][1], "25.000000");
}

TEST_F(CommunicationAnalyzerTest, RetransmissionAnalyzerComputeStatisticsTest) {
    RetransmissionAnalyzerInheritance analyzer;
    std::vector<RetransmissionClassificationInfo> inputData;
    RetransmissionClassificationInfo info1{"1", "(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_1", 2000.0, 2000.0};
    RetransmissionClassificationInfo info2{"1", "(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_2", 2000.0, 5000.0};
    RetransmissionClassificationInfo info3{"1", "(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_3", 5000.0, 2000.0};
    RetransmissionClassificationInfo info4{"1", "(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_4", 5000.0, 5000.0};
    RetransmissionClassificationInfo info5{"1", "(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_5", 4500.0, 7000.0};
    inputData = {info1, info2, info3, info4, info5};
    analyzer.SetData(inputData);
    analyzer.ComputeStatistics();
    std::vector<RetransmissionAnalyzerStatistics> statistics = analyzer.GetStatistics();
    ASSERT_EQ(statistics.size(), 2); // 2
    EXPECT_EQ(statistics[0].groupName, "(0, 1, 2, 3, 4, 5, 6, 7)");
    EXPECT_EQ(statistics[0].opName, "hcom_allReduce_4");
    EXPECT_EQ(statistics[1].groupName, "(0, 1, 2, 3, 4, 5, 6, 7)");
    EXPECT_EQ(statistics[1].opName, "hcom_allReduce_5");
}

TEST_F(CommunicationAnalyzerTest, RetransmissionAnalyzerAssembleAdvisorTest) {
    RetransmissionAnalyzerInheritance analyzer;
    std::vector<RetransmissionAnalyzerStatistics> inputStatistics = {
        {"(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_1"}, {"(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_allReduce_2"}};
    analyzer.SetStatistics(inputStatistics);
    CommunicationAdvisorInfo advisor;
    analyzer.AssembleAdvisor(advisor);
    EXPECT_EQ(advisor.name, "Communication Retransmission Analysis");
    ASSERT_EQ(advisor.statistics["group name"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["group name"][0], "(0, 1, 2, 3, 4, 5, 6, 7)");
    EXPECT_EQ(advisor.statistics["group name"][1], "(0, 1, 2, 3, 4, 5, 6, 7)");
    ASSERT_EQ(advisor.statistics["name"].size(), 2); // 2
    EXPECT_EQ(advisor.statistics["name"][0], "hcom_allReduce_1");
    EXPECT_EQ(advisor.statistics["name"][1], "hcom_allReduce_2");
}
}
}
}
