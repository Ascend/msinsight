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
#include <fcntl.h>
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
int PythonUtil::ExecuteScript(const std::string &scriptPath, const std::vector<std::string> &arguments,
    const std::optional<std::string> &logPath) {
    std::string pythonCommand = GetPythonCommand();
    std::vector<std::string> commandArguments;
#if defined(_WIN32) || defined(__APPLE__)
#if !defined(INSIGHT_DEBUG)
    // Windows系统和macOS系统上发布模式加上-I选项和用户环境隔离
    commandArguments.emplace_back("-I");
#endif
#endif
    // scriptPath是相对于profiler_server所在目录的相对路径，需拼接FileUtil::GetCurrPath()获得的profiler_server所在目录
    std::string curPath = FileUtil::GetCurrPath();
    commandArguments.emplace_back(FileUtil::SplicePath(curPath, scriptPath));
    commandArguments.insert(commandArguments.end(), arguments.begin(), arguments.end());
    return ExecuteCommand(pythonCommand, commandArguments, logPath);
}

std::string PythonUtil::GetPythonCommand() {
#if defined(__linux__)
    // Linux系统上Python解释器由用户自主安装，调用方式为直接使用"python3"命令，Linux不用加上-I选项
    return "python3";
#endif
#if defined(_WIN32) || defined(__APPLE__)
#if defined(INSIGHT_DEBUG)
    // Windows系统和macOS系统上调试模式使用系统已安装的Python解释器
#if defined(_WIN32)
    return "python";
#endif
#if defined(__APPLE__)
    return "python3";
#endif
#endif
    // Windows系统和macOS系统上发布模式Python解释器由Insight提供，调用方式为使用绝对路径调用，避免和用户已安装的Python解释器混淆
    std::string curPath = FileUtil::GetCurrPath();
#if defined(_WIN32)
    return FileUtil::SplicePath(curPath, "python", "python.exe");
#endif
#if defined(__APPLE__)
    return FileUtil::SplicePath(curPath, "..", "..", "..", "..", "Resources", "python", "bin", "python3");
#endif
#endif
}

int PythonUtil::ExecuteCommand(const std::string &executablePath, const std::vector<std::string> &arguments,
    const std::optional<std::string> &logPath) {
#if defined(_WIN32)
    /*
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
    */

    DWORD flags = CREATE_NO_WINDOW;

    STARTUPINFOW startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    HANDLE handleOfLogFile = INVALID_HANDLE_VALUE;
    BOOL inheritHandles = FALSE;
    if (logPath.has_value()) {
        SECURITY_ATTRIBUTES securityAttributes;
        securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        securityAttributes.lpSecurityDescriptor = NULL;
        securityAttributes.bInheritHandle = TRUE;

        std::wstring wlogPath = StringUtil::String2WString(logPath.value());
        handleOfLogFile = CreateFileW(wlogPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_WRITE | FILE_SHARE_READ,
            &securityAttributes, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handleOfLogFile == INVALID_HANDLE_VALUE) {
            Server::ServerLog::Error("Failed to open log file in Windows. Error code: ", GetLastError());
            return -1;
        }
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdInput = NULL;
        startupInfo.hStdOutput = handleOfLogFile;
        startupInfo.hStdError = handleOfLogFile;
        inheritHandles = TRUE;
    }

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));

    // CreateProcessW的第二个参数是需要执行的命令，命令开头是可执行文件，因为可执行文件的路径可能包含空格，需要用双引号包裹
    // 可执行文件也会在环境变量PATH的路径搜索，所以调试模式只要系统安装了python并通过环境变量PATH指定就可以搜索到
    std::string commandLine = "\"" + executablePath + "\"";
    for (const auto &argument : arguments) {
        commandLine.append(" \"" + argument + "\"");
    }
    std::wstring wcommandLine = StringUtil::String2WString(commandLine);

    int result = CreateProcessW(
        NULL, wcommandLine.data(), NULL, NULL, inheritHandles, flags, NULL, NULL, &startupInfo, &processInfo);

    // CreateProcessW返回值为非零值时表示成功，零值表示失败，与posix_spawnp()正好相反
    if (result == 0) {
        Server::ServerLog::Error("Failed to create a process in Windows. Error code: ", GetLastError());
        if (handleOfLogFile != INVALID_HANDLE_VALUE) {
            CloseHandle(handleOfLogFile);
        }
        return -1;
    }
    Server::ServerLog::Info("Successfully created a process in Windows.");
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    if (handleOfLogFile != INVALID_HANDLE_VALUE) {
        CloseHandle(handleOfLogFile);
    }
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return 0;

    // _P_WAIT表示同步等待新创建的进程完成，返回新进程的返回值
    // return static_cast<int>(_wspawnvp(_P_WAIT, wexecutablePath.c_str(), wargv));
#endif
#if defined(__linux__) || defined(__APPLE__)
    // 将执行Python脚本的标准输出和标准错误重定向到日志文件
    posix_spawn_file_actions_t action;
    posix_spawn_file_actions_t *actionPtr = NULL;
    if (logPath.has_value()) {
        int actionResult = posix_spawn_file_actions_init(&action);
        if (actionResult != 0) {
            Server::ServerLog::Error("Failed to init posix spawn file actions. strerror: ", strerror(actionResult));
            return -1;
        }
        actionPtr = &action;
        actionResult =
            posix_spawn_file_actions_addopen(actionPtr, STDOUT_FILENO, logPath.value().c_str(), O_WRONLY | O_APPEND, 0);
        if (actionResult != 0) {
            posix_spawn_file_actions_destroy(actionPtr);
            Server::ServerLog::Error("Failed to add stdout redirect action. strerror: ", strerror(actionResult));
            return -1;
        }
        actionResult = posix_spawn_file_actions_adddup2(actionPtr, STDOUT_FILENO, STDERR_FILENO);
        if (actionResult != 0) {
            posix_spawn_file_actions_destroy(actionPtr);
            Server::ServerLog::Error("Failed to add stderr redirect action. strerror: ", strerror(actionResult));
            return -1;
        }
    }

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
    int result = posix_spawnp(&pid, executablePath.c_str(), actionPtr, NULL, ptrs.data(), environ);
#endif
#if defined(__APPLE__)
    int result = posix_spawnp(&pid, executablePath.c_str(), actionPtr, NULL, ptrs.data(), *(_NSGetEnviron()));
#endif
    if (actionPtr != NULL) {
        posix_spawn_file_actions_destroy(actionPtr);
    }
    if (result != 0) {
        Server::ServerLog::Error("Failed to spawn a process in Linux or macOS. strerror: ", strerror(result));
        return -1;
    }
    Server::ServerLog::Info("Successfully spawned a process in Linux or macOS.");
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}
}
