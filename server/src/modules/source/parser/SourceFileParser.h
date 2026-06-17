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

#ifndef PROFILER_SERVER_SOURCEFILEPARSER_H
#define PROFILER_SERVER_SOURCEFILEPARSER_H

#include <future>
#include <map>
#include <unordered_map>
#include "document.h"
#include "rapidjson.h"
#include "document.h"
#include "FileParser.h"
#include "ThreadPool.h"
#include "SourceProtocolResponse.h"
#include "JsonFileProcess.h"
#include "SourceProtocol.h"
#include "SourceInstructionParser.h"
#include "SafeFile.h"

namespace Dic {
namespace Module {
namespace Source {
class SourceFileParser : public FileParser {
public:
    const static uint16_t reverseConst = 0x5a5a;
    static SourceFileParser &Instance();

    SourceFileParser();
    ~SourceFileParser() override;

    bool Parse(const std::vector<std::string> &filePaths,
               const std::string &rankId,
               const std::string &selectedFile,
               const std::string &fileId) override;
    void Reset() override;
    bool CheckOperatorBinary(const std::string &selectedFilePath, std::string &errMsg);
    static void PreParseTask(const std::string &rankId, const std::string &fileId);
    static bool InitParser(const std::string &rankId, const std::string &fileId);
    static void ParseEndCallBack(const std::string &rankId,
                                 bool result,
                                 const std::string &message,
                                 const std::string &fileId);
    static void ParseTask(const std::string &rankId, std::pair<int64_t, int64_t> pos, const std::string &fileId);
    static void EndParseTask(const std::string &rankId,
                             std::shared_ptr<std::vector<std::future<void>>> futures,
                             const std::string &fileId);
    std::vector<std::string> GetCoreList();
    std::vector<std::string> GetSourceList();
    std::vector<SourceFileLine> GetApiLinesByCoreAndSource(const std::string &core, const std::string &sourceName);
    std::vector<SourceFileLineDynamicCol> GetApiLinesDynamic(const std::string &core, const std::string &sourceName);
    std::string GetInstr();
    std::vector<SourceApiInstruction> GetInstructions(std::string &coreName);
    std::vector<SourceFileInstructionDynamicCol> GetInstrDynamic(std::string &coreName);
    std::map<std::string, int> GetInstructionColumnTypeMap();
    std::map<std::string, int> GetSourceLineColumnTypeMap();
    std::string GetSourceByName(std::string &sourceName);
    bool GetDetailsBaseInfo(Protocol::DetailsBaseInfoResBody &responseBody, bool isBaseline);
    bool GetDetailsLoadInfo(Protocol::DetailsLoadInfoResBody &responseBody, bool isBaseline);
    bool GetDetailsMemoryGraph(const std::string& targetBlockId, bool isBaseline,
                               Protocol::DetailsMemoryGraphResBody &responseBody);
    bool GetDetailsMemoryTable(const std::string& targetBlockId, bool isBaseline,
                               Protocol::DetailsMemoryTableResBody &responseBody);
    void ConvertToData();
    bool GetDetailsInterCoreLoadAnalysisGraph(Protocol::DetailsInterCoreLoadGraphBody& responseBody, bool isBaseline);
    bool GetDetailsRoofline(Protocol::DetailsRooflineBody &responseBody);
    void SetFilePath(const std::string &inputFilePath);
    void SetBaselineFilePath(const std::string &inputFilePath);
    bool IsBaselineParsed(const std::string &inputFilePath);
    void SynchronizeBaselineInfo();
    void ResetBaseline();
    std::vector<Position> GetPositionByType(DataTypeEnum type);
    bool HasCachelineRecords();
    bool GetTopWarpStallReason(std::vector<Protocol::StallReasonItem> &data);
    int8_t GetInstrVersion() const;
    std::string GetFilePath();
private:
    std::string filePath;
    std::map<int, std::vector<Position>> dataBlockMap;
    std::map<std::string, std::pair<int64_t, int64_t>> traceFiles;
    SourceInstructionParser sourceInstructionParser;

    std::string baselineFilePath;
    std::map<int, std::vector<Position>> baselineDataBlockMap;

    static uint64_t CalculateTotalSize(std::vector<std::pair<int64_t, int64_t>> &filePos);

    std::unique_ptr<ThreadPool> threadPool;
    const int maxThreadNum = 4;

    std::mutex mutex;
    std::mutex trackMutex;
    std::mutex processMutex;
    std::mutex threadMutex;
    std::unordered_map<std::string, std::map<std::pair<std::string, int64_t>, int64_t>> trackIdMap;
    std::unordered_map<std::string, std::map<std::string, int64_t>> simulationPidMap;
    std::unordered_map<std::string, std::map<std::pair<std::string, std::string>, int64_t>> simulationTidMap;
    int64_t trackId = 0;
    int64_t pid = 0;
    int64_t tid = 0;
    int8_t instrVersion = 0x5a;

    const int dataSizeLen = 8; // 数据类型字段距离数据大小字段的偏移
    const int dataTypeLen = 1; // 填充长度字段距离数据类型字段的偏移
    const int paddingLen = 1;  // 填充长度字段距离数据类型字段的偏移
    /* 实际数据距离填充长度字段的偏移,在25年3月迭代高八位用作版本标识
    ** 0标识新版本，地址用AscendC Inner Code做映射
    ** 0x5a 老版本，用addressrange 做映射
    */
    const int instrVersionLen = 1;

    const int reserveLen = 1;
    const int filePathLen = 4096;

    bool ParseDataBlocks(std::ifstream &file, long long fileSize,
                         std::map<int, std::vector<Position>> &curDataBlockMap);
};
} // end of namespace Summary
} // end of namespace Module
} // end of namespace Dic


#endif // PROFILER_SERVER_SOURCEFILEPARSER_H
