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

#ifndef PROFILER_SERVER_EXPORTAFFINITYAPIADVICE_H
#define PROFILER_SERVER_EXPORTAFFINITYAPIADVICE_H

#include "AdvisorRequestHandler.h"
#include "AdvisorProtocolResponse.h"
#include <fstream>

namespace Dic::Module::Advisor {

class ExportAffinityAPIAdvice : public AdvisorRequestHandler {
  public:
    ExportAffinityAPIAdvice() { command = Protocol::REQ_RES_ADVISOR_EXPORT_AFFINITY_API; }
    ~ExportAffinityAPIAdvice() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    // 按CSV规范转义单个字段，供导出使用并便于单元测试覆盖分隔符、引号和换行场景。
    static std::string EscapeCsvField(const std::string &value);
    // 清洗文件名中的非法字符，避免文本工程以文件路径作为rankId时导出失败。
    static std::string SanitizeFileName(const std::string &value);

  private:
    friend class ExportAffinityAPIIntegrationTest;
    static std::string CheckColumn(const std::string &column);
    static std::string GenerateCsvRow(const Protocol::AffinityAPIData &item);
    bool CreateCsvFile(
        const std::string &rankId, const std::string &projectName, Protocol::ExportAffinityAPIResBody &resBody);
    bool OpenUniqueCsvFile(const std::string &fileStem, Protocol::ExportAffinityAPIResBody &resBody);
    bool AppendFileContent(const std::string &str);
    bool DestroyFile();

    std::ofstream ofs;
    uint64_t currentFileSize = 0;
    const uint64_t maxFileSize = 1024ULL * 1024 * 1024; // 1GB
};

} // Dic::Module::Advisor

#endif // PROFILER_SERVER_EXPORTAFFINITYAPIADVICE_H
