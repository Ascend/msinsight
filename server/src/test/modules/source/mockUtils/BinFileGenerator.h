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

#ifndef PROFILER_SERVER_BINFILEGENERATOR_H
#define PROFILER_SERVER_BINFILEGENERATOR_H

#include <utility>

#include <istream>
#include "DataBlock.h"

namespace Dic::Module::Source::Test {

class BinFileGenerator {
  public:
    void AddDataBlock(std::unique_ptr<DataBlock> blockPtr) { dataBlocks.push_back(std::move(blockPtr)); }

    void Generate(const std::string &outputPath);
    static bool RemoveFile(const std::string &path);
    bool RemoveFile();

  private:
    std::vector<std::unique_ptr<DataBlock>> dataBlocks;
    std::string filePath;
};

}

#endif // PROFILER_SERVER_BINFILEGENERATOR_H
