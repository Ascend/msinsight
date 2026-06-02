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
#ifndef PROFILER_SERVER_PARSERJSON_MOCK_DATA_H
#define PROFILER_SERVER_PARSERJSON_MOCK_DATA_H
#include "FileDef.h"
namespace Dic::Module::ParserJsonMock {
int64_t CheckParseFileInfoSizeWhenOneFileIs20GThenReturnFalseMock(const std::string &filePath) {
    static uint32_t count = 0;
    const static uint32_t exceptionOrder = 20;
    const static long long FILE_SIZE_COUNT = (JSON_MAX_FILE_SIZE + 1);
    if (count == exceptionOrder) {
        count = 0;
        return FILE_SIZE_COUNT;
    } else {
        count++;
        return 0;
    }
}

int64_t CheckParseFileInfoSizeWhenTotalFileSizeExceed20GThenReturnFalseMock(const std::string &filePath) {
    static uint32_t count = 0;
    const static uint32_t exceptionOrder = 20;
    const static long long FILE_SIZE_COUNT = 1024 * 1024 * 1024;
    if (count == exceptionOrder) {
        count = 0;
        return 1;
    } else {
        count++;
        return FILE_SIZE_COUNT;
    }
}
}
#endif // PROFILER_SERVER_PARSERJSON_MOCK_DATA_H
