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

#ifndef DATA_INSIGHT_CORE_VALIDATEUTIL_H
#define DATA_INSIGHT_CORE_VALIDATEUTIL_H

#include "ServerLog.h"
#include "SafeFile.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "FileDef.h"

#if defined(_WIN32)
#include <io.h>
#endif

namespace Dic {
class ValidateUtil {
  public:
    static bool CheckCsvFile(const std::string &fileName) {
        std::ifstream file = OpenReadFileSafely(fileName);
        if (!file.good()) {
            Server::ServerLog::Error("Check csv file cannot get file");
            return false;
        }
        std::string filePath = FileUtil::PathPreprocess(fileName);
        if (access(filePath.c_str(), R_OK) == -1) {
            Server::ServerLog::Error("Check csv file cannot read file");
            return false;
        }
        long long size = FileUtil::GetFileSize(filePath.c_str());
        if (size > MAX_FILE_SIZE_2G) {
            Server::ServerLog::Warn("The csv file is too large, and the max size is 2G");
        }
        return true;
    }

    static bool CheckCsvFileList(const std::vector<std::string> &fileNameList) {
        for (const auto &fileName : fileNameList) {
            if (!CheckCsvFile(fileName)) {
                return false;
            }
        }
        return true;
    }
};
} // end of namespace Dic
#endif // DATA_INSIGHT_CORE_VALIDATEUTIL_H
