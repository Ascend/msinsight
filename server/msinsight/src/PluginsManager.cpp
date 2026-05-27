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
#include <cstdlib>
#include "ServerLog.h"
#include "./utils/FileUtil.h"
#include "PluginsManager.h"
namespace Dic::Core {
#ifdef _WIN32
#include <windows.h>
const static std::string EXT = ".dll";
#else
#ifdef __APPLE__
const static std::string EXT = ".dylib";
#else
const static std::string EXT = ".so";
#endif
#endif

void AddPathEnv(const std::string &newDir) {
#ifdef _WIN32
    char buffer[32767]; // PATH 的最大长度通常为 32767 字符
    DWORD length = GetEnvironmentVariable("PATH", buffer, sizeof(buffer));
    if (length == 0) {
        return;
    }
    std::string path;
    if (length != 0) {
        // 将 PATH 转换为 std::string
        path.assign(buffer, length);
        // 检查新目录是否已经存在于 PATH 中
        if (path.find(newDir) != std::string::npos) {
            return;
        }
    }

    // 追加新目录到 PATH
    if (!path.empty() && path.back() != ';') {
        path += ";"; // 添加分隔符
    }
    path += newDir;
    SetEnvironmentVariable("PATH", path.c_str());
#endif
}

PluginsManager &PluginsManager::Instance() {
    static PluginsManager instance;
    return instance;
}
bool PluginsManager::RegisterPlugin(std::unique_ptr<BasePlugin> plugin) {
    if (pluginsMap_.count(plugin->GetPluginName()) != 0) {
        return false;
    }
    pluginsMap_.emplace(plugin->GetPluginName(), std::move(plugin));
    return true;
}
void PluginsManager::LoadPlugins() {
    std::string tempPath = StringUtil::ToUtf8Str(FileUtil::SplicePath(FileUtil::GetCurrPath(), "plugins"));
    if (!FileUtil::CheckPathSecurity(StringUtil::ToLocalStr(tempPath))) {
        return;
    }
    auto pluginsDir = fs::u8path(tempPath);
    if (!fs::exists(pluginsDir)) {
        return;
    }
    for (auto &dir : fs::directory_iterator(pluginsDir)) {
        if (!is_directory(dir)) {
            continue;
        }
        std::string dirPath = StringUtil::ToLocalStr(dir.path().string());
        if (!FileUtil::CheckPathSecurity(dirPath)) {
            continue;
        }
        for (auto &file : fs::directory_iterator(dir)) {
            std::string filePath = StringUtil::ToLocalStr(file.path().string());
            if (is_directory(file) || file.path().extension().string() != EXT ||
                !FileUtil::CheckPathSecurity(filePath, CHECK_FILE_READ) ||
                !FileUtil::CheckWritableByOtherOrGroup(filePath)) {
                continue;
            }
            AddPathEnv(dirPath);
#ifdef _WIN32
            if (LoadLibraryA(filePath.c_str()) == nullptr) {
                std::cerr << "Load " << filePath << " failed, err=" << GetLastError() << std::endl;
            }
#else
            if (dlopen(file.path().string().c_str(), RTLD_LAZY) == nullptr) {
                std::cerr << "Load " << file.path().string() << " failed, err=" << dlerror() << std::endl;
            }
#endif
        }
    }
}
std::map<std::string, std::unique_ptr<BasePlugin>> &PluginsManager::GetAllPlugins() { return pluginsMap_; }
PluginRegister::PluginRegister(std::unique_ptr<BasePlugin> plugin) {
    PluginsManager::Instance().RegisterPlugin(std::move(plugin));
}
}
