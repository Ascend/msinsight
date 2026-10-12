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
import { readFile } from "node:fs/promises";
import { PermissionDeniedError } from "./permissionService.mjs";

const PERMISSION_DENIED = { code: -32000, message: "permission_denied" };
const FILE_IO_ERROR = { code: -32000, message: "file_io_error" };

export const createFileReadService = ({ permissionService, cwd }) => {
    const readTextFile = async ({ sessionId, path, line, limit } = {}) => {
        let policy;
        try {
            policy = await permissionService.ensureReadAllowed({ sessionId, path, cwd: typeof cwd === "function" ? cwd() : cwd, source: "fs/read_text_file" });
        } catch (error) {
            if (error instanceof PermissionDeniedError) return { error: { ...PERMISSION_DENIED, data: { details: error.message } } };
            throw error;
        }

        try {
            const content = await readFile(policy.normalizedPath, "utf8");
            return { result: { content: sliceLines(content, line, limit) } };
        } catch (error) {
            return { error: { ...FILE_IO_ERROR, data: { details: error.message, path: policy.normalizedPath } } };
        }
    };

    return { readTextFile };
};

export const createPermissionHostHandler = ({ permissionService, cwd }) => async (params = {}) => {
    const requestOptions = Array.isArray(params.options) && params.options.length ? params.options : defaultProtocolOptions();
    const target = permissionTarget(params);
    const kind = permissionKind(params, target.path);
    if (kind !== "tool" && !target.value) return { error: { code: -32602, message: "permission_target_required" } };
    if (kind === "bash" && permissionService.isRemembered(params.sessionId, params.rememberKey)) {
        return selectedOutcome(requestOptions, "allowed_once");
    }

    let normalizedPath = target.value;
    if (kind === "filesystem") {
        const policy = await permissionService.evaluate({
            sessionId: params.sessionId,
            path: target.value,
            cwd: typeof cwd === "function" ? cwd() : cwd,
        });
        if (policy.action === "allow") return selectedOutcome(requestOptions, "allowed_once");
        normalizedPath = policy.normalizedPath;
    }

    const result = await permissionService.requestApproval({
        sessionId: params.sessionId,
        path: target.value,
        normalizedPath,
        kind,
        title: params.title,
        target: kind === "tool" ? target.toolName : params.target ?? target.value,
        details: permissionDetails(params, kind, target.toolName),
        rememberKey: params.rememberKey,
        source: "session/request_permission",
        options: requestOptions,
    });
    if (result.allowed) return selectedOutcome(requestOptions, result.state);
    if (result.state === "denied") return selectedOutcome(requestOptions, "denied");
    return cancelledOutcome();
};

const defaultProtocolOptions = () => [
    { optionId: "allow_once", kind: "allow_once", name: "Allow once" },
    { optionId: "allow_always", kind: "allow_always", name: "Allow always" },
    { optionId: "deny", kind: "reject_once", name: "Deny" },
];

// 只有明确的文件类 Tool 才进入路径策略；普通 MCP Tool 即使参数名叫 path 也必须走通用 Tool 授权。
const permissionKind = (params, path) => {
    const toolKind = String(params.toolCall?.kind ?? "").toLowerCase();
    if (params.kind === "bash" || toolKind === "execute") return "bash";
    if (params.kind === "filesystem" || (path && ["read", "edit", "search"].includes(toolKind))) return "filesystem";
    return "tool";
};

const permissionTarget = (params) => {
    const locations = params.toolCall?.locations;
    const path = params.path
        ?? (Array.isArray(locations) ? locations.find((location) => location?.path)?.path : undefined)
        ?? params.toolCall?.rawInput?.path;
    const toolName = String(params.toolCall?.title ?? params.title ?? "").trim() || "Tool";
    const isBash = params.kind === "bash" || String(params.toolCall?.kind ?? "").toLowerCase() === "execute";
    const value = isBash ? params.target ?? params.toolCall?.rawInput?.command : path ?? params.target;
    return { path, toolName, value };
};

const permissionDetails = (params, kind, toolName) => {
    const details = params.details && typeof params.details === "object" ? { ...params.details } : {};
    if (kind === "bash" && !details.cwd && params.toolCall?.rawInput?.cwd) details.cwd = params.toolCall.rawInput.cwd;
    if (kind !== "tool") return Object.keys(details).length ? details : undefined;
    const input = permissionInputPreview(params.toolCall?.rawInput);
    return {
        ...details,
        toolName,
        ...(input === undefined ? {} : { input }),
    };
};

const permissionInputPreview = (input) => {
    if (!input || typeof input !== "object" || !Object.keys(input).length) return undefined;
    try {
        const text = JSON.stringify(input);
        return text.length <= 4000 ? input : `${text.slice(0, 3999)}…`;
    } catch (_error) {
        return "[Unserializable tool input]";
    }
};

const selectedOutcome = (options, state) => {
    const optionId = selectOption(options, state);
    return optionId ? { result: { outcome: { outcome: "selected", optionId } } } : cancelledOutcome();
};
const cancelledOutcome = () => ({ result: { outcome: { outcome: "cancelled" } } });

const selectOption = (options, state) => {
    const desiredKinds = state === "allowed_once"
        ? ["allow_once"]
        : state === "allowed_always"
            ? ["allow_always"]
            : ["reject_once", "reject_always"];
    return options.find((option) => desiredKinds.includes(option.kind))?.optionId
        ?? options.find((option) => desiredKinds.includes(option.optionId))?.optionId;
};

const sliceLines = (content, line, limit) => {
    if (line === undefined && limit === undefined) return content;
    const lines = content.split(/\r?\n/);
    const start = Math.max(0, Number(line ?? 0));
    const count = Number(limit ?? lines.length);
    return lines.slice(start, start + count).join("\n");
};
