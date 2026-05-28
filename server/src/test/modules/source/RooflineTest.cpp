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
#include "RooflineParserImpl.h"
#include "SourceProtocolUtil.h"

using namespace std;
using namespace Dic::Protocol;

class RooflineTest : public ::testing::Test {
  public:
    Dic::Module::Source::RooflineParserImpl praser;
    std::string jsonStr = R"({
    "advice": "latency bound:pipeline caused",
    "multiple_rooflines": [
        {
            "title": "Memory Unit",
            "rooflines": [
                {
                    "bw": "18.44",
                    "bwName": "L1 Read + Write",
                    "computility": "324.40",
                    "computilityName": "Cube_FP100.000000%",
                    "point": [
                        "33.28",
                        "15.38"
                    ],
                    "ratio": "0.047412"
                }
            ]
        }
    ]
})";
};

TEST_F(RooflineTest, Advice) {
    DetailsRooflineBody body;
    auto res = praser.GetDetailsRoofline(jsonStr, body);
    EXPECT_EQ(res, true);
    EXPECT_EQ(body.advice, "latency bound:pipeline caused");
}

TEST_F(RooflineTest, ConvertStrToRooflineData) {
    auto res = praser.ConvertStrToRooflineData(jsonStr);
    EXPECT_EQ(res.has_value(), true);
    auto rooflineGraph = res.value();
    EXPECT_EQ(rooflineGraph.multipleRooflines.size(), 1);
    EXPECT_EQ(rooflineGraph.multipleRooflines[0].title, "Memory Unit");
    EXPECT_EQ(rooflineGraph.multipleRooflines[0].rooflines.size(), 1);
    auto rooflines = rooflineGraph.multipleRooflines[0].rooflines[0];
    EXPECT_EQ(rooflines.bw, "18.44");
    EXPECT_EQ(rooflines.computility, "324.40");
    EXPECT_EQ(rooflines.ratio, "0.047412");
    int expect = 2;
    EXPECT_EQ(rooflines.point.size(), expect);
    EXPECT_EQ(rooflines.point[0], "33.28");
}

TEST_F(RooflineTest, ConvertStrToRooflineData_NoMultipleRooflines) {
    std::string jsonStrNoRoofline = R"("{"advice":"latency"}")";
    auto res = praser.ConvertStrToRooflineData(jsonStrNoRoofline);
    EXPECT_EQ(res == std::nullopt, true);
}

TEST_F(RooflineTest, ConvertStrToRooflineData_MultipleRooflinesIsNotArray) {
    std::string jsonStrNotArray = R"("{"advice":"latency", "multiple_rooflines":"xxxx"}")";
    auto res = praser.ConvertStrToRooflineData(jsonStrNotArray);
    EXPECT_EQ(res == std::nullopt, true);
}

TEST_F(RooflineTest, ConvertStrToRooflineData_PointSize) {
    std::string jsonStrPointSize = R"({"adivce":"latency bound:pipeline caused", "multiple_rooflines": )"
                                   R"([{"title": "Memory Unit", "rooflines":)"
                                   R"([{"bw": 18.44, "bwName": "L1 Read + Write", "computility": 324.4001, )"
                                   R"("computilityName": "Cube_FP100.000000%",)"
                                   R"("point": ["33.28"], "ratio": "0.047412"}]}]})";
    auto res = praser.ConvertStrToRooflineData(jsonStrPointSize);
    auto data = res.value();
    EXPECT_EQ(data.multipleRooflines[0].rooflines[0].point.empty(), true);
}
