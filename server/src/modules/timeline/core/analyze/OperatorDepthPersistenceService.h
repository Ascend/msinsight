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
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_OPERATORDEPTHPERSISTENCESERVICE_H
#define PROFILER_SERVER_OPERATORDEPTHPERSISTENCESERVICE_H

#include <functional>
#include <string>

namespace Dic::Module {
class Database;

namespace FullDb {
class DbTraceDataBase;
}

namespace Timeline {
class TextTraceDatabase;

class OperatorDepthPersistenceService {
  public:
    static bool CalculateAndPersistTextDepth(TextTraceDatabase &database, const std::string &rankId);
    static bool CalculateAndPersistDbDepth(FullDb::DbTraceDataBase &database, const std::string &dbId);

  protected:
    using PersistenceWork = std::function<bool()>;
    static bool RunWithStatusAndTransaction(Database &database, const PersistenceWork &work);
};
}
}

#endif // PROFILER_SERVER_OPERATORDEPTHPERSISTENCESERVICE_H
