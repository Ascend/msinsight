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

#include "RLMicroBatchMegatronClassifier.h"
#include "RLMicroBatchFSDPClassifier.h"
#include "gtest/gtest.h"

using namespace Dic::Module::RL;

class MicroBatchTest : public testing::Test {
  protected:
    class MicroBatchMegaTronTest : public RLMicroBatchMegatronClassifier {
      public:
        std::vector<RLPipelineNode> Classifier(std::vector<RLPipelineNode> &items) {
            return RLMicroBatchMegatronClassifier::MicroBatchClassifier(items);
        }
    };

    class MicroBatchFSDPTest : public RLMicroBatchFSDPClassifier {
      public:
        std::vector<RLPipelineNode> Classifier(std::vector<RLPipelineNode> &items) {
            return RLMicroBatchFSDPClassifier::MicroBatchClassifier(items);
        }

        std::vector<RLPipelineNode> NodeSortMerge(
            const std::vector<RLPipelineNode> &left, const std::vector<RLPipelineNode> &right) {
            return RLMicroBatchFSDPClassifier::NodeSortMerge(left, right);
        }
    };
    std::vector<RLPipelineNode> generatorOneByOneData() {
        std::vector<RLPipelineNode> res;
        RLPipelineNode fp1{"", "FP", 0, 10, "transformerBlock", "rollout"}; // set [0,10]
        res.push_back(fp1);
        RLPipelineNode bp1{"", "BP", 12, 10, "transformerLayer", "rollout"}; // set [12, 22]
        res.push_back(bp1);
        RLPipelineNode fp2{"", "FP", 24, 10, "transformerBlock", "rollout"}; // set [24, 34]
        res.push_back(fp2);
        RLPipelineNode bp2{"", "BP", 35, 2, "transformerLayer", "rollout"}; // set [35 , 37]
        res.push_back(bp2);
        return res;
    }

    std::vector<RLPipelineNode> generatorData1() {
        std::vector<RLPipelineNode> res;
        RLPipelineNode fp1{"", "FP", 0, 50, "transformerBlock", "rollout"}; // set [0, 50]
        res.push_back(fp1);
        RLPipelineNode bp1{"", "BP", 1, 2, "transformerLayer", "rollout"}; // set [1, 2]
        res.push_back(bp1);
        RLPipelineNode bp2{"", "BP", 4, 4, "transformerLayer", "rollout"}; // set [4, 4]
        res.push_back(bp2);
        RLPipelineNode fp2{"", "FP", 51, 10, "transformerBlock", "rollout"}; // set [51, 61]
        res.push_back(fp2);
        RLPipelineNode bp3{"", "BP", 62, 2, "transformerLayer", "rollout"}; // set [62, 64]
        res.push_back(bp3);
        RLPipelineNode bp4{"", "BP", 65, 3, "transformerLayer", "rollout"}; // set [65, 68]
        res.push_back(bp4);
        RLPipelineNode bp5{"", "BP", 69, 10, "transformerLayer", "rollout"}; // set [69, 10]
        res.push_back(bp5);
        return res;
    }

    std::vector<RLPipelineNode> generatorFPOverData() {
        std::vector<RLPipelineNode> res;
        RLPipelineNode fp1{"", "FP", 0, 10, "transformerBlock", "rollout"}; // set [0,10]
        res.push_back(fp1);
        RLPipelineNode bp1{"", "FP", 2, 8, "transformerLayer", "rollout"}; // set [12, 22]
        res.push_back(bp1);
        return res;
    }
};

TEST_F(MicroBatchTest, normalOnebyOne) {
    auto originalData = generatorOneByOneData();
    MicroBatchMegaTronTest classifier;
    auto res = classifier.Classifier(originalData);
    EXPECT_EQ(res.size(), 4); // expect size 4
    auto node1 = res[0];
    EXPECT_EQ(node1.nodeType, "FP");
    EXPECT_EQ(node1.startTime, 0);
    EXPECT_EQ(node1.duration, 10); // expect duration 10
    auto node2 = res[1]; // get 1
    EXPECT_EQ(node2.nodeType, "BP");
    EXPECT_EQ(node2.startTime, 12); // expect startTime 12
    EXPECT_EQ(node2.duration, 10); // expect duration 10
    auto node3 = res[2]; // get 2
    EXPECT_EQ(node3.nodeType, "FP");
    EXPECT_EQ(node3.startTime, 24); // expect startTime 24
    EXPECT_EQ(node3.duration, 10); // expect duration 10
    auto node4 = res[3]; // get 3
    EXPECT_EQ(node4.nodeType, "BP");
    EXPECT_EQ(node4.startTime, 35); // expect start time 35
    EXPECT_EQ(node4.duration, 2); // expect duration 2
}

TEST_F(MicroBatchTest, timeCover) {
    auto original = generatorData1();
    MicroBatchMegaTronTest classifier;
    auto res = classifier.Classifier(original);
    EXPECT_EQ(res.size(), 4); // expect size 4
    auto node1 = res[0];
    EXPECT_EQ(node1.nodeType, "FP");
    EXPECT_EQ(node1.startTime, 0);
    EXPECT_EQ(node1.duration, 50); // expect duration is 50ns
    auto node2 = res[1];
    EXPECT_EQ(node2.nodeType, "FP");
    EXPECT_EQ(node2.startTime, 51); // expect start time 51ns
    EXPECT_EQ(node2.duration, 10); // expect duration 10ns
    auto node3 = res[2]; // get 2
    EXPECT_EQ(node3.nodeType, "BP");
    EXPECT_EQ(node3.startTime, 62); // expect start time is 62ns
    EXPECT_EQ(node3.duration, 6); // expect duration time 6ns
    auto node4 = res[3]; // get 3
    EXPECT_EQ(node4.nodeType, "BP");
    EXPECT_EQ(node4.startTime, 69); // expect start time 69
    EXPECT_EQ(node4.duration, 10); // expect duration 10
}

TEST_F(MicroBatchTest, fsdp_normalOnebyOne) {
    auto originalData = generatorOneByOneData();
    MicroBatchFSDPTest classifier;
    auto res = classifier.Classifier(originalData);
    EXPECT_EQ(res.size(), 4); // expect size 4
    auto node1 = res[0];
    EXPECT_EQ(node1.nodeType, "FP");
    EXPECT_EQ(node1.startTime, 0);
    EXPECT_EQ(node1.duration, 10); // expect duration 10
    auto node2 = res[1]; // get 1
    EXPECT_EQ(node2.nodeType, "BP");
    EXPECT_EQ(node2.startTime, 12); // expect startTime 12
    EXPECT_EQ(node2.duration, 10); // expect duration 10
    auto node3 = res[2]; // get 2
    EXPECT_EQ(node3.nodeType, "FP");
    EXPECT_EQ(node3.startTime, 24); // expect startTime 24
    EXPECT_EQ(node3.duration, 10); // expect duration 10
    auto node4 = res[3]; // get 3
    EXPECT_EQ(node4.nodeType, "BP");
    EXPECT_EQ(node4.startTime, 35); // expect start time 35
    EXPECT_EQ(node4.duration, 2); // expect duration 2
}

TEST_F(MicroBatchTest, fsdp_timeCover) {
    auto original = generatorData1();
    MicroBatchFSDPTest classifier;
    auto res = classifier.Classifier(original);
    EXPECT_EQ(res.size(), 4); // expect size 4
    auto node1 = res[0];
    EXPECT_EQ(node1.nodeType, "FP");
    EXPECT_EQ(node1.startTime, 0);
    EXPECT_EQ(node1.duration, 50); // expect duration is 50ns
    auto node2 = res[1];
    EXPECT_EQ(node2.nodeType, "BP");
    EXPECT_EQ(node2.startTime, 1); // expect start time 1ns
    EXPECT_EQ(node2.duration, 7); // expect duration 10ns
    auto node3 = res[2]; // get 2
    EXPECT_EQ(node3.nodeType, "FP");
    EXPECT_EQ(node3.startTime, 51); // expect start time is 62ns
    EXPECT_EQ(node3.duration, 10); // expect duration time 6ns
    auto node4 = res[3]; // get 3
    EXPECT_EQ(node4.nodeType, "BP");
    EXPECT_EQ(node4.startTime, 62); // expect start time 69
    EXPECT_EQ(node4.duration, 17); // expect duration 17
}

TEST_F(MicroBatchTest, fsdp_fp_timer_cover) {
    auto original = generatorFPOverData();
    MicroBatchFSDPTest classifier;
    auto res = classifier.Classifier(original);
    EXPECT_EQ(res.size(), 1); // expect size 1
    EXPECT_EQ(res[0].startTime, 0);
    EXPECT_EQ(res[0].duration, 10); // expect dur 10
}

TEST_F(MicroBatchTest, fsdp_node_sort_merge) {
    auto left = generatorData1();
    auto right = generatorFPOverData();
    MicroBatchFSDPTest classifier;
    auto res = classifier.NodeSortMerge(left, right);
    EXPECT_EQ(res.size(), 9); // expect  get 9
}
