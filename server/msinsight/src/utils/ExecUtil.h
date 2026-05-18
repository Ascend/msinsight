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

#ifndef DATA_INSIGHT_CORE_EXECUTIL_H
#define DATA_INSIGHT_CORE_EXECUTIL_H

#include <cstdio>
#include <string>

namespace Dic {
constexpr int CMD_RESULT_BUF_SIZE = 1024;
class ExecUtil {
  public:
    // 该方法调用前需校验command参数，避免命令注入
    static inline std::string Exec(const std::string &command) {
        std::string result;
        char buffer[CMD_RESULT_BUF_SIZE] = {0};
        FILE *ptr;

        if ((ptr = popen(command.c_str(), "r")) != nullptr) {
            while (fgets(buffer, sizeof(buffer), ptr) != nullptr) {
                result.append(buffer);
            }
            pclose(ptr);
        }
        return result;
    }

    static inline std::string SelectFolder() {
        std::string command;
#ifdef _WIN32
        command = "PowerShell -Command "
                  "\"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;"
                  "Add-Type -AssemblyName System.Windows.Forms;"
                  "$dialog = New-Object System.Windows.Forms.FolderBrowserDialog;"
                  "$result = $dialog.ShowDialog();"
                  " if ($result -eq 'OK')"
                  " { $dialog.SelectedPath }"
                  "\"";
#else
        command = "zenity --file-selection --directory";
#endif
        std::string folder = Exec(command);
        folder.erase(folder.find_last_not_of('\n') + 1);
        return folder;
    }
};
} // end of namespace Dic
#endif // DATA_INSIGHT_CORE_EXECUTIL_H
