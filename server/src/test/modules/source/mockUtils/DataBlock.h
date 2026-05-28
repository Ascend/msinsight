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

#ifndef PROFILER_SERVER_DATABLOCK_H
#define PROFILER_SERVER_DATABLOCK_H

#include <iostream>
#include <fstream>
#include <vector>
#include "SourceFileParser.h"

namespace Dic::Module::Source::Test {

using namespace Dic::Module::Source;

/**
 * 文件协议头 4字节对齐
 */
#pragma pack(4)
struct BinaryBlockHeader {
    uint64_t contentSize = 0; // 数据内容的长度
    uint8_t type = 0; // 数据块的类型
    uint8_t padding = 0; // 四字节对齐的补0位数
    uint16_t reverse = 0x5a5a; // 保留位
};
#pragma pack()

/**
 * 代表一个数据块，包含数据头和数据体
 */
class DataBlock {
  public:
    explicit DataBlock(DataTypeEnum type) { header.type = static_cast<uint8_t>(type); }

    /**
     * 将数据块内容写入文件
     *
     * @param file 写入的目标文件
     */
    void Write2File(std::ofstream &file) {
        CalculateHeader();
        WriteHeader(file);
        WriteBody(file);
    }

    /**
     * 计算填充位数
     *
     * @return 填充位数
     */
    virtual uint8_t CalculatePadding() = 0;

    /**
     * 计算数据体长度
     *
     * @return 数据体长度
     */
    virtual uint64_t CalculateContentSize() = 0;

    /**
     * 将数据体写入目标文件
     *
     * @param file 目标文件
     */
    virtual void WriteBody(std::ofstream &file) = 0;

    virtual ~DataBlock() = default;

  protected:
    const int integerBytes_ = 4;
    BinaryBlockHeader header;

  private:
    /**
     * 计算数据头 @BinaryBlockHeader 的信息
     */
    void CalculateHeader() {
        header.padding = CalculatePadding();
        header.contentSize = CalculateContentSize();
    }

    /**
     * 将数据头写入目标文件
     *
     * @param file 目标文件
     */
    void WriteHeader(std::ofstream &file) {
        size_t size = 1;
        WriteStructs2File(file, &header, size);
    }

    /**
     * 将指定类型的结构体数组写入目标文件
     *
     * @tparam T 结构体的类型
     * @param file 目标文件
     * @param data 结构体数组指针
     * @param count 结构体数组长度
     */
    template <typename T> void WriteStructs2File(std::ofstream &file, const T *data, size_t count) const {
        file.write(reinterpret_cast<const char *>(data), sizeof(T) * count);
    }
};

/**
 * Json字符串类型的数据块
 */
class NormalDataBlock : public DataBlock {
  public:
    NormalDataBlock(DataTypeEnum type, const std::string &dataBody) : DataBlock(type), dataBody(dataBody) {}

    uint8_t CalculatePadding() override {
        auto temp = dataBody.size() % integerBytes_;
        if (temp != 0) {
            return header.padding = integerBytes_ - temp;
        }
        return 0;
    }

    uint64_t CalculateContentSize() override {
        if (header.padding != 0) {
            dataBody.resize(dataBody.size() + header.padding, 0);
        }
        return dataBody.size();
    }

    void WriteBody(std::ofstream &file) override { file.write(dataBody.c_str(), dataBody.size()); }

  private:
    std::string dataBody;
};

/**
 * Source Code类型的数据块
 */
class SourceDataBlock : public NormalDataBlock {
  public:
    SourceDataBlock(const std::string &dataBody, const std::string &sourceFilePath)
        : NormalDataBlock(DataTypeEnum::SOURCE, dataBody), sourceFilePath(sourceFilePath) {}

    void WriteBody(std::ofstream &file) override {
        constexpr int pathLength = 4096;
        sourceFilePath.resize(pathLength, '\0');
        file.write(sourceFilePath.c_str(), sourceFilePath.size());
        NormalDataBlock::WriteBody(file);
    }

  private:
    std::string sourceFilePath;
};

template <typename T> class StructDataBlock : public DataBlock {
  public:
    StructDataBlock(DataTypeEnum type, std::vector<T> dataBody) : DataBlock(type), dataBody(dataBody) {}

    uint8_t CalculatePadding() override {
        auto temp = dataBody.size() * sizeof(T) % integerBytes_;
        if (temp != 0) {
            return integerBytes_ - temp;
        }
        return 0;
    }

    uint64_t CalculateContentSize() override { return dataBody.size() * sizeof(T) + header.padding; }

    void WriteBody(std::ofstream &file) override {
        if (dataBody.empty()) {
            return;
        }
        WriteStructs2File(file, dataBody.data(), dataBody.size() * sizeof(T));
        std::string padding;
        padding.resize(header.padding);
        file.write(padding.c_str(), header.padding);
    }

  private:
    std::vector<T> dataBody;
};

}

#endif // PROFILER_SERVER_DATABLOCK_H
