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

#ifndef PROFILER_SERVER_DBMEMORYDATABASE_H
#define PROFILER_SERVER_DBMEMORYDATABASE_H

#include <map>
#include "VirtualMemoryDataBase.h"
#include "TimelineProtocolEvent.h"

namespace Dic {
namespace Module {
namespace FullDb {
class DbMemoryDataBase : public Memory::VirtualMemoryDataBase {
  public:
    explicit DbMemoryDataBase(std::recursive_mutex &sqlMutex) : Memory::VirtualMemoryDataBase(sqlMutex) {};
    ~DbMemoryDataBase() override = default;

    bool OpenDb(const std::string &dbPath, bool clearAllTable) override;
    bool QueryMemoryType(std::string &type, std::vector<std::string> &graphId) override;
    bool QueryMemoryResourceType(std::string &type) override;
    int64_t QueryOperatorDetail(
        Protocol::MemoryOperatorParams &requestParams, std::vector<Protocol::MemoryOperator> &opDetails) override;
    bool QueryComponentDetail(Protocol::MemoryComponentParams &requestParams,
        std::vector<Protocol::MemoryTableColumnAttr> &columnAttr,
        std::vector<Protocol::MemoryComponent> &componentDetails) override;
    bool QueryMemoryView(Protocol::MemoryViewParams &requestParams, Protocol::MemoryViewData &operatorBody,
        uint64_t offsetTime) override;
    bool QueryComponentsTotalNum(Protocol::MemoryComponentParams &requestParams, int64_t &totalNum) override;
    bool QueryOperatorSize(Protocol::MemoryOperatorSizeParams &requestParams, double &min, double &max) override;
    bool QueryStaticOperatorSize(Protocol::StaticOperatorSizeParams &requestParams, double &min, double &max) override;

    int64_t QueryStaticOperatorList(Protocol::StaticOperatorListParams &requestParams,
        std::vector<Protocol::StaticOperatorItem> &opDetails) override;

    bool QueryStaticOperatorGraph(
        Protocol::StaticOperatorGraphParams &requestParams, Protocol::StaticOperatorGraphItem &graphItem) override;

    bool QueryEntireOperatorTable(Protocol::MemoryOperatorParams &requestParams,
        std::vector<Protocol::MemoryOperator> &opDetails, uint64_t offsetTime) override;
    bool QueryEntireComponentTable(Protocol::MemoryComponentParams &requestParams,
        std::vector<Protocol::MemoryComponent> &componentDetails, uint64_t offsetTime) override;
    bool QueryEntireStaticOperatorTable(Protocol::StaticOperatorListParams &requestParams,
        std::vector<Protocol::StaticOperatorItem> &opDetails) override;

    static void ParserEnd(std::string rankId, bool result, std::string fileId);
    static void ParseCallBack(
        const std::string &rankId, const std::string &fileId, bool result, const std::string &msg);
    std::map<std::string, Protocol::MemorySuccess> GetRanks();
    static void Reset();
    void GetSelectOperatorMemoryColumnAndAlias(
        std::string_view columnKey, uint64_t baseTimestamp, std::string &column, std::string &alias) override;
    MemoryDataBaseContext GetMemoryDbContext() override;

  private:
    static std::map<std::string, Protocol::MemorySuccess> ranks;
    std::string BuildOperatorDetailSql(const uint64_t baseTimestamp);
    static std::string GetJoinStringIDSAlias(std::string_view joinCol);
    std::string deviceIdColumnName;
    const std::set<std::string_view> OPERATOR_MEMORY_TIMESTAMP_NS_COLUMNS_SET = {OpMemoryColumn::ALLOCATION_TIME,
        OpMemoryColumn::RELEASE_TIME, OpMemoryColumn::ACTIVE_RELEASE_TIME, OpMemoryColumn::DURATION,
        OpMemoryColumn::ACTIVE_DURATION};
};
}
}
}

#endif // PROFILER_SERVER_DBMEMORYDATABASE_H
