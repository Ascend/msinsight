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

#include "SourceInstructionParserTest.h"
#include <gtest/gtest.h>
#include "SourceInstructionParser.h"
#include "NumberUtil.h"
#include "../handler/ComputeSourceFile.h"

using namespace Dic;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

class SourceInstructionParserTest : public ::testing::Test {};

class SourceInstructionParserDerived : public SourceInstructionParser {
  public:
    void ConvertApiInstr(std::string &json) { SourceInstructionParser::ConvertApiInstr(json); }

    void ConvertApiInstrDynamic(std::string &json) { SourceInstructionParser::ConvertApiInstrDynamic(json); }

    std::vector<SourceFileInstructionDynamicCol> &GetInstructionList() {
        return SourceInstructionParser::GetInstructionList();
    }

    void ConvertApiFile(std::string &json) { SourceInstructionParser::ConvertApiFile(json); }

    void ConvertApiFileDynamic(std::string &json) { SourceInstructionParser::ConvertApiFileDynamic(json); }

    std::unordered_map<std::string, std::vector<SourceFileLineDynamicCol>> &GetSourceLinesMap() {
        return SourceInstructionParser::GetSourceLinesMap();
    }

    std::vector<SourceApiInstruction> &GetApiInstructionList() {
        return SourceInstructionParser::GetApiInstructionList();
    }

    std::map<std::string, std::vector<SourceFileLine>> &GetApiFiles() { return SourceInstructionParser::GetApiFiles(); }

    void PreprocessInstrWrapper(document_t &doc) { SourceInstructionParser::PreprocessInstr(doc); }
};

TEST(SourceInstructionParserTest, testConvertApiInstrNewWithValidJsonData) {
    SourceInstructionParserDerived parser;
    std::string json = std::string(SOURCE_INSTRUCTIONS_JSON);
    parser.ConvertApiInstrDynamic(json);
    auto list = parser.GetInstructionList();
    EXPECT_TRUE(list.size() == 2);
    auto instr = list[1];
    EXPECT_EQ(instr.stringColumnMap["string"].size(), 1);
    EXPECT_EQ(instr.stringColumnMap["string"][0], "0x1269f001");
    EXPECT_EQ(instr.stringColumnMap["string list"].size(), 2);
    EXPECT_EQ(instr.stringColumnMap["string list"][1], "bb1");

    EXPECT_EQ(instr.intColumnMap["int"].size(), 1);
    EXPECT_EQ(instr.intColumnMap["int"][0], 11);
    EXPECT_EQ(instr.intColumnMap["int list"].size(), 2);
    EXPECT_EQ(instr.intColumnMap["int list"][1], 22);
    EXPECT_EQ(instr.floatColumnMap["float"].size(), 1);
    EXPECT_EQ(instr.floatColumnMap["float list"].size(), 2);
}

TEST(SourceInstructionParserTest, testConvertApiFileNewWithValidJsonData) {
    SourceInstructionParserDerived parser;
    std::string json = std::string(SOURCE_API_FILE_JSON);
    parser.ConvertApiFileDynamic(json);
    auto map = parser.GetSourceLinesMap();
    EXPECT_EQ(map.size(), 2);
    EXPECT_TRUE(map.count("b.cpp") != 0);
    auto lines = map["b.cpp"];
    EXPECT_EQ(lines.size(), 2);
    auto line = lines[1];
    EXPECT_EQ(line.addressRange.size(), 2);
    EXPECT_EQ(line.addressRange[1].first, "ag");
    EXPECT_EQ(line.addressRange[1].second, "ah");

    EXPECT_EQ(line.stringColumnMap["string"].size(), 1);
    EXPECT_EQ(line.stringColumnMap["string"][0], "0x1269f001");
    EXPECT_EQ(line.intColumnMap["int"].size(), 1);
    EXPECT_EQ(line.intColumnMap["int"][0], 11);
    EXPECT_EQ(line.floatColumnMap["float"].size(), 1);
    EXPECT_TRUE(NumberUtil::IsEqual(line.floatColumnMap["float"][0], 1.2));
    EXPECT_EQ(line.stringColumnMap["string list"].size(), 2);
    EXPECT_EQ(line.stringColumnMap["string list"][1], "bb1");
    EXPECT_EQ(line.intColumnMap["int list"].size(), 2);
    EXPECT_EQ(line.intColumnMap["int list"][1], 22);
    EXPECT_EQ(line.floatColumnMap["float list"].size(), 2);
    EXPECT_TRUE(NumberUtil::IsEqual(line.floatColumnMap["float list"][1], 12.2));
}

TEST(SourceInstructionParserTest, testConvertApiInstrNewWithoutDtype) {
    SourceInstructionParserDerived parser;
    std::string json = std::string(INSTR_FILE);
    parser.ConvertApiInstr(json);
    auto list = parser.GetApiInstructionList();
    EXPECT_TRUE(!list.empty());
    auto line = list[0];
    EXPECT_EQ(line.address, "0x1134e2d8");
    EXPECT_EQ(line.source, "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:0|R],[#ofst:9],[cop:1],[l2_cache_hint:0]");
    EXPECT_EQ(line.pipe, "RVECLD");
    EXPECT_EQ(line.ascendCInnerCode, "/test/vec_add1_simt.cpp:50");
    EXPECT_TRUE(!line.realStallCycles.empty());
    EXPECT_EQ(line.realStallCycles[0], 13); // value is 13
    EXPECT_TRUE(!line.theoreticalStallCycles.empty());
    EXPECT_EQ(line.theoreticalStallCycles[0], 8); // value is 8
    EXPECT_TRUE(!line.cycles.empty());
    EXPECT_EQ(line.cycles[0], 62); // value is 62
    EXPECT_TRUE(!line.instructionsExecuted.empty());
    EXPECT_EQ(line.instructionsExecuted[0], 4); // value is 4
}

TEST(SourceInstructionParserTest, testConvertApiFileNewWithoutDtype) {
    SourceInstructionParserDerived parser;
    std::string json = std::string(API_FILE);
    std::string sourceName = "/test/vec_add1_simt.cpp";
    parser.ConvertApiFile(json);
    auto map = parser.GetApiFiles();
    auto lines = map[sourceName];
    EXPECT_TRUE(!lines.empty());
    auto line = lines[0];
    EXPECT_EQ(line.line, 31); // value is 31
    EXPECT_TRUE(!line.instructionsExecuted.empty());
    EXPECT_EQ(line.instructionsExecuted[0], 8); // value is 8
    EXPECT_TRUE(!line.cycles.empty());
    EXPECT_EQ(line.cycles[0], 56); // value is 56
    EXPECT_TRUE(!line.addressRange.empty());
    EXPECT_EQ(line.addressRange[0].first, "0x1134e2d8");
    EXPECT_EQ(line.addressRange[0].second, "0x1134e4d8");
}

TEST(SourceInstructionParserTest, testGRPStatusHelperProgressAndReset) {
    GRPStatusHelper helper;
    EXPECT_EQ(helper.UpdateGRPStatus("R0", 3, GRPStatus::READ), GRPProgress::BEGIN);
    EXPECT_EQ(helper.UpdateGRPStatus("R0", 3, GRPStatus::READ), GRPProgress::IN_USE);
    EXPECT_EQ(helper.UpdateGRPStatus("R0", 3, GRPStatus::READ), GRPProgress::END);
    EXPECT_EQ(helper.UpdateGRPStatus("R0", 3, GRPStatus::READ), GRPProgress::IN_USE);
    helper.Reset();
    EXPECT_EQ(helper.UpdateGRPStatus("R0", 2, GRPStatus::WRITE), GRPProgress::BEGIN);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperGetRegisterLifeTimeBasic) {
    GRPStatusHelper helper;

    helper.UpdateGRPStatus("R0", 5, GRPStatus::READ);
    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 5);

    helper.UpdateGRPStatus("R0", 5, GRPStatus::READ);
    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 5);

    helper.UpdateGRPStatus("R0", 5, GRPStatus::READ);
    helper.UpdateGRPStatus("R0", 5, GRPStatus::READ);
    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 5);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperGetRegisterLifeTimeForNonExistent) {
    GRPStatusHelper helper;
    EXPECT_EQ(helper.GetRegisterLifeTime("NonExistent", 10), 10);
    EXPECT_EQ(helper.GetRegisterLifeTime("R999", 7), 7);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperGetRegisterLifeTimeAfterEnd) {
    GRPStatusHelper helper;

    helper.UpdateGRPStatus("R0", 3, GRPStatus::READ);
    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 3);

    helper.UpdateGRPStatus("R0", 3, GRPStatus::READ);
    helper.UpdateGRPStatus("R0", 3, GRPStatus::WRITE);
    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 5), 3);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperGetIndexBasic) {
    GRPStatusHelper helper;

    int index0 = helper.GetIndex("R0");
    EXPECT_EQ(index0, 0);

    int index1 = helper.GetIndex("R1");
    EXPECT_EQ(index1, 1);

    int index2 = helper.GetIndex("R2");
    EXPECT_EQ(index2, 2);

    int index0Again = helper.GetIndex("R0");
    EXPECT_EQ(index0Again, 0);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperGetIndexWithReuse) {
    GRPStatusHelper helper;

    int idx0 = helper.GetIndex("R0");
    EXPECT_EQ(idx0, 0);

    int idx1 = helper.GetIndex("R1");
    EXPECT_EQ(idx1, 1);

    helper.UpdateGRPStatus("R0", 1, GRPStatus::READ);
    EXPECT_EQ(helper.UpdateGRPStatus("R0", 1, GRPStatus::READ), GRPProgress::IN_USE);

    int idx2 = helper.GetIndex("R2");
    EXPECT_EQ(idx2, 2);

    int idx3 = helper.GetIndex("R3");
    EXPECT_EQ(idx3, 3);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperGetIndexAfterReset) {
    GRPStatusHelper helper;

    helper.GetIndex("R0");
    helper.GetIndex("R1");
    helper.GetIndex("R2");

    helper.Reset();

    int idx0 = helper.GetIndex("R0");
    EXPECT_EQ(idx0, 0);

    int idx1 = helper.GetIndex("R1");
    EXPECT_EQ(idx1, 1);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperResetClearsAllMaps) {
    GRPStatusHelper helper;

    helper.UpdateGRPStatus("R0", 5, GRPStatus::READ);
    helper.UpdateGRPStatus("R1", 3, GRPStatus::WRITE);
    helper.GetIndex("R0");
    helper.GetIndex("R1");

    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 5);
    EXPECT_EQ(helper.GetRegisterLifeTime("R1", 0), 3);

    helper.Reset();

    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 7), 7);
    EXPECT_EQ(helper.GetRegisterLifeTime("R1", 8), 8);

    int newIdx = helper.GetIndex("R0");
    EXPECT_EQ(newIdx, 0);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperCompleteLifecycleWithIndex) {
    GRPStatusHelper helper;

    int idx = helper.GetIndex("R0");
    EXPECT_EQ(idx, 0);

    auto progress1 = helper.UpdateGRPStatus("R0", 4, GRPStatus::READ);
    EXPECT_EQ(progress1, GRPProgress::BEGIN);
    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 4);

    auto progress2 = helper.UpdateGRPStatus("R0", 4, GRPStatus::READ);
    EXPECT_EQ(progress2, GRPProgress::IN_USE);

    auto progress3 = helper.UpdateGRPStatus("R0", 4, GRPStatus::READ);
    EXPECT_EQ(progress3, GRPProgress::IN_USE);

    auto progress4 = helper.UpdateGRPStatus("R0", 4, GRPStatus::WRITE);
    EXPECT_EQ(progress4, GRPProgress::END);

    int newIdx = helper.GetIndex("R1");
    EXPECT_EQ(newIdx, 1);
}

TEST(SourceInstructionParserTest, testGRPStatusHelperMultipleRegistersWithIndices) {
    GRPStatusHelper helper;

    int idx0 = helper.GetIndex("R0");
    int idx1 = helper.GetIndex("R1");
    int idx2 = helper.GetIndex("R2");

    EXPECT_EQ(idx0, 0);
    EXPECT_EQ(idx1, 1);
    EXPECT_EQ(idx2, 2);

    helper.UpdateGRPStatus("R0", 5, GRPStatus::READ);
    helper.UpdateGRPStatus("R1", 3, GRPStatus::WRITE);
    helper.UpdateGRPStatus("R2", 4, GRPStatus::READ_WRITE);

    EXPECT_EQ(helper.GetRegisterLifeTime("R0", 0), 5);
    EXPECT_EQ(helper.GetRegisterLifeTime("R1", 0), 3);
    EXPECT_EQ(helper.GetRegisterLifeTime("R2", 0), 4);

    EXPECT_EQ(helper.GetIndex("R0"), idx0);
    EXPECT_EQ(helper.GetIndex("R1"), idx1);
    EXPECT_EQ(helper.GetIndex("R2"), idx2);
}

TEST(SourceInstructionParserTest, testPreprocessInstrSortsAndUpdatesGRPStatus) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x2",
                    "GPR Status": [
                        {"regIndex": "R1", "survivalTime": 2, "regStatus": 1}
                    ]
                },
                {
                    "Address": "0x1",
                    "GPR Status": [
                        {"regIndex": "R1", "survivalTime": 2, "regStatus": 2}
                    ]
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    ASSERT_TRUE(doc.HasMember("Instructions"));
    auto &instructions = doc["Instructions"];
    ASSERT_TRUE(instructions.IsArray());
    ASSERT_EQ(instructions.Size(), 2);
    EXPECT_STREQ(instructions[0]["Address"].GetString(), "0x1");
    EXPECT_STREQ(instructions[1]["Address"].GetString(), "0x2");

    ASSERT_TRUE(instructions[0].HasMember("GPR Status"));
    auto &firstStatus = instructions[0]["GPR Status"];
    ASSERT_TRUE(firstStatus.IsArray());
    ASSERT_EQ(firstStatus.Size(), 1);
    EXPECT_STREQ(firstStatus[0]["name"].GetString(), "R1");
    EXPECT_EQ(firstStatus[0]["state"].GetInt(), static_cast<int>(GRPStatus::WRITE));
    EXPECT_EQ(firstStatus[0]["progress"].GetInt(), static_cast<int>(GRPProgress::BEGIN));
    EXPECT_EQ(firstStatus[0]["length"].GetInt(), 2);

    ASSERT_TRUE(instructions[1].HasMember("GPR Status"));
    auto &secondStatus = instructions[1]["GPR Status"];
    ASSERT_TRUE(secondStatus.IsArray());
    ASSERT_EQ(secondStatus.Size(), 1);
    EXPECT_STREQ(secondStatus[0]["name"].GetString(), "R1");
    EXPECT_EQ(secondStatus[0]["state"].GetInt(), static_cast<int>(GRPStatus::READ));
    EXPECT_EQ(secondStatus[0]["progress"].GetInt(), static_cast<int>(GRPProgress::END));
    EXPECT_EQ(secondStatus[0]["length"].GetInt(), 2);
}

TEST(SourceInstructionParserTest, testPreprocessInstrMultipleRegistersWithDifferentCostTimes) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x100",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 5, "regStatus": 1},
                        {"regIndex": "R1", "survivalTime": 3, "regStatus": 2}
                    ]
                },
                {
                    "Address": "0x200",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 5, "regStatus": 1},
                        {"regIndex": "R1", "survivalTime": 3, "regStatus": 2}
                    ]
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    auto &instructions = doc["Instructions"];
    ASSERT_EQ(instructions.Size(), 2);

    auto &firstInstrStatus = instructions[0]["GPR Status"];
    ASSERT_EQ(firstInstrStatus.Size(), 2);
    EXPECT_STREQ(firstInstrStatus[0]["name"].GetString(), "R0");
    EXPECT_EQ(firstInstrStatus[0]["progress"].GetInt(), static_cast<int>(GRPProgress::BEGIN));
    EXPECT_STREQ(firstInstrStatus[1]["name"].GetString(), "R1");
    EXPECT_EQ(firstInstrStatus[1]["progress"].GetInt(), static_cast<int>(GRPProgress::BEGIN));

    auto &secondInstrStatus = instructions[1]["GPR Status"];
    ASSERT_EQ(secondInstrStatus.Size(), 2);
    EXPECT_STREQ(secondInstrStatus[0]["name"].GetString(), "R0");
    EXPECT_EQ(secondInstrStatus[0]["progress"].GetInt(), static_cast<int>(GRPProgress::IN_USE));
    EXPECT_EQ(secondInstrStatus[0]["length"].GetInt(), 5);
    EXPECT_STREQ(secondInstrStatus[1]["name"].GetString(), "R1");
    EXPECT_EQ(secondInstrStatus[1]["progress"].GetInt(), static_cast<int>(GRPProgress::IN_USE));
}

TEST(SourceInstructionParserTest, testPreprocessInstrSortsByCostTimeThenRegName) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x100",
                    "GPR Status": [
                        {"regIndex": "R2", "survivalTime": 3, "regStatus": 1},
                        {"regIndex": "R0", "survivalTime": 3, "regStatus": 2},
                        {"regIndex": "R1", "survivalTime": 3, "regStatus": 3}
                    ]
                },
                {
                    "Address": "0x200",
                    "GPR Status": [
                        {"regIndex": "R2", "survivalTime": 3, "regStatus": 1},
                        {"regIndex": "R0", "survivalTime": 3, "regStatus": 2},
                        {"regIndex": "R1", "survivalTime": 3, "regStatus": 3}
                    ]
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    auto &instructions = doc["Instructions"];
    auto &secondInstrStatus = instructions[1]["GPR Status"];
    ASSERT_EQ(secondInstrStatus.Size(), 3);

    EXPECT_STREQ(secondInstrStatus[0]["name"].GetString(), "R2");
    EXPECT_STREQ(secondInstrStatus[1]["name"].GetString(), "R0");
    EXPECT_STREQ(secondInstrStatus[2]["name"].GetString(), "R1");
}

TEST(SourceInstructionParserTest, testPreprocessInstrWithCompleteLifecycle) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x100",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 4, "regStatus": 1}
                    ]
                },
                {
                    "Address": "0x200",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 4, "regStatus": 1}
                    ]
                },
                {
                    "Address": "0x300",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 4, "regStatus": 2}
                    ]
                },
                {
                    "Address": "0x400",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 4, "regStatus": 3}
                    ]
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    auto &instructions = doc["Instructions"];
    ASSERT_EQ(instructions.Size(), 4);

    auto &instr0Status = instructions[0]["GPR Status"];
    EXPECT_EQ(instr0Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::BEGIN));
    EXPECT_EQ(instr0Status[0]["length"].GetInt(), 4);

    auto &instr1Status = instructions[1]["GPR Status"];
    EXPECT_EQ(instr1Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::IN_USE));
    EXPECT_EQ(instr1Status[0]["length"].GetInt(), 4);

    auto &instr2Status = instructions[2]["GPR Status"];
    EXPECT_EQ(instr2Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::IN_USE));
    EXPECT_EQ(instr2Status[0]["length"].GetInt(), 4);

    auto &instr3Status = instructions[3]["GPR Status"];
    EXPECT_EQ(instr3Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::END));
    EXPECT_EQ(instr3Status[0]["length"].GetInt(), 4);
}

TEST(SourceInstructionParserTest, testPreprocessInstrWithEmptyGPRStatus) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x100",
                    "GPR Status": []
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    auto &instructions = doc["Instructions"];
    ASSERT_EQ(instructions.Size(), 1);
    auto &status = instructions[0]["GPR Status"];
    ASSERT_TRUE(status.IsArray());
    EXPECT_EQ(status.Size(), 0);
}

TEST(SourceInstructionParserTest, testPreprocessInstrWithoutGPRStatus) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x100"
                },
                {
                    "Address": "0x200"
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    auto &instructions = doc["Instructions"];
    ASSERT_EQ(instructions.Size(), 2);
    EXPECT_STREQ(instructions[0]["Address"].GetString(), "0x100");
    EXPECT_STREQ(instructions[1]["Address"].GetString(), "0x200");
}

TEST(SourceInstructionParserTest, testPreprocessInstrRegisterReusageAfterLifecycleEnd) {
    SourceInstructionParserDerived parser;
    document_t doc;
    doc.Parse(R"(
        {
            "Instructions": [
                {
                    "Address": "0x100",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 2, "regStatus": 1}
                    ]
                },
                {
                    "Address": "0x200",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 2, "regStatus": 2}
                    ]
                },
                {
                    "Address": "0x300",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 3, "regStatus": 3}
                    ]
                },
                {
                    "Address": "0x400",
                    "GPR Status": [
                        {"regIndex": "R0", "survivalTime": 3, "regStatus": 1}
                    ]
                }
            ]
        }
    )");

    parser.PreprocessInstrWrapper(doc);

    auto &instructions = doc["Instructions"];
    ASSERT_EQ(instructions.Size(), 4);

    auto &instr0Status = instructions[0]["GPR Status"];
    EXPECT_EQ(instr0Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::BEGIN));
    EXPECT_EQ(instr0Status[0]["length"].GetInt(), 2);

    auto &instr1Status = instructions[1]["GPR Status"];
    EXPECT_EQ(instr1Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::END));
    EXPECT_EQ(instr1Status[0]["length"].GetInt(), 2);

    auto &instr2Status = instructions[2]["GPR Status"];
    EXPECT_EQ(instr2Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::BEGIN));
    EXPECT_EQ(instr2Status[0]["length"].GetInt(), 3);

    auto &instr3Status = instructions[3]["GPR Status"];
    EXPECT_EQ(instr3Status[0]["progress"].GetInt(), static_cast<int>(GRPProgress::IN_USE));
    EXPECT_EQ(instr3Status[0]["length"].GetInt(), 3);
}
