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

#ifndef PROFILER_SERVER_KERNELPARSE_H
#define PROFILER_SERVER_KERNELPARSE_H

#include <string>
#include <map>
#include <set>
#include <functional>
#include "SummaryDef.h"
#include "ThreadPool.h"
#include "TextSummaryDataBase.h"
#include "FileParser.h"
#include "ConstantDefs.h"

namespace Dic::Module::Summary {
// 表头字段-公共部分
const std::string FIELD_TASK_ID = "Task ID";
const std::string FIELD_OP_STATE = "OP State";
const std::string FIELD_BLOCK_DIM = "Block Dim";
const std::string FIELD_BLOCK_NUM = "Block Num";
const std::string FIELD_INPUT_SHAPES = "Input Shapes";
const std::string FIELD_INPUT_DATA_TYPES = "Input Data Types";
const std::string FIELD_INPUT_FORMATS = "Input Formats";
const std::string FIELD_OUTPUT_SHAPES = "Output Shapes";
const std::string FIELD_OUTPUT_DATA_TYPES = "Output Data Types";
const std::string FIELD_OUTPUT_FORMATS = "Output Formats";

// 表头字段-Ascend PyTorch Profiler
const std::string FIELD_NAME = "Name";
const std::string FIELD_TYPE = "Type";
const std::string FIELD_ACCELERATOR_CORE = "Accelerator Core";
const std::string FIELD_START_TIME = "Start Time(us)";
const std::string FIELD_DURATION = "Duration(us)";
const std::string FIELD_WAIT_TIME = "Wait Time(us)";

// 表头字段-msprof
const std::string FIELD_OP_NAME = "Op Name";
const std::string FIELD_OP_TYPE = "OP Type";
const std::string FIELD_TASK_TYPE = "Task Type";
const std::string FIELD_TASK_START_TIME = "Task Start Time(us)";
const std::string FIELD_TASK_DURATION = "Task Duration(us)";
const std::string FIELD_TASK_WAIT_TIME = "Task Wait Time(us)";
const std::string FIELD_AICORE_TIME = "aicore_time(us)";

const std::vector<std::vector<std::string>> VALID_HEADERS = {
    {FIELD_ACCELERATOR_CORE, FIELD_BLOCK_DIM, FIELD_DURATION, FIELD_NAME, FIELD_START_TIME, FIELD_TYPE,
        FIELD_WAIT_TIME}, // pytorch header1
    {FIELD_ACCELERATOR_CORE, FIELD_BLOCK_NUM, FIELD_DURATION, FIELD_NAME, FIELD_START_TIME, FIELD_TYPE,
        FIELD_WAIT_TIME}, // pytorch header1
    {FIELD_ACCELERATOR_CORE, FIELD_BLOCK_DIM, FIELD_DURATION, FIELD_NAME, FIELD_TASK_START_TIME, FIELD_TYPE,
        FIELD_WAIT_TIME}, // pytorch header2
    {FIELD_ACCELERATOR_CORE, FIELD_BLOCK_NUM, FIELD_DURATION, FIELD_NAME, FIELD_TASK_START_TIME, FIELD_TYPE,
        FIELD_WAIT_TIME}, // pytorch header2
    {
        FIELD_BLOCK_DIM,
        FIELD_OP_TYPE,
        FIELD_OP_NAME,
        FIELD_TASK_DURATION,
        FIELD_TASK_START_TIME,
        FIELD_TASK_TYPE,
        FIELD_TASK_WAIT_TIME,
    }, // msprof header
    {
        FIELD_BLOCK_NUM,
        FIELD_OP_TYPE,
        FIELD_OP_NAME,
        FIELD_TASK_DURATION,
        FIELD_TASK_START_TIME,
        FIELD_TASK_TYPE,
        FIELD_TASK_WAIT_TIME,
    } // msprof header
};

class KernelParse : public FileParser {
  public:
    static KernelParse &Instance();
    KernelParse();
    ~KernelParse() override;
    void Reset() override;

    bool Parse(const std::vector<std::string> &filePaths, const std::string &rankId, const std::string &selectedFolder,
        const std::string &fileId) override;
    bool Parse(const RankEntry &rankEntry);

    static std::vector<std::string> GetKernelFiles(const std::vector<std::string> &paths);

  protected:
    static bool ParseKernelCsv(const std::string &filePath, const std::string &rankId, const std::string &statusId,
        std::string &message, std::set<std::string> &devices);
    static bool CheckHeaderFieldAndFilterParseFunc(std::vector<std::string> rowVector,
        std::vector<std::function<void(const std::map<std::string, size_t> &dataMap,
            const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel)>> &parseFuncList);
    static bool GetUtilizationColumns(const std::vector<std::string> &rowVector, std::vector<std::string> &columns);

  private:
    const int maxThreadNum = 4;
    std::unique_ptr<ThreadPool> threadPool;

    static std::map<std::vector<std::string>,
        std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
            const std::string &fileId, Kernel &kernel)>>
        parseFuncMap;
    const std::string kernelDetailReg = R"((kernel_details|op_summary[_\d]*)\.csv$)";

    static void InitKernelParseMap();

    static void SetParseCallBack();
    static void ParseEndCallBack(
        const std::string &rankId, const std::string &fileId, bool result, const std::string &msg);
    static void ParseCallBack(
        const std::string &rankId, const std::string &fileId, bool result, const std::string &msg);

    static void PreParseTask(
        const std::vector<std::string> &filePathList, const std::string &rankId, const std::string &fileId);
    static bool ParseTask(
        const std::vector<std::string> &filePathList, const std::string &fileId, std::string &message);

    static bool InitParser(const std::vector<std::string> &filePathList, const std::string &rankId,
        const std::string &fileId, std::string &message);
    static void PostParseTask(
        const std::set<std::string> &devices, const std::string &rankId, const std::string &fileId);

    static void ParsePyTorchOpBaseInfoData(const std::map<std::string, size_t> &dataMap,
        const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel);
    static void ParseShapeInfoData(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel);
    static void ParseOpStateInfoData(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel);
    static void ParseMsProfOpBaseInfoData(const std::map<std::string, size_t> &dataMap,
        const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel);
    static void ParsePyTorchStepInfoData(const std::map<std::string, size_t> &dataMap,
        const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel);
    static void ParseTaskIdInfoData(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel);
    static void ParseStartTimeInfoData(const std::map<std::string, size_t> &dataMap,
        const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel);
    static void ParseTaskStartTimeInfoData(const std::map<std::string, size_t> &dataMap,
        const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel);
    static void ParseAICoreMetricsInfoData(const std::map<std::string, size_t> &dataMap,
        const std::vector<std::string> &row, const std::string &fileId, Kernel &kernel);
    static bool ProcessHeaderGetParseFunc(std::shared_ptr<TextSummaryDataBase> db, std::vector<std::string> &rowVector,
        std::vector<std::string> &columns, std::map<std::string, size_t> &dataMap,
        std::vector<std::function<void(const std::map<std::string, size_t> &dataMap,
            const std::vector<std::string> &rows, const std::string &fileId, Kernel &kernel)>> &parseFuncList);

    static bool blockDimReplacedByBlockNum;
};

} // end of namespace Dic::Module::Summary

#endif // PROFILER_SERVER_KERNELPARSE_H
