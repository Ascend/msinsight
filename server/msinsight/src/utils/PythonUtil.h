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

#ifndef PROFILER_SERVER_PYTHONUTIL_H
#define PROFILER_SERVER_PYTHONUTIL_H

#include <string>
#include <vector>

namespace Dic {
class PythonUtil {
  public:
    // 执行Python脚本，scriptPath是相对于profiler_server所在目录的相对路径，arguments是脚本的参数
    static int ExecuteScript(const std::string &scriptPath, const std::vector<std::string> &arguments);
    static std::string GetPythonCommand();
    static int ExecuteCommand(const std::string &executablePath, const std::vector<std::string> &arguments);
};
}

#endif // PROFILER_SERVER_PYTHONUTIL_H
