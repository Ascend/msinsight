/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { createHash } from "node:crypto";
import { basename, extname } from "node:path";
import { parse as parseYaml } from "yaml";

const FRONTMATTER_PATTERN = /^---\r?\n([\s\S]*?)\r?\n---\r?\n?([\s\S]*)$/;
const AGENT_ID_PATTERN = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const MODES = new Set(["primary", "subagent", "all"]);
const PERMISSION_BEHAVIORS = new Set(["allow", "ask", "deny"]);
const SUPPORTED_FIELDS = new Set(["name", "description", "mode", "permission"]);

/** 功能：解析 OpenCode 兼容的 Primary Agent Markdown，并返回运行时定义和诊断。 */
export const parseAgentMarkdown = ({ filePath, content, source }) => {
    const id = basename(filePath, extname(filePath));
    if (!AGENT_ID_PATTERN.test(id)) throw new Error(`invalid agent id: ${id}`);
    const match = String(content ?? "").replace(/^﻿/, "").match(FRONTMATTER_PATTERN);
    if (!match) throw new Error("missing YAML frontmatter");
    const [, yaml, markdown] = match;
    const frontmatter = parseFrontmatter(yaml);
    const name = String(frontmatter.name ?? "").trim();
    const description = String(frontmatter.description ?? "").trim();
    if (!description) throw new Error("description is required");
    const mode = String(frontmatter.mode ?? "all").trim();
    if (!MODES.has(mode)) throw new Error(`unsupported mode: ${mode}`);
    const body = markdown.trim();
    if (!body) throw new Error("agent instructions are required");
    const permission = parsePermission(frontmatter.permission);
    const bashRules = parseBashRules(permission.bash);
    const diagnostics = unsupportedFieldDiagnostics(frontmatter, permission, filePath, source);
    const fingerprint = createHash("sha256").update(JSON.stringify({
        id,
        name,
        description,
        mode,
        body,
        bashRules,
        source: { id: source.id, kind: source.kind },
    })).digest("hex");
    return { id, name, description, mode, body, bashRules, fingerprint, source, diagnostics };
};

const parseFrontmatter = (yaml) => {
    const value = parseYaml(yaml);
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("frontmatter must be an object");
    return value;
};

const parsePermission = (permission) => {
    if (permission === undefined) return {};
    if (!permission || typeof permission !== "object" || Array.isArray(permission)) throw new Error("permission must be an object");
    return permission;
};

const parseBashRules = (rules) => {
    if (rules === undefined) return [];
    if (!rules || typeof rules !== "object" || Array.isArray(rules)) throw new Error("permission.bash must be an object");
    return Object.entries(rules).map(([pattern, behavior]) => {
        const normalizedPattern = String(pattern).trim();
        const normalizedBehavior = String(behavior).trim();
        if (!normalizedPattern) throw new Error("permission.bash contains an empty command pattern");
        if (!PERMISSION_BEHAVIORS.has(normalizedBehavior)) {
            throw new Error(`invalid Bash permission behavior for '${normalizedPattern}': ${normalizedBehavior}`);
        }
        return { pattern: normalizedPattern, behavior: normalizedBehavior };
    });
};

const unsupportedFieldDiagnostics = (frontmatter, permission, filePath, source) => [
    ...Object.keys(frontmatter).filter((field) => !SUPPORTED_FIELDS.has(field)).map((field) => ({
        code: "AGENT_FIELD_IGNORED",
        message: `Unsupported Agent frontmatter field is ignored: ${field}`,
        path: filePath,
        sourceId: source.id,
        resourceId: basename(filePath, extname(filePath)),
    })),
    ...Object.keys(permission).filter((field) => field !== "bash").map((field) => ({
        code: "AGENT_PERMISSION_IGNORED",
        message: `Unsupported Agent permission field is ignored: ${field}`,
        path: filePath,
        sourceId: source.id,
        resourceId: basename(filePath, extname(filePath)),
    })),
];
