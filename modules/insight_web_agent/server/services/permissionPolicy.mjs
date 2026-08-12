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
import { realpath } from "node:fs/promises";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";

const normalizeMaybeRealpath = async (targetPath) => {
    const resolved = resolve(String(targetPath ?? ""));
    try {
        return await realpath(resolved);
    } catch (_error) {
        return resolved;
    }
};

export const normalizeReadPath = async (targetPath, cwd = process.cwd()) => {
    const value = String(targetPath ?? "").trim();
    if (!value) throw new Error("path is required");
    const absolute = isAbsolute(value) ? value : resolve(cwd, value);
    return normalizeMaybeRealpath(absolute);
};

export const createDefaultAllowlist = async ({
    rootDir,
    cwd,
    activeAgentName,
    activeAgentWorkspaceKey,
    projectRoot,
    includeDocsRoot = true,
    includeAgentWorkspaceRoot = true,
    includeProjectRoot = true,
    extraPaths = [],
} = {}) => {
    const candidates = [
        includeDocsRoot && rootDir ? join(rootDir, "docs") : undefined,
        includeAgentWorkspaceRoot ? (cwd && activeAgentName ? join(cwd, activeAgentWorkspaceKey ?? activeAgentName) : cwd) : undefined,
        includeProjectRoot ? projectRoot : undefined,
        ...extraPaths,
    ].filter(Boolean);

    const entries = [];
    for (const candidate of candidates) {
        try {
            entries.push(await normalizeReadPath(candidate));
        } catch (_error) {
            // Ignore unavailable optional roots rather than broadening access.
        }
    }
    return entries;
};

export const isPathInside = (targetPath, rootPath) => {
    if (!targetPath || !rootPath) return false;
    if (targetPath === rootPath) return true;
    const relation = relative(rootPath, targetPath);
    return Boolean(relation) && !relation.startsWith("..") && !isAbsolute(relation);
};

export const evaluateReadPolicy = async ({ targetPath, cwd, defaultAllowlist = [], runtimeAllowlist = [] }) => {
    const normalized = await normalizeReadPath(targetPath, cwd);
    const allowlist = [...defaultAllowlist, ...runtimeAllowlist].filter(Boolean);
    const allowedBy = allowlist.find((entry) => isPathInside(normalized, entry));
    return {
        action: allowedBy ? "allow" : "prompt",
        normalizedPath: normalized,
        allowedBy,
    };
};

export const parentAllowlistEntry = (normalizedPath) => dirname(normalizedPath);
