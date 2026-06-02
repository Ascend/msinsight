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

#ifndef PROFILER_SERVER_CURVEREPO_H
#define PROFILER_SERVER_CURVEREPO_H
#include "ServitizationContext.h"
#include "BaseDomain.h"
namespace Dic::Module::IE {
class CurveRepo {
  public:
    virtual ~CurveRepo() = default;
    virtual std::vector<std::string> QueryAllViews(const std::string &fileId);
    virtual std::vector<ColumnAtt> QueryTableInfoByName(const std::string &fileId, const std::string &tableName);
    virtual std::string QueryTableNameDesc(const std::string &fileId, const std::string &tableName, bool isZh);
    virtual std::vector<std::map<std::string, std::string>> QueryDataByColumn(
        const std::string &fileId, const std::string &tableName, const std::vector<ColumnAtt> &columns);
    virtual std::vector<std::map<std::string, std::string>> QueryDataByColumnPage(
        const PageQuery &query, const std::vector<ColumnAtt> &columns);
    virtual uint64_t QueryCountByTableName(const PageQuery &query, const std::string &abscissa = "");

  protected:
    std::shared_ptr<ServitizationContext> context = std::make_shared<ServitizationContext>();

    std::vector<std::string> ComputeColNames(const PageQuery &query, const std::vector<ColumnAtt> &columns) const;

    std::string &AppendConditionSql(const PageQuery &query, std::string &sql) const;
};
}
#endif // PROFILER_SERVER_CURVEREPO_H
