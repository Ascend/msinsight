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

#ifndef PROFILER_SERVER_CLUSTERFILEPARSER_H
#define PROFILER_SERVER_CLUSTERFILEPARSER_H

#include <string>
#include <vector>
#include "VirtualClusterDatabase.h"
#include "ClusterDef.h"
#include "DbClusterDataBase.h"
#include "pch.h"

namespace Dic {
namespace Module {
namespace Timeline {
enum class AttDataType { TEXT, DB };
class ClusterFileParser {
  public:
    ClusterFileParser(
        const std::string &filePath, std::shared_ptr<VirtualClusterDatabase> database, const std::string &uniqueKey);
    bool ParseClusterFiles();
    bool ParseClusterStep2Files();
    void ParseCommunication(const std::vector<std::string> &filePathList);
    void ParseStepStatisticsFile(const std::vector<std::string> &filePathList);
    void SaveClusterBaseInfo(const std::string &selectedPath);
    void ParseCommunicationMatrix(const std::vector<std::string> &filePathList);
    bool ParserClusterOfDb();
    std::string GetClusterDbPath();
    static bool CheckIsCluster(const std::string &filePath);
    inline std::string GetClusterPath() const { return selectedFilePath; }

  private:
    void SaxParseJsonFile(const std::string &filePath, int saxHandlerType);
    bool InitClusterDatabase();
    StepStatistic MapToStepStatistic(std::map<std::string, size_t> &dataMap, const std::vector<std::string> &tokens);
    static void InitFullDbClusterBaseInfo(
        std::shared_ptr<FullDb::DbClusterDataBase> &clusterDatabase, ClusterBaseInfo &baseInfo);
    size_t subStrlen = 2;
    std::string clusterDbPath;
    bool needClearDb = true;
    std::string selectedFilePath;
    std::string uniqueKey;
    const std::string CLUSTER_IDENTIFY = "cluster_";
    std::shared_ptr<VirtualClusterDatabase> database;
    static bool AttAnalyze(
        const std::string &selectedPath, const std::string &mode, AttDataType dataType = AttDataType::TEXT);
    bool TransCommunicationToDb(const std::string &selectedPath, const std::regex &patternCommunication);
    bool InitCommunicationGroupInfo(std::vector<CommGroupParallelInfo> &groupInfos);
    static bool CheckDocumentValid(const Document &doc);
    static std::string GetStrValue(
        std::map<std::string, size_t> &dataMap, const std::vector<std::string> &tokens, const std::string &key);
    bool SkipClusterParse();
    bool InitBaseInfoAndMatrixData();
    bool BackupExistedClusterFiles(const std::vector<std::string> &backUpMatrixList,
        const std::vector<std::string> &backUpGroupList, const std::vector<std::string> &backUpStepList);
    bool RestoreClusterFiles(const std::vector<std::string> &backUpMatrixList,
        const std::vector<std::string> &backUpGroupList, const std::vector<std::string> &backUpStepList);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_CLUSTERFILEPARSER_H
