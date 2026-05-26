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
#include "NumberUtil.h"
#include "CurveContainer.h"
namespace Dic::Module::Memory {
CurveView CurveContainer::ComputeCurve(double xMin, double xMax, const std::string &input) {
    std::unique_lock<std::mutex> lock(mutex);
    CurveView res;
    if (key != input || legends.empty()) {
        return res;
    }
    if (!legends.empty() && flatData.size() >= legends.size() && NumberUtil::IsDoubleEqual(xMin, xMax) &&
        NumberUtil::IsDoubleEqual(xMin, 0)) {
        xMin = flatData[0];
        xMax = flatData[flatData.size() - legends.size()];
    }
    res.title = title;
    res.legends = legends;
    const uint16_t numBuckets = 1000;
    size_t n = flatData.size() / legends.size(); // 行数
    if (n == 0) {
        return res;
    }
    // 过滤范围，构造一个索引数组，避免复制
    std::vector<size_t> indices;
    indices.reserve(n);
    for (size_t i = 0; i < n; i++) {
        double x = flatData[i * legends.size()];
        if (x >= xMin && x <= xMax) {
            indices.push_back(i);
        }
    }
    size_t m = indices.size();
    if (m == 0) {
        return res;
    }
    if (m <= numBuckets) {
        GetAllPoint(res, indices);
        return res;
    }
    uint64_t bucketSize = static_cast<uint64_t>(ceil(static_cast<double>(m) / numBuckets));
    for (int b = 0; b < numBuckets; b++) {
        std::vector<int> indexRes = ComputeDataIndex(indices, m, bucketSize, b);
        AddCompeteData(res, indexRes);
    }
    return res;
}

void CurveContainer::PutCurve(const std::string &inputKey, CurveView &curve) {
    std::unique_lock<std::mutex> lock(mutex);
    key = inputKey;
    title = curve.title;
    legends = curve.legends;
    flatData = curve.tempData;
}

bool CurveContainer::Exist(const std::string &inputKey) {
    std::unique_lock<std::mutex> lock(mutex);
    return inputKey == key;
}

void CurveContainer::Clear() {
    std::unique_lock<std::mutex> lock(mutex);
    key.clear();
    title.clear();
    legends.clear();
    flatData.clear();
}

std::vector<int> CurveContainer::ComputeDataIndex(
    const std::vector<size_t> &indices, size_t m, uint64_t bucketSize, int b) {
    uint64_t start = static_cast<size_t>(b) * bucketSize;
    uint64_t tempM = m;
    uint64_t end = std::min(tempM, start + bucketSize);
    std::vector<int> indexRes;
    for (size_t col = 1; col <= legends.size() - 1; col++) {
        double minVal = std::numeric_limits<double>::infinity();
        double maxVal = -std::numeric_limits<double>::infinity();
        int minIdx = -1;
        int maxIdx = -1;
        for (size_t k = start; k < end; k++) {
            size_t i = indices[k];
            double val = flatData[i * legends.size() + col];
            if (std::isnan(val)) {
                continue;
            }
            if (val < minVal) {
                minVal = val;
                minIdx = static_cast<int>(i);
            }
            if (val > maxVal) {
                maxVal = val;
                maxIdx = static_cast<int>(i);
            }
        }
        if (minIdx > maxIdx) {
            std::swap(minIdx, maxIdx);
        }
        if (minIdx >= 0 && std::find(indexRes.begin(), indexRes.end(), minIdx) == indexRes.end()) {
            indexRes.emplace_back(minIdx);
        }
        if (maxIdx >= 0 && std::find(indexRes.begin(), indexRes.end(), maxIdx) == indexRes.end()) {
            indexRes.emplace_back(maxIdx);
        }
    }
    return indexRes;
}

void CurveContainer::AddCompeteData(CurveView &res, std::vector<int> &indexRes) {
    for (const auto &item : indexRes) {
        res.dataLines.emplace_back();
        res.dataLines.reserve(legends.size());
        uint64_t startIndex = legends.size() * item;
        for (size_t i = 0; i < legends.size(); ++i) {
            if (std::isnan(flatData[startIndex + i])) {
                res.dataLines.back().emplace_back("NULL");
                continue;
            }
            res.dataLines.back().emplace_back(DoubleToString(flatData[startIndex + i]));
        }
    }
}

void CurveContainer::GetAllPoint(CurveView &res, std::vector<size_t> &indices) { // 直接返回范围内的数据
    for (size_t idx : indices) {
        res.dataLines.emplace_back();
        res.dataLines.reserve(legends.size());
        uint64_t start = legends.size() * idx;
        for (size_t i = 0; i < legends.size(); ++i) {
            if (std::isnan(flatData[start + i])) {
                res.dataLines.back().emplace_back("NULL");
                continue;
            }
            res.dataLines.back().emplace_back(DoubleToString(flatData[start + i]));
        }
    }
}
} // namespace Dic::Module::Memory
