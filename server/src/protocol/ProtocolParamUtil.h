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

#ifndef PROFILER_SERVER_PROTOCOLPARAMUTIL_H
#define PROFILER_SERVER_PROTOCOLPARAMUTIL_H

#include <string>
#include "ProtocolMessage.h"

namespace Dic::Protocol {
struct OrderParam {
    std::string orderBy;
    std::string orderType;
    std::string NormalizeOrderType(const std::string &type) const {
        if (type == "ascend" || type == "Ascend") {
            return "ASC";
        } else {
            return "DESC";
        }
    }

    std::string GetNormalizeOrderType() const { return NormalizeOrderType(orderType); }

    bool CheckOrderByInjection() const { return false; }

    std::string GenerateSql() const {
        std::string sql = " ";
        if (!orderBy.empty() && !orderType.empty()) {
            sql = " ORDER BY " + orderBy + " " + GetNormalizeOrderType() + " ";
        }
        return sql;
    }
};

struct SearchParam {
    std::string condition;
    std::string type;
};

struct PageParam {
    uint32_t current{};
    uint32_t pageSize{};
    uint64_t total{};
};

struct ThreadDetailParams {
    std::string rankId;
    std::string pid;
    std::string tid;
    std::string id;
    std::string metaType;
    uint64_t startTime = 0;
    uint32_t depth = 0;
    bool isPythonStack = false;
    bool CheckParams(std::string &errMsg) const {
        if (!StringUtil::CheckSqlValid(id)) {
            errMsg = "Id Invalid!";
            return false;
        }
        return true;
    }
};

struct SetCardAliasParams {
    std::string rankId;
    std::string cardAlias;
    bool CheckParams(std::string alias, std::string &errMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(alias, paramError)) {
            errMsg = "[Timeline]Failed to check card alias, " + paramError;
            return false;
        }
        return true;
    }
};
}
#endif // PROFILER_SERVER_PROTOCOLPARAMUTIL_H
