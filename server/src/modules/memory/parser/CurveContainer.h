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
#ifndef PROFILER_SERVER_CURVECONTAINER_H
#define PROFILER_SERVER_CURVECONTAINER_H
#include <vector>
#include <string>
#include <climits>
#include <cmath>
#include <mutex>
namespace Dic::Module::Memory {
struct CurveView {
    std::vector<std::string> legends;
    std::vector<std::vector<std::string>> dataLines;
    std::vector<double> tempData;
    std::string title;
};
class CurveContainer {
  public:
    CurveView ComputeCurve(double xMin, double xMax, const std::string &input);
    void PutCurve(const std::string &inputKey, CurveView &curve);
    bool Exist(const std::string &inputKey);
    void Clear();

  private:
    std::mutex mutex;
    std::vector<double> flatData;
    std::vector<std::string> legends;
    std::string title;
    std::string key;

    static inline std::string DoubleToString(double value) {
        if (value > INT_MAX || value < 0) {
            return {};
        }
        auto int_part = static_cast<long long>(value);
        int frac_part = static_cast<int>(std::round((value - int_part) * 1000));
        if (frac_part == 1000) {
            int_part += 1;
            frac_part = 0;
        }
        long long tmp = int_part;
        int int_len = (tmp == 0) ? 1 : 0;
        while (tmp > 0) {
            tmp /= 10;
            int_len++;
        }
        int frac_len = 0;
        if (frac_part != 0) {
            frac_len = 3;
            int t = frac_part;
            if (t % 10 == 0) {
                frac_len--;
            }
            if (frac_len > 1 && (t / 10) % 10 == 0) {
                frac_len--;
            }
        }
        std::string result;
        result.resize(int_len + (frac_len > 0 ? 1 + frac_len : 0));
        long long n = int_part;
        for (int i = int_len - 1; i >= 0; --i) {
            result[i] = '0' + n % 10;
            n /= 10;
        }
        if (frac_len > 0) {
            result[int_len] = '.';
            int d1 = frac_part / 100;
            int d2 = (frac_part / 10) % 10;
            int d3 = frac_part % 10;
            result[int_len + 1] = '0' + d1;
            if (frac_len >= 2) {
                result[int_len + 2] = '0' + d2;
            }
            if (frac_len == 3) {
                result[int_len + 3] = '0' + d3;
            }
        }
        return result;
    }

    std::vector<int> ComputeDataIndex(const std::vector<size_t> &indices, size_t m, uint64_t bucketSize, int b);

    void AddCompeteData(CurveView &res, std::vector<int> &indexRes);

    void GetAllPoint(CurveView &res, std::vector<size_t> &indices);
};
} // namespace Dic::Module::Memory
#endif // PROFILER_SERVER_CURVECONTAINER_H
