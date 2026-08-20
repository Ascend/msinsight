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
    const kind = params.kind === "bash" ? "bash" : "filesystem";
    const path = permissionPath(params, kind);
    if (!path) return { error: { code: -32602, message: "permission_target_required" } };
    if (kind === "bash" && permissionService.isRemembered(params.sessionId, params.rememberKey)) {
        return { result: { outcome: { outcome: "selected", optionId: selectOption(requestOptions, "allowed_once") } } };
    }

    let normalizedPath = path;
    if (kind === "filesystem") {
        const policy = await permissionService.evaluate({
            sessionId: params.sessionId,
            path,
            cwd: typeof cwd === "function" ? cwd() : cwd,
        });
        if (policy.action === "allow") {
            return { result: { outcome: { outcome: "selected", optionId: selectOption(requestOptions, "allowed_once") } } };
        }
        normalizedPath = policy.normalizedPath;
    }

    const result = await permissionService.requestApproval({
        sessionId: params.sessionId,
        path,
        normalizedPath,
        kind,
        title: params.title,
        target: params.target,
        details: params.details,
        rememberKey: params.rememberKey,
        source: "session/request_permission",
        options: requestOptions,
    });
    if (result.allowed) {
        return { result: { outcome: { outcome: "selected", optionId: selectOption(requestOptions, result.state) } } };
    }
    if (result.state === "denied") {
        return { result: { outcome: { outcome: "selected", optionId: selectOption(requestOptions, "denied") } } };
    }
    return { result: { outcome: { outcome: "cancelled" } } };
};

const defaultProtocolOptions = () => [
    { optionId: "allow_once", kind: "allow_once", name: "Allow once" },
    { optionId: "allow_always", kind: "allow_always", name: "Allow always" },
    { optionId: "deny", kind: "reject_once", name: "Deny" },
];

const permissionPath = (params, kind) => {
    const locations = params.toolCall?.locations;
    const firstLocationPath = Array.isArray(locations) ? locations.find((location) => location?.path)?.path : undefined;
    if (kind === "bash") return params.target ?? params.toolCall?.rawInput?.command;
    return params.path ?? firstLocationPath ?? params.toolCall?.rawInput?.path ?? params.target;
};

const selectOption = (options, state) => {
    const desiredKind = state === "allowed_once" ? "allow_once" : state === "allowed_always" ? "allow_always" : "reject_once";
    return options.find((option) => option.kind === desiredKind)?.optionId
        ?? options.find((option) => option.optionId === desiredKind)?.optionId
        ?? options[0]?.optionId;
};

const sliceLines = (content, line, limit) => {
    if (line === undefined && limit === undefined) return content;
    const lines = content.split(/\r?\n/);
    const start = Math.max(0, Number(line ?? 0));
    const count = Number(limit ?? lines.length);
    return lines.slice(start, start + count).join("\n");
};
