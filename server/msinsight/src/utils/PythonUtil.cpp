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

#if defined(_WIN32)
#include <process.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#if defined(__linux__)
#include <unistd.h>
extern "C" {
extern char **environ;
}
#endif
#if defined(__APPLE__)
#include <crt_externs.h>
extern "C" {
extern char ***_NSGetEnviron();
}
#endif
#endif
#include "StringUtil.h"
#include "FileUtil.h"
#include "ServerLog.h"
#include "PythonUtil.h"

namespace Dic {
int PythonUtil::ExecuteScript(const std::string &scriptPath, const std::vector<std::string> &arguments) {
    std::string pythonCommand = GetPythonCommand();
    std::vector<std::string> commandArguments;
#if defined(_WIN32) || defined(__APPLE__)
    // Windows和macOS加上-I选项和用户环境隔离
    commandArguments.emplace_back("-I");
#endif
    // scriptPath是相对于profiler_server所在目录的相对路径，需拼接FileUtil::GetCurrPath()获得的profiler_server所在目录
    std::string curPath = FileUtil::GetCurrPath();
    commandArguments.emplace_back(FileUtil::SplicePath(curPath, scriptPath));
    commandArguments.insert(commandArguments.end(), arguments.begin(), arguments.end());
    return ExecuteCommand(pythonCommand, commandArguments);
}

std::string PythonUtil::GetPythonCommand() {
#if defined(__linux__)
    // Linux系统上Python解释器由用户自主安装，调用方式为直接使用"python3"命令，Linux不用加上-I选项
    return "python3";
#endif
#if defined(_WIN32) || defined(__APPLE__)
    // Windows系统和macOS系统上Python解释器由Insight提供，调用方式为使用绝对路径调用，避免和用户已安装的Python解释器混淆
    std::string curPath = FileUtil::GetCurrPath();
#if defined(_WIN32)
    return FileUtil::SplicePath(curPath, "python", "python.exe");
#endif
#if defined(__APPLE__)
    return FileUtil::SplicePath(curPath, "..", "..", "..", "..", "Resources", "python", "bin", "python3");
#endif
#endif
}

int PythonUtil::ExecuteCommand(const std::string &executablePath, const std::vector<std::string> &arguments) {
#if defined(_WIN32)
    std::wstring wexecutablePath = StringUtil::String2WString(executablePath);
    std::vector<const wchar_t *> wptrs;
    // _wspawnvp参数需要转换为wstring
    // _wspawnvp的参数列表，第一个参数是可执行文件名，最后一个参数必须是NULL
    wptrs.reserve(arguments.size() + 2);
    std::wstring wcommand = StringUtil::String2WString("\"python\"");
    wptrs.push_back(wcommand.c_str());
    // _wspawnvp传入的cmdname参数可以包含空格，但是argument参数如果包含空格，会被识别成两项参数，需手动添加双引号包裹

    std::vector<std::wstring> warguments;
    for (const auto &argument : arguments) {
        warguments.push_back(StringUtil::String2WString("\"" + argument + "\""));
    }
    // 注意生命周期，需要确保取.c_str()方法的字符串在这个函数内始终存在，所以必须等warguments不再变化了才能取.c_str()，而不能在上个循环里取
    for (const auto &wargument : warguments) {
        wptrs.push_back(wargument.c_str());
    }
    wptrs.push_back(NULL);
    // 如果executablePath或arguments被销毁或重新分配，argv会成为悬空指针
    const wchar_t *const *wargv = wptrs.data();
    // _P_WAIT表示同步等待新创建的进程完成，返回新进程的返回值
    return static_cast<int>(_wspawnvp(_P_WAIT, wexecutablePath.c_str(), wargv));
#endif
#if defined(__linux__) || defined(__APPLE__)
    std::vector<char *> ptrs;
    ptrs.reserve(arguments.size() + 2);
    ptrs.push_back(const_cast<char *>(executablePath.c_str()));
    for (const auto &argument : arguments) {
        ptrs.push_back(const_cast<char *>(argument.c_str()));
    }
    ptrs.push_back(NULL);
    pid_t pid;
#if defined(__linux__)
    // Linux执行python3命令需要从PATH环境变量查找，所以统一使用posix_spawnp
    int result = posix_spawnp(&pid, executablePath.c_str(), NULL, NULL, ptrs.data(), environ);
#endif
#if defined(__APPLE__)
    int result = posix_spawnp(&pid, executablePath.c_str(), NULL, NULL, ptrs.data(), *(_NSGetEnviron()));
#endif
    if (result != 0) {
        Server::ServerLog::Error("Failed to spawn a process in Linux or macOS. strerror: ", strerror(result));
        return -1;
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}
}
