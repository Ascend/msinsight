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
#include "FileParser.h"

namespace Dic {
namespace Module {
std::string FileParser::GetError() { return error; }

void FileParser::SetParseEndCallBack(
    std::function<void(const std::string, const std::string, bool result, const std::string)> &callback) {
    parseEndCallback = callback;
}

void FileParser::SetParseProgressCallBack(
    std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> &callback) {
    parseProgressCallback = callback;
}

void FileParser::Reset() {
    error.clear();
    parseEndCallback = nullptr;
    parseProgressCallback = nullptr;
    fileProgressMap.clear();
}
} // end of namespace Module
} // end of namespace Dic
