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

#ifndef PROFILER_SERVER_MOCKFILEREADER_H
#define PROFILER_SERVER_MOCKFILEREADER_H
#include <gmock/gmock.h>
#include "IFileReader.h"
class MockFileReader : public Dic::Module::IFileReader {
  public:
    ~MockFileReader() override = default;
    MOCK_METHOD(int64_t, GetFileSize, (const std::string &filePath), (override));
    MOCK_METHOD(std::string, ReadJsonArray, (const std::string &filePath, int64_t startPosition, int64_t endPosition),
        (override));
};
#endif // PROFILER_SERVER_MOCKFILEREADER_H
