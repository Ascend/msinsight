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
#include "ComputeQuerySourceApiHandlerTest.h"
#include "QueryApiLineHandler.h"
#include "SourceProtocolRequest.h"
#include "QueryCodeFileHandler.h"

using namespace Dic::Server;
using namespace Dic::Module;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

TEST_F(ComputeQuerySourceApiHandlerTest, testQueryApiLineHandlerRequestWithValidData) {
    QueryApiLineHandler handler;
    auto ptr = std::make_unique<SourceApiLineRequest>();
    ptr->params.sourceName = SOURCE_NAME;
    ptr->params.coreName = CORE_NAME;
    ptr->moduleName = "Source";
    ptr->projectName = "project";
    handler.HandleRequest(std::move(ptr));
}

TEST_F(ComputeQuerySourceApiHandlerTest, testQueryCodeFileHandlerRequestWithValidData) {
    QueryCodeFileHandler handler;
    auto ptr = std::make_unique<SourceCodeFileRequest>();
    ptr->params.sourceName = SOURCE_NAME;
    handler.HandleRequest(std::move(ptr));
}
