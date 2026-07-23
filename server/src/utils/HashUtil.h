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
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_HASHUTIL_H
#define PROFILER_SERVER_HASHUTIL_H

#include <string>
#include <string_view>

namespace Dic::HashUtil {
std::string CalculateFileSha256(const std::string &filePath, std::string_view salt = "");
}

#endif // PROFILER_SERVER_HASHUTIL_H
