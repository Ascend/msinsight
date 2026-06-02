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

#ifndef COMPUTE_FUZZ_H
#define COMPUTE_FUZZ_H
#include <cstdint>
#include <string>

struct BinaryBlockHeader {
    uint64_t contentSize = 0;
    uint8_t type = 0;
    uint8_t padding = 0;
    uint16_t reverse = 0x5a5a;
};

struct BinaryBlockHeader4File {
    BinaryBlockHeader binaryBlockHeader{};
    char filePath[4096] = {0};
};
#pragma pack()

const int INTEGER_BYTES = 4;

inline void GetFilePadding(BinaryBlockHeader &binaryBlockHeader, const uint64_t &sizeContent, std::string &buffer) {
    if (sizeContent % INTEGER_BYTES != 0) {
        binaryBlockHeader.padding = INTEGER_BYTES - (sizeContent % INTEGER_BYTES);
        buffer.resize(sizeContent + binaryBlockHeader.padding, 0);
    }
}

// namespace
#endif // COMPUTE_FUZZ_H
