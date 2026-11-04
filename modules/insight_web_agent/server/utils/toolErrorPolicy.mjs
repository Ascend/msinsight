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
export const TOOL_ERROR_LIMITS = Object.freeze({
    text: 3600,
    message: 600,
    details: 2000,
    state: 256,
    stderr: 1600,
    stdout: 200,
    inputText: 32768,
    skills: 20,
    skillName: 64,
    causeDepth: 3,
});

export const TOOL_ERROR_SOURCES = Object.freeze([
    "command", "cli", "bash", "skill", "rag", "capability_http", "runtime", "sdk", "external_acp",
]);
export const TOOL_ERROR_STAGES = Object.freeze([
    "validate", "authorize", "dispatch", "execute", "retrieve", "decode", "respond", "stream",
]);
export const TOOL_ERROR_OUTCOMES = Object.freeze(["not_started", "failed", "completed", "unknown"]);
export const TOOL_ERROR_RETRY_ADVICE = Object.freeze([
    "correct_input", "retry_after_ready", "refresh_context", "inspect_before_retry", "do_not_retry",
]);

const commandFields = [
    "reason", "command", "moduleId", "targetModule", "activeModule", "backendCode", "timeoutMs",
    "eventId", "deviceId", "sliceIndex", "phase",
];
const commandState = ["activeModule", "targetModule", "moduleId", "ready", "reason"];
const processFields = ["reason", "exitCode", "signal", "stderr", "stdout", "timeoutMs", "limitBytes", "causeCode"];
const networkFields = ["reason", "status", "httpStatus", "contentType", "causeCode", "timeoutMs"];
const reasons = Object.freeze([
    "module_not_active", "module_not_registered", "command_not_found", "command_not_registered",
    "invalid_argument", "cancelled", "session_cancelled", "service_disposed", "deadline_exceeded",
    "user_cancelled", "connection_lost", "result_not_serializable", "unavailable", "context_changed",
]);
const ragReasons = Object.freeze([
    "embedding_unavailable", "index_unavailable", "knowledge_pack_unavailable",
    "worker_unavailable", "retrieval_failed",
]);

const policies = Object.create(null);
const register = (codes, fields, { retry = false, state = [], reasonValues = reasons, reasonFallback, message } = {}) => {
    for (const code of codes) {
        policies[code] = Object.freeze({
            fields: Object.freeze([...fields]),
            state: Object.freeze([...state]),
            retry,
            reasonValues: Object.freeze([...reasonValues]),
            reasonFallback,
            message,
        });
    }
};

register([
    "CAPABILITY_INVALID", "CAPABILITY_CONFLICT", "CAPABILITY_NOT_FOUND",
    "CAPABILITY_INVALID_ARGUMENT", "CAPABILITY_EXECUTION_FAILED",
    "CAPABILITY_DENIED_BY_USER", "CAPABILITY_SESSION_UNAVAILABLE",
    "TOOL_EXECUTION_FAILED", "TOOL_NOT_FOUND", "TOOL_CANCELLED",
], ["reason", "causeCode"]);
register(["CAPABILITY_CONCURRENT"], ["reason"], { retry: true });
register(["TOOL_OUTCOME_UNKNOWN"], ["reason", "causeCode", "httpStatus"]);
register(["CAPABILITY_AUTH_FAILED", "CAPABILITY_REQUEST_FAILED", "CAPABILITY_RESPONSE_INVALID"], networkFields, { retry: true });
register([
    "COMMAND_INVALID", "COMMAND_NOT_FOUND", "COMMAND_EXECUTION_FAILED",
    "COMMAND_PERMISSION_DENIED", "COMMAND_APPROVAL_REQUIRED", "COMMAND_CANCELLED", "COMMAND_RESULT_INVALID",
], commandFields, { state: commandState });
register(["COMMAND_UNAVAILABLE", "COMMAND_BUSY", "COMMAND_TIMEOUT", "COMMAND_CONNECTION_LOST"], commandFields, { retry: true, state: commandState });
// Table errors share one field/state shape; each code is registered exactly once — the
// retryable subset gets its own registration instead of overriding an earlier one.
const TABLE_FIELDS = ["expectedRevision", "currentRevision", "requestedPage", "maxPage", "total", "pageSize"];
const TABLE_STATE = ["query", "total", "rowCount"];
const TABLE_RETRYABLE = Object.freeze([
    "TABLE_TARGET_UNAVAILABLE", "TABLE_NOT_READY", "TABLE_BUSY", "TABLE_STATE_STALE",
    "TABLE_PAGE_OUT_OF_RANGE", "TABLE_COMMAND_SUPERSEDED", "TABLE_COMMAND_TIMEOUT", "TABLE_CONNECTION_LOST",
]);
const TABLE_ALL = Object.freeze([
    "TABLE_TARGET_UNAVAILABLE", "TABLE_NOT_READY", "TABLE_BUSY", "TABLE_STATE_STALE",
    "TABLE_CAPABILITY_UNSUPPORTED", "TABLE_COMMAND_INVALID", "TABLE_COLUMN_UNKNOWN",
    "TABLE_FILTER_UNSUPPORTED", "TABLE_FILTER_VALUE_INVALID", "TABLE_SORT_UNSUPPORTED",
    "TABLE_PAGE_OUT_OF_RANGE", "TABLE_ROW_UNKNOWN", "TABLE_SELECTION_LIMIT_EXCEEDED",
    "TABLE_EXPANSION_LIMIT_EXCEEDED", "TABLE_COMMAND_SUPERSEDED", "TABLE_COMMAND_TIMEOUT",
    "TABLE_TRANSITION_FAILED", "TABLE_COPY_FAILED", "TABLE_CONNECTION_LOST",
]);
register(TABLE_ALL.filter((code) => !TABLE_RETRYABLE.includes(code)), TABLE_FIELDS, { state: TABLE_STATE });
register([...TABLE_RETRYABLE], TABLE_FIELDS, { state: TABLE_STATE, retry: true });
register(["TABLE_COMMAND_CANCELLED"], [], { state: ["query", "total", "rowCount"] });
register([
    "CLI_START_FAILED", "CLI_EXIT_NONZERO", "CLI_OUTPUT_LIMIT", "CLI_CANCELLED",
    "BASH_INPUT_INVALID", "BASH_SESSION_UNAVAILABLE", "BASH_DENIED_BY_USER", "BASH_POLICY_DENIED",
    "BASH_START_FAILED", "BASH_OUTPUT_LIMIT", "BASH_CANCELLED",
], processFields);
register(["CLI_BUSY", "CLI_TIMEOUT", "BASH_CONCURRENT", "BASH_TIMEOUT"], processFields, { retry: true });
register(["SKILL_UNAVAILABLE", "SKILL_PATH_ESCAPE", "SKILL_LOAD_FAILED"], ["name", "availableSkills", "total", "reason", "causeCode"]);
register(["RAG_BUSY"], ["reason"], { retry: true, reasonValues: ["rag_busy"], message: "RAG retrieval is busy." });
register(["RAG_RETRIEVAL_FAILED"], ["reason"], {
    reasonValues: ragReasons,
    reasonFallback: "retrieval_failed",
    message: "RAG retrieval failed.",
});

const fallback = policies.TOOL_EXECUTION_FAILED;
export const toolErrorPolicy = (code) => policies[code] ?? fallback;
export const isKnownToolErrorCode = (code) => typeof code === "string" && Object.hasOwn(policies, code);
export const isKnownCauseCode = (value) => typeof value === "string" && [
    "ENOENT", "EACCES", "EPERM", "ECONNREFUSED", "ECONNRESET", "ETIMEDOUT",
    "EAI_AGAIN", "ENOTFOUND", "EPIPE", "ERR_INVALID_URL", "UND_ERR_CONNECT_TIMEOUT", "UND_ERR_SOCKET",
].includes(value);
