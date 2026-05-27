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

#ifndef PROFILER_SERVER_ABSTRACTPARSEUNIT_H
#define PROFILER_SERVER_ABSTRACTPARSEUNIT_H
#include <string>

#include "DbTraceDataBase.h"
#include "TextTraceDatabase.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::FullDb {
struct ParseUnitParams {
    std::string dbId;
};

// 非模板的基类接口，用于注册和管理
class IAbstractParseUnit {
  public:
    virtual ~IAbstractParseUnit() = default;
    virtual bool Handle(const ParseUnitParams &params) = 0;
    virtual std::string GetUnitName() const = 0;
};

template <typename DatabaseType = Timeline::VirtualTraceDatabase> class AbstractParseUnit : public IAbstractParseUnit {
  public:
    AbstractParseUnit() = default;
    virtual ~AbstractParseUnit() = default;
    bool Handle(const ParseUnitParams &params) override;

  protected:
    // 获取解析单元名称
    std::string GetUnitName() const override = 0;
    // 前置校验
    virtual bool PreCheck(
        const ParseUnitParams &params, const std::shared_ptr<DatabaseType> &database, std::string &error) = 0;
    // 处理解析流程
    virtual bool HandleParseProcess(
        const ParseUnitParams &params, const std::shared_ptr<DatabaseType> &database, std::string &error) = 0;
    // 发送通知:默认实现 有特殊要求 请在子类进行覆盖
    virtual void SendNotify(const ParseUnitParams &params, bool parseRes, const std::string &error);

  private:
    bool Parse(const ParseUnitParams &params, std::string &error);
};
extern template class AbstractParseUnit<Timeline::VirtualTraceDatabase>;
extern template class AbstractParseUnit<DbTraceDataBase>;
extern template class AbstractParseUnit<TextTraceDatabase>;
}
#endif // PROFILER_SERVER_ABSTRACTPARSEUNIT_H
