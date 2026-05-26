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

#ifndef PROFILER_SERVER_MEMORY_DATABASE_H
#define PROFILER_SERVER_MEMORY_DATABASE_H
#include "ProtocolMessage.h"
#include "VirtualMemoryDataBase.h"
#include "MemoryProtocolRespose.h"
#include "MemoryProtocolRequest.h"
#include "MemoryDef.h"

namespace Dic {
namespace Module {
namespace Memory {
using componentDtoVector = std::vector<Protocol::ComponentDto>;
class TextMemoryDataBase : public VirtualMemoryDataBase {
  public:
    explicit TextMemoryDataBase(std::recursive_mutex &sqlMutex);
    ~TextMemoryDataBase() override;

    bool OpenDb(const std::string &dbPath, bool clearAllTable) override;
    bool SetConfig() override;
    bool CreateTable();
    bool DropTable();
    bool InitStmt();
    void ReleaseStmt();

    void InsertOperatorDetailList(const std::vector<Operator> &eventList);
    void InsertOperatorDetail(const Operator &event);
    void InsertRecordDetailList(const std::vector<Record> &eventList);
    void InsertRecordDetail(const Record &event);
    void InsertStaticOpDetailList(const std::vector<StaticOp> &eventList);
    void InsertStaticOpDetail(const StaticOp &event);
    void InsertComponentDetailList(const std::vector<Component> &eventList);
    void InsertComponentDetail(const Component &event);

    bool QueryMemoryType(std::string &type, std::vector<std::string> &graphId) override;
    bool QueryMemoryResourceType(std::string &type) override;
    int64_t QueryOperatorDetail(
        Protocol::MemoryOperatorParams &requestParams, std::vector<Protocol::MemoryOperator> &opDetails) override;
    bool QueryComponentDetail(Protocol::MemoryComponentParams &requestParams,
        std::vector<Protocol::MemoryTableColumnAttr> &columnAttr,
        std::vector<Protocol::MemoryComponent> &componentDetails) override;
    bool QueryMemoryView(Protocol::MemoryViewParams &requestParams, Protocol::MemoryViewData &operatorBody,
        uint64_t offsetTime) override;

    int64_t QueryStaticOperatorList(Protocol::StaticOperatorListParams &requestParams,
        std::vector<Protocol::StaticOperatorItem> &opDetails) override;

    bool QueryStaticOperatorGraph(
        Protocol::StaticOperatorGraphParams &requestParams, Protocol::StaticOperatorGraphItem &graphItem) override;

    void SaveRecordDetail();
    void SaveOperatorDetail();
    void SaveStaticOpDetail();
    void SaveComponentDetail();

    bool QueryComponentsTotalNum(Protocol::MemoryComponentParams &requestParams, int64_t &totalNum) override;
    bool QueryOperatorSize(Protocol::MemoryOperatorSizeParams &requestParams, double &min, double &max) override;
    bool QueryStaticOperatorSize(Protocol::StaticOperatorSizeParams &requestParams, double &min, double &max) override;
    bool QueryEntireOperatorTable(Protocol::MemoryOperatorParams &requestParams,
        std::vector<Protocol::MemoryOperator> &opDetails, uint64_t offsetTime) override;
    bool QueryEntireComponentTable(Protocol::MemoryComponentParams &requestParams,
        std::vector<Protocol::MemoryComponent> &componentDetails, uint64_t offsetTime) override;
    bool QueryEntireStaticOperatorTable(Protocol::StaticOperatorListParams &requestParams,
        std::vector<Protocol::StaticOperatorItem> &opDetails) override;
    uint64_t QueryMinOperatorAllocationTime();
    uint64_t QueryMinRecordTimestamp();
    uint64_t QueryMinComponentTimestamp();

    bool UpdateParseStatus(const std::string &status);
    bool HasFinishedParseLastTime();
    void GetSelectOperatorMemoryColumnAndAlias(
        std::string_view columnKey, uint64_t baseTimestamp, std::string &column, std::string &alias) override;
    MemoryDataBaseContext GetMemoryDbContext() override;

  private:
    // 动态图表格数据在数据库中存储表名为operator，全量DB对应表名OP_MEMORY
    const std::string operatorTable = "operator";
    // 动态图折线图数据在数据库中存储表名为record，全量DB对应表名MEMORY_RECORD
    const std::string recordTable = "record";
    // 静态图表格折线图数据在数据库中存储表名为static_op，全量DB无对应表
    const std::string staticOpTable = "static_op";
    // 组件级表格数据在数据库中存储表名为module，全量DB对应表名NPU_MODULE_MEM
    const std::string componentTable = "module";
    const std::string memoryParseStatus = "Memory files parsing status";

    bool hasInitStmt = false;
    const uint32_t cacheSize = 100;
    std::vector<Operator> operatorCache;
    std::vector<Record> recordCache;
    std::vector<StaticOp> staticOpCache;
    std::vector<Component> componentCache;

    sqlite3_stmt *insertOperatorStmt = nullptr;
    sqlite3_stmt *insertRecordStmt = nullptr;
    sqlite3_stmt *insertStaticOpStmt = nullptr;
    sqlite3_stmt *insertComponentStmt = nullptr;

    sqlite3_stmt *GetOperatorStmt(uint64_t paramLen);
    sqlite3_stmt *GetRecordStmt(uint64_t paramLen);
    sqlite3_stmt *GetStaticOpStmt(uint64_t paramLen);
    sqlite3_stmt *GetComponentStmt(uint64_t paramLen);

    std::string GetOperatorSql(Protocol::MemoryOperatorParams &requestParams);
    std::string GetStaticOperatorSql(Protocol::StaticOperatorListParams &requestParams);
    void GenerateGetStaticGraphNodeIndexSql(
        Protocol::StaticOperatorGraphParams &requestParams, std::string &startSql, std::string &endSql);

    std::string GetCreateOperatorMemoryTableSql();
    std::string GetCreateStaticOpTableSql() const;

    const std::string COMPONENT_APP = "APP";
    const std::string COMPONENT_GE = "GE";
    const std::string COMPONENT_PTA = "PTA";
    const std::string COMPONENT_PTA_AND_GE = "PTA+GE";
};

} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_MEMORY_DATABASE_H
