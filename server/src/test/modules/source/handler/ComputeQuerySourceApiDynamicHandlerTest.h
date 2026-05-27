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

#ifndef PROFILER_SERVER_COMPUTEQUERYSOURCEAPIDYNAMICHANDLERTEST_H
#define PROFILER_SERVER_COMPUTEQUERYSOURCEAPIDYNAMICHANDLERTEST_H

#include <gtest/gtest.h>
#include "ComputeSourceFile.h"
#include "QueryApiLineHandler.h"
#include "SourceProtocolRequest.h"
#include "../mockUtils/BinFileGenerator.h"
#include "QueryCodeFileHandler.h"

namespace Dic::Module::Source::Test {
using namespace Dic::Server;
using namespace Dic::Module;
using namespace Dic::Module::Source;

class ComputeQuerySourceApiDynamicHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        // generate bin file
        auto apiPtr = std::make_unique<NormalDataBlock>(DataTypeEnum::API_FILE, std::string(API_FILE_WITH_DTYPE));
        auto instrPtr = std::make_unique<NormalDataBlock>(DataTypeEnum::API_INSTR, std::string(INSTR_FILE_WITH_DTYPE));
        auto sourcePtr = std::make_unique<SourceDataBlock>(std::string(SOURCE_FILE), std::string(SOURCE_NAME));
        BinFileGenerator generator;

        generator.AddDataBlock(std::move(apiPtr));
        generator.AddDataBlock(std::move(instrPtr));
        generator.AddDataBlock(std::move(sourcePtr));
        generator.Generate(std::string(BIN_FILE_PATH_API_LINE_DYNAMIC));

        // init parser
        std::string binPath = std::string(BIN_FILE_PATH_API_LINE_DYNAMIC);
        SourceFileParser::Instance().SetFilePath(binPath);
        SourceFileParser::Instance().Parse({binPath}, binPath, binPath, "");
    }

    static void TearDownTestSuite() {
        // remove bin file
        Dic::Module::Source::Test::BinFileGenerator::RemoveFile(std::string(BIN_FILE_PATH_API_LINE_DYNAMIC));

        // reset parser
        SourceFileParser::Instance().Reset();
    }
};
}

#endif // PROFILER_SERVER_COMPUTEQUERYSOURCEAPIDYNAMICHANDLERTEST_H
