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

#ifndef PROFILER_SERVER_FILE_SELECTOR_H
#define PROFILER_SERVER_FILE_SELECTOR_H

#include <string>
#include <vector>
#include <memory>
#include "GlobalProtocolResponse.h"

namespace Dic {
namespace Module {
namespace Global {
class FileSelector {
  public:
    FileSelector() = default;
    ~FileSelector() = default;

    static void GetFoldersAndFiles(const std::string &path,
        std::vector<std::unique_ptr<Protocol::Folder>> &childrenFolders,
        std::vector<std::unique_ptr<Protocol::File>> &childrenFiles, bool &exist);

  private:
    static bool CompareStrIgnoreCase(const std::string &s1, const std::string &s2);
};
} // end of namespace Global
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_FILE_SELECTOR_H
