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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { realpath } from "node:fs/promises";
import { basename, dirname, isAbsolute, join, relative, resolve } from "node:path";

/** 功能：判断请求是否发往指定模型服务，仅比较 URL 的 origin；
 * 输入：请求地址或 Request 对象 input、模型服务地址 baseUrl；
 * 输出：布尔值；
 * 示例：isModelRequest("https://api.example/v1/messages", "https://api.example/v1") 返回 true。 */
export const isModelRequest = (input, baseUrl) => {
    if (!baseUrl) return false;
    const url = typeof input === "string" || input instanceof URL ? String(input) : input?.url;
    try {
        return new URL(url).origin === new URL(baseUrl).origin;
    } catch (_error) {
        return false;
    }
};

/** 功能：将 Retry-After 秒数或 HTTP 日期转换为等待秒数；
 * 输入：响应头 value；
 * 输出：非负整数秒或 undefined；
 * 示例：parseRetryAfterSeconds("30") 返回 30。 */
export const parseRetryAfterSeconds = (value) => {
    const seconds = Number(value);
    if (Number.isFinite(seconds)) return Math.max(0, Math.ceil(seconds));
    const retryAt = Date.parse(String(value ?? ""));
    return Number.isFinite(retryAt) ? Math.max(0, Math.ceil((retryAt - Date.now()) / 1000)) : undefined;
};

/** 功能：判断目标路径规范化后是否位于任一允许根目录内；
 * 输入：目标路径 inputPath、规范根目录数组 canonicalRoots；
 * 输出：Promise<boolean>；
 * 示例：await isAllowedFilesystemPath("docs/a.md", ["D:/repo"]) 返回是否允许。 */
export const isAllowedFilesystemPath = async (inputPath, canonicalRoots) => {
    const value = String(inputPath ?? "").trim();
    if (!value) return false;
    const target = await canonicalPath(isAbsolute(value) ? value : resolve(process.cwd(), value));
    return canonicalRoots.some((root) => {
        const relation = relative(root, target);
        return !relation || (!relation.startsWith("..") && !isAbsolute(relation));
    });
};

/** 功能：解析绝对路径并尽量消除符号链接，路径不存在时保留解析后的绝对路径；
 * 输入：路径 path；
 * 输出：Promise<string>；
 * 示例：await canonicalPath("./docs") 返回规范绝对路径。 */
export const canonicalPath = async (path) => {
    const absolutePath = resolve(path);
    try {
        return await realpath(absolutePath);
    } catch (_error) {
        const parent = dirname(absolutePath);
        if (parent === absolutePath) return absolutePath;
        return join(await canonicalPath(parent), basename(absolutePath));
    }
};

/** 功能：把工具输入或输出安全序列化并限制为 4000 字符；
 * 输入：任意 value；
 * 输出：字符串或 undefined；
 * 示例：limitedToolValue({ path: "a.md" }) 返回格式化 JSON。 */
export const limitedToolValue = (value) => {
    if (value === undefined || value === null) return undefined;
    let text;
    try {
        text = typeof value === "string" ? value : JSON.stringify(value, null, 2);
    } catch (_error) {
        text = String(value);
    }
    return text.length > 4000 ? `${text.slice(0, 4000)}…` : text;
};

/** 功能：把运行时会话转换为会话列表中的轻量展示项；
 * 输入：运行时会话 session；
 * 输出：包含 sessionId、title、updatedAt 的展示对象；
 * 示例：toSessionListItem(session) 返回前端会话列表项。 */
export const toSessionListItem = (session) => ({
    sessionId: session.sessionId,
    title: session.title,
    updatedAt: new Date(session.updatedAt).toISOString(),
});

/** 功能：把运行时会话转换为不含 AI runtime 实例和规范路径缓存的持久化记录；
 * 输入：运行时会话 session；
 * 输出：可写入 sessions.json 的纯数据对象；
 * 示例：toStoredSession(session) 返回持久化会话记录。 */
export const toStoredSession = (session) => ({
    sessionId: session.sessionId,
    title: session.title,
    messages: session.messages,
    bladeSessionId: session.runtimeSessionId ?? session.bladeSessionId,
    hostSystemPrompt: session.hostSystemPrompt,
    bladeSystemPrompt: session.runtimeSystemPrompt ?? session.bladeSystemPrompt,
    bladeContextNeedsRestore: session.runtimeContextNeedsRestore ?? session.bladeContextNeedsRestore,
    projectRoot: session.projectRoot,
    lastPageObservationFingerprint: session.lastPageObservationFingerprint,
    createdAt: session.createdAt,
    updatedAt: session.updatedAt,
});
