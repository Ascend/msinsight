/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
 * -------------------------------------------------------------------------
 */

#include "HashUtil.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Dic::HashUtil {
namespace {
constexpr size_t HASH_BUFFER_SIZE = 1024 * 1024;

class Sha256 {
  public:
    void Update(const char *data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            buffer[bufferSize++] = static_cast<uint8_t>(data[i]);
            if (bufferSize == buffer.size()) {
                Transform();
                bitLength += 512;
                bufferSize = 0;
            }
        }
    }

    std::string Final() {
        const size_t messageSize = bufferSize;
        buffer[bufferSize++] = 0x80;
        if (bufferSize > 56) {
            while (bufferSize < buffer.size()) {
                buffer[bufferSize++] = 0;
            }
            Transform();
            bufferSize = 0;
        }
        while (bufferSize < 56) {
            buffer[bufferSize++] = 0;
        }
        bitLength += static_cast<uint64_t>(messageSize) * 8;
        for (size_t i = 0; i < 8; ++i) {
            buffer[63 - i] = static_cast<uint8_t>(bitLength >> (i * 8));
        }
        Transform();

        std::stringstream result;
        result << std::hex << std::setfill('0');
        for (uint32_t value : state) {
            result << std::setw(8) << value;
        }
        return result.str();
    }

  private:
    static constexpr std::array<uint32_t, 64> ROUND_CONSTANTS = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
        0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
        0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351,
        0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f,
        0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    static uint32_t RotateRight(uint32_t value, uint32_t bits) { return (value >> bits) | (value << (32 - bits)); }

    void Transform() {
        std::array<uint32_t, 64> words{};
        for (size_t i = 0; i < 16; ++i) {
            const size_t offset = i * 4;
            words[i] = (static_cast<uint32_t>(buffer[offset]) << 24) |
                (static_cast<uint32_t>(buffer[offset + 1]) << 16) | (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
                static_cast<uint32_t>(buffer[offset + 3]);
        }
        for (size_t i = 16; i < words.size(); ++i) {
            const uint32_t s0 = RotateRight(words[i - 15], 7) ^ RotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const uint32_t s1 = RotateRight(words[i - 2], 17) ^ RotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        auto working = state;
        for (size_t i = 0; i < words.size(); ++i) {
            const uint32_t sum1 =
                RotateRight(working[4], 6) ^ RotateRight(working[4], 11) ^ RotateRight(working[4], 25);
            const uint32_t choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
            const uint32_t temp1 = working[7] + sum1 + choice + ROUND_CONSTANTS[i] + words[i];
            const uint32_t sum0 =
                RotateRight(working[0], 2) ^ RotateRight(working[0], 13) ^ RotateRight(working[0], 22);
            const uint32_t majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
            const uint32_t temp2 = sum0 + majority;
            for (size_t j = working.size() - 1; j > 0; --j) {
                working[j] = working[j - 1];
            }
            working[4] += temp1;
            working[0] = temp1 + temp2;
        }
        for (size_t i = 0; i < state.size(); ++i) {
            state[i] += working[i];
        }
    }

    std::array<uint8_t, 64> buffer{};
    size_t bufferSize{0};
    uint64_t bitLength{0};
    std::array<uint32_t, 8> state{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
};
}

std::string CalculateFileSha256(const std::string &filePath, std::string_view salt) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    Sha256 hash;
    hash.Update(salt.data(), salt.size());
    std::vector<char> buffer(HASH_BUFFER_SIZE);
    while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || file.gcount() > 0) {
        hash.Update(buffer.data(), static_cast<size_t>(file.gcount()));
    }
    if (!file.eof()) {
        return "";
    }
    return hash.Final();
}
}
