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

#ifndef PROFILER_SERVER_IECURVEHANDLER_H
#define PROFILER_SERVER_IECURVEHANDLER_H
#include "CurveRepo.h"
#include "IERequestHandler.h"
#include "IEProtocolResquest.h"
#include "IEProtocolResponse.h"

namespace Dic::Module::IE {
class IECurveHandler : public IERequestHandler {
  public:
    IECurveHandler() { moduleName = MODULE_IE; }
    ~IECurveHandler() override = default;
    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) override;

  protected:
    std::shared_ptr<CurveRepo> repo = std::make_shared<CurveRepo>();

    void QueryDatasByCols(
        const IEUsageViewParamsRequest &request, IEUsageViewResponse &response, std::vector<ColumnAtt> &atts);

  private:
    // 采样桶数量常量
    static const uint16_t DEFAULT_SAMPLE_BUCKETS = 1000;

    // 采样方法：参照 CurveContainer::ComputeCurve 的算法思想
    void SampleLines(const std::vector<std::vector<std::string>> &lines, std::vector<std::vector<std::string>> &result);

    // 辅助方法：将字符串转换为 double
    static double StringToDouble(const std::string &str);
};
}
#endif // PROFILER_SERVER_IECURVEHANDLER_H
