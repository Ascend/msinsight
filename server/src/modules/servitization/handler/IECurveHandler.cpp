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
#include <algorithm>
#include <cmath>
#include <limits>
#include "IEProtocolResquest.h"
#include "IEProtocolResponse.h"
#include "ServerLog.h"
#include "WsSessionManager.h"
#include "IECurveHandler.h"
namespace Dic::Module::IE {
using namespace Dic;
using namespace Dic::Server;
bool IECurveHandler::HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<IEUsageViewParamsRequest &>(*requestPtr);
    std::unique_ptr<IEUsageViewResponse> responsePtr = std::make_unique<IEUsageViewResponse>();
    IEUsageViewResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (!request.params.type.empty()) {
        auto translate = repo->QueryTableNameDesc(request.params.rankId, request.params.type, request.params.isZh);
        response.data.desc = translate;
        auto atts = repo->QueryTableInfoByName(request.params.rankId, request.params.type);
        if (atts.size() <= 1) {
            SendResponse(std::move(responsePtr), true);
            return true;
        }
        QueryDatasByCols(request, response, atts);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

void IECurveHandler::QueryDatasByCols(
    const IEUsageViewParamsRequest &request, IEUsageViewResponse &response, std::vector<ColumnAtt> &atts) {
    std::string rankId = request.params.rankId;
    std::string tableName = request.params.type;
    auto data = repo->QueryDataByColumn(rankId, tableName, atts);

    // 第一步：收集所有去重后的行
    std::vector<std::vector<std::string>> uniqueLines;
    std::vector<std::string> curline(atts.size());
    for (auto &item : data) {
        std::vector<std::string> line;
        for (const auto &att : atts) {
            line.emplace_back(item[att.key]);
        }
        bool isDifferent = false;
        for (size_t i = 0; i < atts.size(); ++i) {
            if (curline[i] != line[i]) {
                isDifferent = true;
                break;
            }
        }
        if (isDifferent) {
            uniqueLines.emplace_back(line);
            curline = line;
        }
    }

    // 第二步：如果数据量太大，进行采样
    if (uniqueLines.size() > DEFAULT_SAMPLE_BUCKETS) {
        SampleLines(uniqueLines, response.data.lines);
    } else {
        response.data.lines = std::move(uniqueLines);
    }

    for (const auto &att : atts) {
        response.data.legends.emplace_back(att.key);
    }
}

void IECurveHandler::SampleLines(
    const std::vector<std::vector<std::string>> &lines, std::vector<std::vector<std::string>> &result) {
    uint64_t n = lines.size();
    size_t numCols = lines.empty() ? 0 : lines[0].size();

    // 计算每个桶的大小
    auto bucketSize = static_cast<uint64_t>(std::ceil(static_cast<double>(n) / DEFAULT_SAMPLE_BUCKETS));
    std::vector<int> sampledIndices;

    // 对每个桶进行采样
    for (int b = 0; b < DEFAULT_SAMPLE_BUCKETS; ++b) {
        uint64_t start = static_cast<uint64_t>(b) * bucketSize;
        uint64_t end = std::min(n, start + bucketSize);

        if (start >= n) {
            break;
        }

        // 对每一列（除了第一列），找出最小值和最大值的索引
        for (size_t col = 1; col < numCols; ++col) {
            double minVal = std::numeric_limits<double>::infinity();
            double maxVal = -std::numeric_limits<double>::infinity();
            int minIdx = -1;
            int maxIdx = -1;

            for (uint64_t k = start; k < end; ++k) {
                double val = StringToDouble(lines[k][col]);
                if (std::isnan(val)) {
                    continue;
                }
                if (val < minVal) {
                    minVal = val;
                    minIdx = static_cast<int>(k);
                }
                if (val > maxVal) {
                    maxVal = val;
                    maxIdx = static_cast<int>(k);
                }
            }

            // 添加最小值和最大值的索引（确保 minIdx < maxIdx 且不重复）
            if (minIdx > maxIdx && minIdx >= 0 && maxIdx >= 0) {
                std::swap(minIdx, maxIdx);
            }
            if (minIdx >= 0 &&
                std::find(sampledIndices.begin(), sampledIndices.end(), minIdx) == sampledIndices.end()) {
                sampledIndices.emplace_back(minIdx);
            }
            if (maxIdx >= 0 &&
                std::find(sampledIndices.begin(), sampledIndices.end(), maxIdx) == sampledIndices.end()) {
                sampledIndices.emplace_back(maxIdx);
            }
        }
    }

    // 按索引排序并收集结果
    std::sort(sampledIndices.begin(), sampledIndices.end());
    for (int idx : sampledIndices) {
        result.emplace_back(lines[idx]);
    }
}

double IECurveHandler::StringToDouble(const std::string &str) {
    if (str.empty() || str == "NULL") {
        return 0.0;
    }
    try {
        return std::stod(str);
    } catch (...) {
        return 0.0;
    }
}
} // namespace Dic::Module::IE
