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
#include "../TestSuit.h"
#include "JsonUtil.h"

using namespace Dic;

TEST(JsonUtil, IsJsonKeyValid) {
    document_t json;
    json.Parse("{\n"
               "        \"ph\": \"X\",\n"
               "        \"name\": \"contiguous_d_Reshape\",\n"
               "        \"args\": {}\n"
               "}");
    bool ph = JsonUtil::IsJsonKeyValid(json, "ph");
    EXPECT_EQ(ph, true);

    bool name = JsonUtil::IsJsonKeyValid(json, "name");
    EXPECT_EQ(name, true);

    bool nofound = JsonUtil::IsJsonKeyValid(json, "nofound");
    EXPECT_EQ(nofound, false);
}

TEST(JsonUtil, SetByJsonKeyValue) {
    document_t json;
    json.Parse("{\n"
               "        \"name\": \"X\",\n"
               "        \"age\": 18,\n"
               "        \"args\": {}\n"
               "}");
    std::string name;
    JsonUtil::SetByJsonKeyValue(name, json, "name");
    EXPECT_EQ(name, "X");

    int age;
    JsonUtil::SetByJsonKeyValue(age, json, "age");
    EXPECT_EQ(age, 18);
}

TEST(JsonUtil, SetByJsonKeyValueDouble) {
    document_t json;
    json.Parse("{\n"
               "        \"name\": \"X\",\n"
               "        \"age\": 18,\n"
               "        \"args\": {}\n"
               "}");
    double age = 0;
    JsonUtil::SetByJsonKeyValue(age, json, "age");
    EXPECT_EQ(age, 18);
    json.Parse("{\n"
               "        \"name\": \"X\",\n"
               "        \"age\": 18,\n"
               "        \"score\":65.12,\n"
               "        \"args\": {}\n"
               "}");
    double score = 0;
    JsonUtil::SetByJsonKeyValue(score, json, "score");
    EXPECT_FLOAT_EQ(score, 65.12);
}

TEST(JsonUtil, JsonUtil_SetByJsonKeyValueFloat) {
    document_t json;
    json.Parse("{\n"
               "        \"name\": \"X\",\n"
               "        \"age\": 18,\n"
               "        \"args\": {}\n"
               "}");
    float age = 0;
    JsonUtil::SetByJsonKeyValue(age, json, "age");
    EXPECT_EQ(age, 18);
    json.Parse("{\n"
               "        \"name\": \"X\",\n"
               "        \"age\": 18,\n"
               "        \"score\":65.12,\n"
               "        \"args\": {}\n"
               "}");
    float score = 0;
    JsonUtil::SetByJsonKeyValue(score, json, "score");
    EXPECT_FLOAT_EQ(score, 65.12);
}

TEST(JsonUtil, TryParse) {
    std::string jsonStr;
    std::string errMessage;
    jsonStr = "{\n"
              "        \"name\": \"X\",\n"
              "        \"age\": 18,\n"
              "        \"args\": {}\n"
              "}";

    const std::optional<document_t> &json = JsonUtil::TryParse(jsonStr, errMessage);
    EXPECT_EQ(json.has_value(), true);

    jsonStr = "{\"key\":\"value\",}";
    JsonUtil::TryParse(jsonStr, errMessage);
    EXPECT_EQ(errMessage.substr(0, 13), "Error code:4.");
}

TEST(JsonUtil, IsJsonArray) {
    document_t json;
    json.Parse(R"({"args": ["a","b"],"key":"value"})");
    std::vector<std::string> args;
    EXPECT_EQ(JsonUtil::IsJsonArray(json, "args"), true);
    EXPECT_EQ(JsonUtil::IsJsonArray(json, "key"), false);
    EXPECT_EQ(JsonUtil::IsJsonArray(json, "key_null"), false);
}

TEST(JsonUtil, JsonDump) {
    rapidjson::Document d;
    d.Parse("{\n"
            "        \"ph\": \"X\",\n"
            "        \"name\": \"contiguous_d_Reshape\",\n"
            "        \"args\": {}\n"
            "}");
    const std::string &string = JsonUtil::JsonDump(d);
    std::cout << string << std::endl;
    EXPECT_EQ(string, "{\"ph\":\"X\",\"name\":\"contiguous_d_Reshape\",\"args\":{}}");
}

TEST(JsonUtil, GetDoubleAndGetLongDouble) {
    rapidjson::Document d;
    d.Parse("{\n"
            "        \"ph\": \"X\",\n"
            "        \"ts\": \"1699579270364817.47\",\n"
            "        \"dur\": \"169.33\",\n"
            "        \"args\": {}\n"
            "}");
    long double ts = JsonUtil::GetLongDouble(d, "ts");
    double dur = JsonUtil::GetDouble(d, "dur");
    EXPECT_EQ(ts, stold("1699579270364817.47"));
    EXPECT_EQ(dur, 169.33);
}

TEST(JsonUtil, GetInteger) {
    rapidjson::Document d;
    d.Parse("{\n"
            "        \"ph\": \"X\",\n"
            "        \"name\": \"contiguous_d_Reshape\",\n"
            "        \"pid\": 768209,\n"
            "        \"tid\": 768209,\n"
            "        \"args\": {}\n"
            "}");
    int64_t pid = JsonUtil::GetInteger(d, "pid");
    int64_t tid = JsonUtil::GetInteger(d, "tid");
    EXPECT_EQ(pid, 768209);
    EXPECT_EQ(tid, 768209);
}

TEST(JsonUtil, GetString) {
    rapidjson::Document d;
    d.Parse("{\n"
            "        \"ph\": \"X\",\n"
            "        \"name\": \"contiguous_d_Reshape\",\n"
            "        \"cat\": \"cpu_op\",\n"
            "        \"args\": {}\n"
            "}");
    std::string name = JsonUtil::GetString(d, "name");
    std::string cat = JsonUtil::GetString(d, "cat");
    EXPECT_EQ(name, "contiguous_d_Reshape");
    EXPECT_EQ(cat, "cpu_op");
}

TEST(JsonUtil, GetOptionalString) {
    rapidjson::Document d;
    d.Parse("{\n"
            "        \"ph\": \"X\",\n"
            "        \"name\": \"contiguous_d_Reshape\",\n"
            "        \"cat\": \"cpu_op\",\n"
            "        \"args\": {}\n"
            "}");
    std::optional<std::string> name = JsonUtil::GetOptionalString(d, "name");
    std::optional<std::string> cat = JsonUtil::GetOptionalString(d, "cat");
    std::optional<std::string> dog = JsonUtil::GetOptionalString(d, "dog");
    EXPECT_EQ(name.value(), "contiguous_d_Reshape");
    EXPECT_EQ(cat.value(), "cpu_op");
    EXPECT_EQ(dog.has_value(), false);
}

TEST(JsonUtil, GetDumpString) {
    rapidjson::Document d;
    d.Parse("{\n"
            "        \"ph\": \"X\",\n"
            "        \"name\": \"contiguous_d_Reshape\",\n"
            "        \"cat\": \"cpu_op\",\n"
            "        \"args\": {}\n"
            "}");
    std::string name = JsonUtil::GetDumpString(d, "name");
    std::string cat = JsonUtil::GetDumpString(d, "cat");
    EXPECT_EQ(name, "contiguous_d_Reshape");
    EXPECT_EQ(cat, "cpu_op");
}

TEST(JsonUtil, GetVectorFloatNormalTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ 3.4, 6.153, 6]
        }
    )");
    auto data = JsonUtil::GetVector<float>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_FLOAT_EQ(data[0], 3.4);
    EXPECT_FLOAT_EQ(data[1], 6.153);
    EXPECT_FLOAT_EQ(data[2], 6);
}

TEST(JsonUtil, GetVectorFloatInconsistentDataTypeTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ "a", "b", "c"]
        }
    )");
    auto data = JsonUtil::GetVector<float>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_FLOAT_EQ(data[0], 0.0);
    EXPECT_FLOAT_EQ(data[1], 0.0);
    EXPECT_FLOAT_EQ(data[2], 0.0);
}

TEST(JsonUtil, GetVectorDoubleNormalTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ 3.4, 6.153, 6]
        }
    )");
    auto data = JsonUtil::GetVector<double>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 3.4);
    EXPECT_EQ(data[1], 6.153);
    EXPECT_EQ(data[2], 6);
}

TEST(JsonUtil, GetVectorDoubleInconsistentDataTypeTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ "a", "b", "c"]
        }
    )");
    auto data = JsonUtil::GetVector<double>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 0.0);
    EXPECT_EQ(data[1], 0.0);
    EXPECT_EQ(data[2], 0.0);
}

TEST(JsonUtil, GetVectorIntNormalTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ 2, -5, 100]
        }
    )");
    auto data = JsonUtil::GetVector<int>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 2);
    EXPECT_EQ(data[1], -5);
    EXPECT_EQ(data[2], 100);
}

TEST(JsonUtil, GetVectorIntInconsistentDataTypeTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ "a", "b", "c"]
        }
    )");
    auto data = JsonUtil::GetVector<int>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 0);
    EXPECT_EQ(data[2], 0);
}

TEST(JsonUtil, GetVectorStringNormalTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ "a", "b", "c"]
        }
    )");
    auto data = JsonUtil::GetVector<std::string>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], "a");
    EXPECT_EQ(data[1], "b");
    EXPECT_EQ(data[2], "c");
}

TEST(JsonUtil, GetVectorStringInconsistentDataTypeTest) {
    rapidjson::Document d;
    d.Parse(R"(
        {
            "data":[ 3.4, 6.153, 6]
        }
    )");
    auto data = JsonUtil::GetVector<std::string>(d, "data");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], "");
    EXPECT_EQ(data[1], "");
    EXPECT_EQ(data[2], "");
}
