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

#ifndef PROFILER_SERVER_TESTCASEDATABASEUTIL_H
#define PROFILER_SERVER_TESTCASEDATABASEUTIL_H
#include <sqlite3.h>
#include <string>
namespace Dic::Protocol {
using namespace Dic::Module::Timeline;
}
namespace Dic::TimeLine::TestCaseUtil {
class TestCaseDatabaseUtil {
  public:
    static void CreateDatabse(sqlite3 *&db, const std::string &sql) {
        // 在内存中创建SQLite数据库
        int rc = sqlite3_open(":memory:", &db);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            return;
        }
        rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_free(nullptr);
            sqlite3_close(db);
            return;
        }
    }

    static void InsertData(sqlite3 *&db, const std::string &sql) {
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_free(nullptr);
            sqlite3_close(db);
            return;
        }
    }
};
}
#endif // PROFILER_SERVER_TESTCASEDATABASEUTIL_H
