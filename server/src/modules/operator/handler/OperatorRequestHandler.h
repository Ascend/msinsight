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

#ifndef PROFILER_SERVER_OPERATORREQUESTHANDLER_H
#define PROFILER_SERVER_OPERATORREQUESTHANDLER_H

#include "ModuleRequestHandler.h"
#include "ProtocolDefs.h"
#include "OperatorErrorManager.h"
#include "OperatorGroupConverter.h"
#include "ServerLog.h"
#include "StringUtil.h"

namespace Dic::Module::Operator {
class OperatorRequestHandler : public ModuleRequestHandler {
  public:
    OperatorRequestHandler() {
        moduleName = MODULE_OPERATOR;
        async = false;
    }

    ~OperatorRequestHandler() override = default;

    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) override { return true; }

  protected:
    static std::string GetLogContext(const std::string &rankId, const std::string &group) {
        return StringUtil::StrJoin(
            "rankId=", rankId, ", group=", Protocol::OperatorGroupConverter::GetGroupForLog(group));
    }
    static std::string GetLogContext(const std::string &rankId, const std::string &group, bool isCompare) {
        return StringUtil::StrJoin(GetLogContext(rankId, group), ", isCompare=", isCompare ? "true" : "false");
    }
    static std::string GetBaselineLogContext(
        const std::string &rankId, const std::string &baselineName, const std::string &group) {
        return StringUtil::StrJoin("rankId=", rankId, ", baselineName=", baselineName,
            ", group=", Protocol::OperatorGroupConverter::GetGroupForLog(group));
    }
    static std::string BuildFailureLog(const std::string &operation, const std::string &stage,
        const std::string &description, const std::string &context, const std::string &cause,
        const std::string &suggestion) {
        return StringUtil::StrJoin("[Operator][", operation, "][", stage, "] ", description, ". ",
            context.empty() ? "" : context + ", ", "cause=", cause, ", suggestion=", suggestion, ".");
    }
    static void LogInvalidRequest(const std::string &operation, const std::string &cause,
        const std::string &suggestion = "Check the request parameters and retry") {
        Server::ServerLog::Warn(
            "%", BuildFailureLog(operation, "ValidateRequest", "Invalid request parameters", "", cause, suggestion));
    }
    static void LogDatabaseUnavailable(const std::string &operation, const std::string &context, bool error = false) {
        auto log = BuildFailureLog(operation, "GetDatabase", "Operator database is unavailable", context,
            "no summary database was found", "Check whether the profiling data was imported and parsed successfully");
        if (error) {
            Server::ServerLog::Error("%", log);
        } else {
            Server::ServerLog::Warn("%", log);
        }
    }
    static void LogDeviceUnavailable(const std::string &operation, const std::string &context) {
        Server::ServerLog::Error("%",
            BuildFailureLog(operation, "ResolveDevice", "Failed to resolve deviceId", context,
                "no device mapping was found", "Check whether the rank information was parsed successfully"));
    }
    static void LogBaselineDeviceUnavailable(const std::string &operation, const std::string &context) {
        Server::ServerLog::Error("%",
            BuildFailureLog(operation, "ResolveBaselineDevice", "Failed to resolve baseline deviceId", context,
                "no baseline device mapping was found",
                "Check whether the baseline rank information was parsed successfully"));
    }
    static void LogBaselineUnavailable(const std::string &operation, const std::string &context) {
        Server::ServerLog::Error("%",
            BuildFailureLog(operation, "ResolveBaseline", "Failed to resolve baseline", context,
                "baseline is not configured", "Configure a baseline and wait for parsing to finish"));
    }
    static void LogBaselineDatabaseUnavailable(const std::string &operation, const std::string &context) {
        Server::ServerLog::Warn("%",
            BuildFailureLog(operation, "GetBaselineDatabase", "Baseline database is unavailable", context,
                "no baseline summary database was found", "Re-import or reparse the baseline data"));
    }
};
}
#endif // PROFILER_SERVER_OPERATORREQUESTHANDLER_H
