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
#include "IEContext.h"
#include "ServitizationContext.h"
namespace Dic::Module::IE {
bool ServitizationContext::InitDataBase(const std::string &fileId, const std::string &dbPath) {
    return IEContext::Instance().InitDataBase(fileId, dbPath);
}

std::string ServitizationContext::ComputeFileIdByFolder(const std::string &folder) {
    return IEContext::Instance().ComputeFileIdByFolder(folder);
}

bool ServitizationContext::ExecuteScript(const std::string &fileId, const std::string &script) {
    return IEContext::Instance().ExecSql(fileId, script);
}

std::shared_ptr<Database> ServitizationContext::GetDatabase(const std::string &fileId) {
    return IEContext::Instance().GetDatabase(fileId);
}

void ServitizationContext::Reset() { IEContext::Instance().Reset(); }
} // namespace Dic::Module::IE
