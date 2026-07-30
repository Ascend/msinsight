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

  private:
    static std::string CheckColumn(const std::string &column);
    static std::string GenerateCsv(const std::vector<Protocol::AffinityAPIData> &data);
    bool CreateCsvFile(
        const std::string &rankId, const std::string &projectName, Protocol::ExportAffinityAPIResBody &resBody);
    bool AppendFileContent(const std::string &str);
    void DestroyFile();

    std::ofstream ofs;
    uint64_t currentFileSize = 0;
    const uint64_t maxFileSize = 1024ULL * 1024 * 1024; // 1GB
};

} // Dic::Module::Advisor

#endif // PROFILER_SERVER_EXPORTAFFINITYAPIADVICE_H
