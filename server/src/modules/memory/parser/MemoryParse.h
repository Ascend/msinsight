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

#ifndef PROFILER_SERVER_MEMORYPARSE_H
#define PROFILER_SERVER_MEMORYPARSE_H

#include <string>
#include <map>
#include <functional>
#include "GlobalDefs.h"
#include "TextMemoryDataBase.h"
#include "ThreadPool.h"
#include "MemoryDef.h"
#include "FileParser.h"
#include "ConstantDefs.h"
#include "CurveContainer.h"
#include "TimelineProtocolEvent.h"

namespace Dic {
namespace Module {
namespace Memory {

class MemoryParse : public FileParser {
  public:
    static MemoryParse &Instance();
    MemoryParse();

    ~MemoryParse() override;
    bool Parse(const std::vector<std::string> &filePaths, const std::string &rankId, const std::string &selectedFolder,
        const std::string &fileId) override;
    bool Parse(const RankEntry &rankEntry);
    void Reset() override;
    bool OperatorParse(const std::string &filePath, const std::string &rankId);
    bool RecordToParse(const std::string &filePath, const std::string &fileId);
    bool StaticOpParse(const std::string &filePath, const std::string &fileId);
    bool ComponentParse(const std::string &filePath, const std::string &fileId);

    static bool CheckRequiredColumnsAndBuildDataMapFromHeaderRow(const std::vector<std::string> &headerRow,
        const std::vector<std::string> &requiredHeaders, std::map<std::string, size_t> &dataMap);

    static bool NeedInterrupt(const std::string &fileId);

    static bool ParseOperatorHeaderLine(std::map<std::string, size_t> &dataMap, const std::vector<std::string> &row);

    static Operator ParseOperatorDataLine(std::map<std::string, size_t> &dataMap, std::vector<std::string> &row);
    MemoryFilePairs GetMemoryFile(const std::string &path);
    CurveView ComputeCurve(double xMin, double xMax, const std::string &input);
    void PutCurve(const std::string &inputKey, CurveView &curve);
    bool Exist(const std::string &inputKey);

  private:
    const uint32_t maxThreadNum = 4;
    const double mbToKb = 1024.0;
    std::map<std::string, Protocol::MemorySuccess> ranks;
    bool isCluster = false;

    std::unique_ptr<TextMemoryDataBase> database;
    std::unique_ptr<ThreadPool> threadPool;
    std::map<std::string, std::future<void>> futureMap;
    CurveContainer curveContainer;

    Record mapperToRecordDetail(std::map<std::string, size_t> dataMap, std::vector<std::string>);
    static Operator mapperToOperatorDetail(std::map<std::string, size_t> &dataMap, std::vector<std::string> &row);
    StaticOp mapperToStaticOpDetail(std::map<std::string, size_t> dataMap, std::vector<std::string>);
    Component mapperToComponentDetail(std::map<std::string, size_t> dataMap, std::vector<std::string>);

    static bool GetMapValid(const std::vector<std::string> &vec, const std::map<std::string, size_t> &dataMap);
    std::vector<std::string> GetPeerDirOperatorFile(const std::string &operatorFile, const std::string &reg);
    std::map<std::string, MemoryFilePairs> GetMemoryFiles(
        const std::vector<std::string> &paths, const std::string &rankId, const std::string &fileId);
    std::vector<std::string> GetMemoryRecordFileLists(const std::vector<std::string> &paths);
    static void SetParseCallBack();
    static void ParseEndCallBack(
        const std::string &rankId, const std::string &fileId, bool result, const std::string &message);
    static void ParseCallBack(
        const std::string &rankId, const std::string &fileId, bool result, const std::string &msg);

    static void PreParseTask(const MemoryFilePairs &filePair, const std::string &fileId);
    static bool ParseTask(const MemoryFilePairs &filePair, const std::string &rankId, std::string &message);
    static bool InitParser(const MemoryFilePairs &filePair, const std::string &fileId, std::string &message);
    static std::string DeleteNPUPrefix(const std::string &str);
};
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_MEMORYPARSE_H
