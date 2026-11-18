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
#include "../base/ParamsParser.h"
#include <filesystem>
#include <iostream>

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
    return ResolveMacPythonCommand(curPath, Server::ParamsParser::Instance().GetOption().resourceRoot);
#endif
#endif
}

std::string PythonUtil::ResolveMacPythonCommand(const std::string &serverDirectory, const std::string &resourceRoot) {
    namespace fs = std::filesystem;
    const auto fail = [&serverDirectory, &resourceRoot](const std::string &reason) {
        // This resolver also runs before logging is configured, including inside a signed App.
        std::cerr << "Failed to locate bundled Python: " << reason << ". Resource root: "
                  << (resourceRoot.empty() ? "<auto>" : resourceRoot) << "; server directory: " << serverDirectory
                  << std::endl;
        return std::string{};
    };
    std::error_code error;
    fs::path root;
    if (!resourceRoot.empty()) {
        if (!fs::path(resourceRoot).is_absolute()) {
            return fail("resource root must be an absolute path");
        }
        root = fs::canonical(resourceRoot, error);
        if (error) {
            return fail("cannot resolve resource root: " + error.message());
        }
    } else {
        // Direct server launches use the same standard bundle contract as the desktop launcher.
        const auto server = fs::canonical(serverDirectory, error);
        if (error) {
            return fail("cannot resolve server directory: " + error.message());
        }
        if (!fs::is_directory(server, error)) {
            return fail("server path is not an accessible directory: " + server.string() +
                        (error ? " (" + error.message() + ")" : ""));
        }
        if (server.filename() != "server" || server.parent_path().filename() != "profiler") {
            return fail("expected server directory in Contents/Resources/profiler/server: " + server.string());
        }
        root = server.parent_path().parent_path();
        if (root.filename() != "Resources" || root.parent_path().filename() != "Contents") {
            return fail("expected resource root at Contents/Resources: " + root.string());
        }
    }
    if (!fs::is_directory(root, error)) {
        return fail("resource root is not an accessible directory: " + root.string() +
                    (error ? " (" + error.message() + ")" : ""));
    }
    const auto python = root / "python" / "bin" / "python3";
    if (!fs::is_regular_file(python, error)) {
        if (!Server::ParamsParser::Instance().GetOption().checkPython) {
            Server::ServerLog::Error(
                "Bundled Python is missing. Resource root: %, interpreter: %", root.string(), python.string());
        }
        return fail("bundled interpreter is missing or not an accessible regular file: " + python.string() +
                    (error ? " (" + error.message() + ")" : ""));
    }
    return python.string();
}

int PythonUtil::ExecuteCommand(const std::string &executablePath, const std::vector<std::string> &arguments,
    const std::optional<std::string> &logPath) {
    const bool checkPython = Server::ParamsParser::Instance().GetOption().checkPython;
    const auto logError = [checkPython](const auto &...args) {
        if (checkPython) {
            // ServerLog lazily creates a file; runtime probes must only write captured stderr.
            (std::cerr << ... << args);
            std::cerr << std::endl;
        } else {
            Server::ServerLog::Error(args...);
        }
    };
#if defined(_WIN32)
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
            logError("Failed to open log file in Windows. Error code: ", GetLastError());
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
        logError("Failed to create a process in Windows. Error code: ", GetLastError());
        if (handleOfLogFile != INVALID_HANDLE_VALUE) {
            CloseHandle(handleOfLogFile);
        }
        return -1;
    }
    if (!checkPython) {
        Server::ServerLog::Info("Successfully created a process in Windows.");
    }
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
            logError("Failed to init posix spawn file actions. strerror: ", strerror(actionResult));
            return -1;
        }
        actionPtr = &action;
        actionResult =
            posix_spawn_file_actions_addopen(actionPtr, STDOUT_FILENO, logPath.value().c_str(), O_WRONLY | O_APPEND, 0);
        if (actionResult != 0) {
            posix_spawn_file_actions_destroy(actionPtr);
            logError("Failed to add stdout redirect action. strerror: ", strerror(actionResult));
            return -1;
        }
        actionResult = posix_spawn_file_actions_adddup2(actionPtr, STDOUT_FILENO, STDERR_FILENO);
        if (actionResult != 0) {
            posix_spawn_file_actions_destroy(actionPtr);
            logError("Failed to add stderr redirect action. strerror: ", strerror(actionResult));
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
        if (checkPython) {
            logError("Failed to spawn executable ", executablePath, ". Error code: ", result,
                ", strerror: ", strerror(result));
        } else {
            Server::ServerLog::Error(
                "Failed to spawn executable %. errno: %, strerror: %", executablePath, result, strerror(result));
        }
        return -1;
    }
    if (!checkPython) {
        Server::ServerLog::Info("Successfully spawned a process in Linux or macOS.");
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}
}
