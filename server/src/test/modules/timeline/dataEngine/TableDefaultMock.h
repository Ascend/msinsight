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

#ifndef PROFILER_SERVER_TABLEDEFAULTMOCK_H
#define PROFILER_SERVER_TABLEDEFAULTMOCK_H
#include "EnumApiTypeTable.h"
#include "CANNApiTable.h"
#include "CommucationOpTable.h"
#include "TaskTable.h"
#include "CommucationTaskInfoTable.h"
#include "EnumHcclLinkTypeTable.h"
#include "EnumHcclTransportTypeTable.h"
#include "EnumHcclRdmaTypeTable.h"
#include "EnumHcclDataTypeTable.h"
#include "MstxEventsTable.h"
#include "EnumMstxEventTypeTable.h"
#include "ComputeTaskInfoTable.h"
#include "PytorchApiTable.h"
#include "PytorchCallchainsTable.h"
#include "PythonGCTable.h"
#include "TaskPmuInfoTable.h"
using namespace Dic::Module::Timeline;
namespace Dic::TimeLine::Table::Default::Mock {
class TableDefaultMock {
  public:
    void SetDb(sqlite3 *dbPtr) { db = dbPtr; }

  protected:
    sqlite3 *db = nullptr;
};
class CommucationOpTableMock : public CommucationOpTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskOpPO> &result) override {
        CommucationOpTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class StringIdsTableMock : public StringIdsTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<StringIdsPO> &result) override {
        StringIdsTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class TaskTableMock : public TaskTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<TaskPO> &result) override {
        TaskTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class CommucationTaskInfoTableMock : public CommucationTaskInfoTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskInfoPO> &result) override {
        CommucationTaskInfoTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class EnumHcclLinkTypeTableMock : public EnumHcclLinkTypeTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<EnumHcclLinkTypePO> &result) override {
        EnumHcclLinkTypeTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class EnumHcclTransportTypeTableMock : public EnumHcclTransportTypeTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<EnumHcclTransportTypePO> &result) override {
        EnumHcclTransportTypeTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class EnumHcclRdmaTypeTableMock : public EnumHcclRdmaTypeTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<EnumHcclRdmaTypePO> &result) override {
        EnumHcclRdmaTypeTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class EnumHcclDataTypeTableMock : public EnumHcclDataTypeTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<EnumHcclDataTypePO> &result) override {
        EnumHcclDataTypeTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class EnumApiTypeTableMock : public EnumApiTypeTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<EnumApiTypePO> &result) override {
        EnumApiTypeTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class CANNApiTableMock : public CANNApiTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<CANNApiPO> &result) override {
        CANNApiTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};
class MstxEventsTableMock : public MstxEventsTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<MstxEventsPO> &result) override {
        MstxEventsTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

class PythonGCTableMock : public PythonGCTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<PythonGCPO> &result) override {
        PythonGCTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

class EnumMstxEventTypeTableMock : public EnumMstxEventTypeTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<EnumMstxEventTypePO> &result) override {
        EnumMstxEventTypeTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

class ComputeTaskInfoTableMock : public ComputeTaskInfoTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<ComputeTaskInfoPO> &result) override {
        ComputeTaskInfoTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

class PytorchApiTableMock : public PytorchApiTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<PytorchApiPO> &result) override {
        PytorchApiTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

class PytorchCallchainsTableMock : public PytorchCallchainsTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<PytorchCallchainsPO> &result) override {
        PytorchCallchainsTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

class TaskPmuInfoTableMock : public TaskPmuInfoTable, public TableDefaultMock {
  protected:
    void ExcuteQuery(const std::string &fileId, std::vector<TaskPmuInfoPO> &result) override {
        TaskPmuInfoTable::ExcuteQuery(db, result);
        ClearThreadLocal();
    }
};

struct PytorchApiDependency {
    std::unique_ptr<StringIdsTableMock> stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    std::unique_ptr<PytorchApiTableMock> pytorchApiTableMock = std::make_unique<PytorchApiTableMock>();
    std::unique_ptr<PytorchCallchainsTableMock> pytorchCallchainsTableMock =
        std::make_unique<PytorchCallchainsTableMock>();
};

struct HardWareDependency {
    std::unique_ptr<StringIdsTableMock> stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    std::unique_ptr<TaskTableMock> taskTableMock = std::make_unique<TaskTableMock>();
    std::unique_ptr<ComputeTaskInfoTableMock> computeTaskInfoTableMock = std::make_unique<ComputeTaskInfoTableMock>();
    std::unique_ptr<TaskPmuInfoTableMock> taskPmuInfoTableMock = std::make_unique<TaskPmuInfoTableMock>();
};

struct CANNDependency {
    std::unique_ptr<StringIdsTableMock> stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    std::unique_ptr<CANNApiTableMock> cannApiTableMock = std::make_unique<CANNApiTableMock>();
    std::unique_ptr<EnumApiTypeTableMock> enumApiTypeTableMock = std::make_unique<EnumApiTypeTableMock>();
};

struct MstxDependency {
    std::unique_ptr<StringIdsTableMock> stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    std::unique_ptr<MstxEventsTableMock> mstxEventsTableMock = std::make_unique<MstxEventsTableMock>();
    std::unique_ptr<EnumMstxEventTypeTableMock> enumMstxEventTypeTableMock =
        std::make_unique<EnumMstxEventTypeTableMock>();
};

struct PythonGcDependency {
    std::unique_ptr<PythonGCTableMock> tableMock = std::make_unique<PythonGCTableMock>();
};

struct HcclDependency {
    std::unique_ptr<CommucationOpTableMock> commucationOpTableMock = std::make_unique<CommucationOpTableMock>();
    std::unique_ptr<StringIdsTableMock> stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    std::unique_ptr<TaskTableMock> taskTableMock = std::make_unique<TaskTableMock>();
    std::unique_ptr<CommucationTaskInfoTableMock> commucationTaskInfoTableMock =
        std::make_unique<CommucationTaskInfoTableMock>();
    std::unique_ptr<EnumHcclLinkTypeTableMock> enumHcclLinkTypeTableMock =
        std::make_unique<EnumHcclLinkTypeTableMock>();
    std::unique_ptr<EnumHcclTransportTypeTableMock> enumHcclTransportTypeTableMock =
        std::make_unique<EnumHcclTransportTypeTableMock>();
    std::unique_ptr<EnumHcclRdmaTypeTableMock> enumHcclRdmaTypeTableMock =
        std::make_unique<EnumHcclRdmaTypeTableMock>();
    std::unique_ptr<EnumHcclDataTypeTableMock> enumHcclDataTypeTableMock =
        std::make_unique<EnumHcclDataTypeTableMock>();
};
}

#endif // PROFILER_SERVER_TABLEDEFAULTMOCK_H
