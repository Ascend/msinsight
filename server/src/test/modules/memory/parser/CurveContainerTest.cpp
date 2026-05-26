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
#include <vector>
#include <string>
#include <limits>
#include "CurveContainer.h"

using namespace Dic::Module::Memory;

class CurveContainerTest : public ::testing::Test {
  protected:
    void SetUp() override { container = std::make_unique<CurveContainer>(); }

    void TearDown() override { container->Clear(); }

    std::unique_ptr<CurveContainer> container;
};

// 测试Exist方法
TEST_F(CurveContainerTest, Exist_WhenKeyMatches_ReturnsTrue) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y1", "Y2"};
    curve.tempData = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    container->PutCurve("test_key", curve);

    EXPECT_TRUE(container->Exist("test_key"));
}

TEST_F(CurveContainerTest, Exist_WhenKeyDoesNotMatch_ReturnsFalse) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y1", "Y2"};
    curve.tempData = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    container->PutCurve("test_key", curve);

    EXPECT_FALSE(container->Exist("different_key"));
}

TEST_F(CurveContainerTest, Exist_WhenContainerEmpty_ReturnsFalse) { EXPECT_FALSE(container->Exist("any_key")); }

// 测试Clear方法
TEST_F(CurveContainerTest, Clear_RemovesAllData) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y1", "Y2"};
    curve.tempData = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    container->PutCurve("test_key", curve);
    container->Clear();

    EXPECT_FALSE(container->Exist("test_key"));
}

// 测试PutCurve方法
TEST_F(CurveContainerTest, PutCurve_StoresDataCorrectly) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y1", "Y2"};
    curve.tempData = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    container->PutCurve("test_key", curve);

    // 验证数据可以通过ComputeCurve正确检索
    auto result = container->ComputeCurve(0.0, 10.0, "test_key");
    EXPECT_EQ(result.title, "Test Curve");
    EXPECT_EQ(result.legends.size(), 3);
}

// 测试ComputeCurve方法 - 键不匹配的情况
TEST_F(CurveContainerTest, ComputeCurve_WhenKeyDoesNotMatch_ReturnsEmptyResult) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y1", "Y2"};
    curve.tempData = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 10.0, "wrong_key");
    EXPECT_TRUE(result.title.empty());
    EXPECT_TRUE(result.legends.empty());
    EXPECT_TRUE(result.dataLines.empty());
}

// 测试ComputeCurve方法 - 空数据的情况
TEST_F(CurveContainerTest, ComputeCurve_WhenLegendsEmpty_ReturnsEmptyResult) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {}; // 空图例
    curve.tempData = {};

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 10.0, "test_key");
    EXPECT_TRUE(result.dataLines.empty());
}

// 测试ComputeCurve方法 - 数据点少于桶数量的情况
TEST_F(CurveContainerTest, ComputeCurve_WhenDataPointsLessThanBuckets_ReturnsAllPoints) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y"};

    // 创建500个数据点（少于1000个桶）
    for (int i = 0; i < 500; ++i) {
        curve.tempData.push_back(static_cast<double>(i)); // X值
        curve.tempData.push_back(static_cast<double>(i * 2)); // Y值
    }

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 500.0, "test_key");

    // 应该返回所有500个点
    EXPECT_EQ(result.dataLines.size(), 500);
}

// 测试ComputeCurve方法 - 数据点多于桶数量的情况
TEST_F(CurveContainerTest, ComputeCurve_WhenDataPointsMoreThanBuckets_ReturnsDownsampledPoints) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y"};

    // 创建2000个数据点（多于1000个桶）
    for (int i = 0; i < 3000; ++i) {
        curve.tempData.push_back(static_cast<double>(i)); // X值
        curve.tempData.push_back(static_cast<double>(i * 2)); // Y值
    }

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 3000.0, "test_key");

    // 应该返回降采样后的点，数量应该小于3000但大于0
    EXPECT_GT(result.dataLines.size(), 0);
    EXPECT_LT(result.dataLines.size(), 3000);
}

// 测试ComputeCurve方法 - 范围过滤
TEST_F(CurveContainerTest, ComputeCurve_WhenRangeSpecified_FiltersPointsCorrectly) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y"};

    // 创建100个数据点，X值从0到99
    for (int i = 0; i < 100; ++i) {
        curve.tempData.push_back(static_cast<double>(i)); // X值
        curve.tempData.push_back(static_cast<double>(i * 2)); // Y值
    }

    container->PutCurve("test_key", curve);

    // 只请求X值在20到30之间的点
    auto result = container->ComputeCurve(20.0, 30.0, "test_key");

    // 应该返回11个点（20,21,...,30）
    EXPECT_EQ(result.dataLines.size(), 11);
}

// 测试ComputeCurve方法 - 处理NaN值
TEST_F(CurveContainerTest, ComputeCurve_WhenDataContainsNaN_HandlesCorrectly) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y"};

    curve.tempData = {
        1.0, 2.0, // 正常点
        2.0, std::numeric_limits<double>::quiet_NaN(), // 包含NaN的点
        3.0, 6.0 // 正常点
    };

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 5.0, "test_key");

    // 应该返回3个点，其中第二个点的Y值应该是"NULL"
    EXPECT_EQ(result.dataLines.size(), 3);
    EXPECT_EQ(result.dataLines[1][1], "NULL"); // 第二个点的Y值
}

// 测试ComputeCurve方法 - 当xMin等于xMax且等于0时的特殊处理
TEST_F(CurveContainerTest, ComputeCurve_WhenXMinXMaxZero_AdjustsRange) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y"};

    // 创建一些数据点
    curve.tempData = {0.0, 1.0, 5.0, 2.0, 10.0, 3.0};

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 0.0, "test_key");

    // 范围应该被调整为数据的实际范围
    EXPECT_GT(result.dataLines.size(), 0);
}

// 测试ComputeDataIndex方法
TEST_F(CurveContainerTest, ComputeDataIndex_FindsMinMaxIndices) {
    // 由于ComputeDataIndex是私有方法，我们需要通过公共方法间接测试
    // 或者可以将测试类声明为友元类来测试私有方法

    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y1", "Y2"};

    // 创建测试数据
    std::vector<double> testData = {
        1.0, 10.0, 100.0, // 第1行：X=1.0, Y1=10.0, Y2=100.0
        2.0, 5.0, 200.0, // 第2行：X=2.0, Y1=5.0, Y2=200.0 (Y1最小值)
        3.0, 15.0, 50.0, // 第3行：X=3.0, Y1=15.0, Y2=50.0 (Y2最小值)
        4.0, 20.0, 300.0 // 第4行：X=4.0, Y1=20.0, Y2=300.0 (Y2最大值)
    };

    curve.tempData = testData;
    container->PutCurve("test_key", curve);

    // 测试范围包含所有点
    auto result = container->ComputeCurve(1.0, 4.0, "test_key");

    // 验证返回了正确的数据点
    EXPECT_EQ(result.dataLines.size(), 4);
}

// 测试边界情况 - 空容器
TEST_F(CurveContainerTest, ComputeCurve_WhenContainerEmpty_ReturnsEmpty) {
    auto result = container->ComputeCurve(0.0, 10.0, "any_key");
    EXPECT_TRUE(result.title.empty());
    EXPECT_TRUE(result.legends.empty());
    EXPECT_TRUE(result.dataLines.empty());
}

// 测试边界情况 - 数据点数量正好等于桶数量
TEST_F(CurveContainerTest, ComputeCurve_WhenDataPointsEqualBuckets_ReturnsAllPoints) {
    CurveView curve;
    curve.title = "Test Curve";
    curve.legends = {"X", "Y"};

    // 创建正好1000个数据点
    for (int i = 0; i < 1000; ++i) {
        curve.tempData.push_back(static_cast<double>(i));
        curve.tempData.push_back(static_cast<double>(i * 2));
    }

    container->PutCurve("test_key", curve);

    auto result = container->ComputeCurve(0.0, 1000.0, "test_key");

    // 应该返回所有1000个点（因为m <= numBuckets）
    EXPECT_EQ(result.dataLines.size(), 1000);
}
