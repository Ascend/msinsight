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
import { randomUUID } from "node:crypto";
import { readFile } from "node:fs/promises";
import { join } from "node:path";
import { toSessionListItem } from "../shared/utils.mjs";

const JSON_RPC_METHOD_NOT_FOUND = -32601;

/** 功能：创建 native-agent 会话用例服务，编排 ACP 会话、AI runtime、存储和前端通知。 */
export const createNativeSessionService = (context) => ({
    handleRequest: (method, params) => handleRequest(context, method, params),
});

/** 功能：将 ACP 方法名路由到初始化、会话管理、提示、取消或配置处理函数。 */
const handleRequest = async (context, method, params) => {
    if (method === "initialize") return createInitializeResult();
    if (method === "session/new") return createSession(context);
    if (method === "session/list") return listSessions(context);
    if (method === "session/load" || method === "session/resume") return loadSession(context, params.sessionId);
    if (method === "session/delete") return deleteSession(context, params.sessionId);
    if (method === "session/prompt") return runPrompt(context, params);
    if (method === "session/cancel") return cancelPrompt(context, params.sessionId);
    if (method === "session/set_config_option") return { configOptions: [] };
    const error = new Error(`method not found: ${method}`);
    error.code = JSON_RPC_METHOD_NOT_FOUND;
    throw error;
};

/** 功能：返回 ACP initialize 响应，声明 native-agent 支持的会话能力。 */
const createInitializeResult = () => ({
    protocolVersion: 1,
    agentInfo: { name: "msinsight-native", version: "0.1.0" },
    agentCapabilities: {
        loadSession: true,
        sessionCapabilities: { list: true, delete: true, resume: true },
    },
});

/** 功能：创建 ACP 会话，初始化文件系统根目录并持久化会话元数据。 */
const createSession = async ({ sessions, sessionStore, filesystem }) => {
    const sessionId = randomUUID();
    const now = Date.now();
    const filesystemRoots = filesystem.createSessionFilesystemRoots();
    sessions.set(sessionId, {
        sessionId,
        title: "New session",
        messages: [],
        runtimeSession: undefined,
        runtimeSessionId: undefined,
        bladeSession: undefined,
        bladeSessionId: undefined,
        hostSystemPrompt: "",
        runtimeSystemPrompt: "",
        bladeSystemPrompt: "",
        runtimeContextNeedsRestore: false,
        bladeContextNeedsRestore: false,
        projectRoot: undefined,
        lastPageObservationFingerprint: undefined,
        filesystemRoots,
        canonicalFilesystemRoots: await filesystem.canonicalizeFilesystemRoots(filesystemRoots),
        createdAt: now,
        updatedAt: now,
    });
    await sessionStore.save();
    return { sessionId, configOptions: [] };
};

/** 功能：返回前端可展示的 ACP 会话标识、标题和更新时间列表。 */
const listSessions = ({ sessions }) => ({
    sessions: [...sessions.values()].map(toSessionListItem),
});

/** 功能：恢复指定 AI runtime 会话并向前端回放对应 ACP 历史。 */
const loadSession = async (context, sessionId) => {
    const session = ensureSession(context, sessionId);
    await context.aiRuntime.restoreSession(session);
    replaySessionHistory(context, session);
    return { sessionId: session.sessionId, configOptions: [] };
};

/** 功能：删除指定 ACP 会话及其关联的 AI runtime 持久化上下文。 */
const deleteSession = async ({ sessions, aiRuntime, sessionStore }, sessionId) => {
    const id = String(sessionId ?? "").trim();
    const session = sessions.get(id);
    if (!session) return {};
    await aiRuntime.deleteSession(session);
    sessions.delete(id);
    await sessionStore.save();
    return {};
};

/** 功能：中止指定 ACP 会话当前运行的控制器和 AI runtime 请求。 */
const cancelPrompt = ({ sessions, running, aiRuntime }, sessionId) => {
    const id = String(sessionId ?? "");
    const session = sessions.get(id);
    if (!session) return {};
    running.get(id)?.abort();
    aiRuntime.abortSession(session);
    return {};
};

/** 功能：执行一次 ACP prompt，更新会话上下文、运行 AI runtime 或诊断降级，并持久化最终状态。 */
const runPrompt = async (context, { sessionId, prompt }) => {
    const id = String(sessionId ?? "").trim();
    const session = ensureSession(context, id);
    if (context.running.has(id)) throw new Error(`session already has a running prompt: ${id}`);
    const controller = new AbortController();
    context.running.set(id, controller);
    try {
        const { userText, pageObservation } = await prepareUserPrompt(context, session, prompt);
        const result = await context.aiRuntime.runPrompt({ session, sessionId: id, userText, pageObservation, controller });
        if (!result.ok && !controller.signal.aborted) await runFallbackPrompt(context, { session, sessionId: id, userText, reason: result.reason, controller });
        session.updatedAt = Date.now();
        await context.sessionStore.save();
        return {};
    } finally {
        context.running.delete(id);
    }
};

/** 功能：提取用户 prompt 元数据，更新系统提示词、文件根、标题和用户消息。 */
const prepareUserPrompt = async ({ filesystem, aiRuntime }, session, prompt) => {
    const hostSystemPrompt = await resolveHostSystemPrompt(prompt);
    if (hostSystemPrompt) session.hostSystemPrompt = hostSystemPrompt;
    const rootsChanged = await filesystem.updateSessionFilesystemRoots(session, extractPromptProjectRoot(prompt));
    if (rootsChanged) aiRuntime.updateFilesystemContext(session);
    const userText = extractPromptText(prompt);
    if (userText && !session.messages.length) session.title = userText.slice(0, 80);
    session.messages.push({ role: "user", text: userText });
    session.updatedAt = Date.now();
    return { userText, pageObservation: extractPromptPageObservation(prompt) };
};

/** 功能：模型不可用时执行页面观测，并以流式分块输出诊断降级回复。 */
const runFallbackPrompt = async ({ toolRegistry, notifier }, { session, sessionId, userText, reason, controller }) => {
    const observationResult = await toolRegistry.execute("msinsight", { command: "observe", args: {} }, { sessionId });
    const text = createFallbackResponse({ userText, observationResult, tools: toolRegistry.list(), reason });
    for (const chunk of chunkText(text)) {
        if (controller.signal.aborted) break;
        notifier.sendSessionChunk(sessionId, chunk);
    }
    session.messages.push({ role: "assistant", text });
    // 诊断降级回复只向前端输出并写入 ACP sessions.json，不会写入 AI runtime 历史；标记后，下一轮会把这段 ACP 对话补入模型输入，避免用户说“继续”时丢失任务。
    session.runtimeContextNeedsRestore = true;
    session.bladeContextNeedsRestore = true;
};

/** 功能：校验会话标识并返回对应运行时会话，不存在时抛出明确错误。 */
const ensureSession = ({ sessions }, sessionId) => {
    const id = String(sessionId ?? "").trim();
    if (!id) throw new Error("sessionId is required");
    const session = sessions.get(id);
    if (!session) throw new Error(`session not found: ${id}`);
    return session;
};

/** 功能：按原角色、thinking、工具调用和正文顺序向前端回放 ACP 会话历史。 */
const replaySessionHistory = ({ notifier }, session) => {
    for (const message of session.messages) {
        if (message.role === "user") {
            if (message.text) notifier.sendSessionContentChunk(session.sessionId, "user_message_chunk", message.text);
            continue;
        }
        if (message.role !== "assistant") continue;
        if (message.thinking) notifier.sendSessionThinkingChunk(session.sessionId, message.thinking);
        for (const toolCall of message.toolCalls ?? []) notifier.sendToolCallUpdate(session.sessionId, "tool_call", toolCall);
        if (message.text) notifier.sendSessionChunk(session.sessionId, message.text);
    }
};

/** 功能：拼接 ACP prompt 中所有 text 块并去除首尾空白。 */
const extractPromptText = (prompt = []) => {
    const blocks = Array.isArray(prompt) ? prompt : [prompt];
    return blocks
        .filter((block) => block?.type === "text")
        .map((block) => String(block.text ?? ""))
        .join("\n")
        .trim();
};

/** 功能：优先提取 ACP prompt 中的系统提示词，缺失时读取工作区规则文件。 */
const resolveHostSystemPrompt = async (prompt = []) => extractHostSystemPrompt(prompt) || await readWorkspaceRules();

/** 功能：提取 ACP prompt 中 insight-system-prompt://project 资源文本。 */
const extractHostSystemPrompt = (prompt = []) => {
    const blocks = Array.isArray(prompt) ? prompt : [prompt];
    return blocks
        .filter((block) => block?.type === "resource" && block.resource?.uri === "insight-system-prompt://project")
        .map((block) => unwrapContextText(block.resource?.text, "insight-system-prompt://project"))
        .join("\n\n")
        .trim();
};

/** 功能：按 opencode 兼容规则从当前 agent workspace 读取项目规则。 */
const readWorkspaceRules = async () => {
    for (const fileName of ["AGENTS.md", "CLAUDE.md"]) {
        const content = await readOptionalTextFile(join(process.cwd(), fileName));
        if (content) return content;
    }
    return "";
};

/** 功能：读取可选文本文件，缺失或不可读时返回空字符串。 */
const readOptionalTextFile = async (filePath) => {
    try {
        return (await readFile(filePath, "utf8")).replace(/^﻿/, "").trim();
    } catch (_error) {
        return "";
    }
};

/** 功能：移除隐藏资源文本开头的 context ref 标记，返回实际上下文内容。 */
const unwrapContextText = (value, ref) => String(value ?? "").replace(new RegExp(`^<context\\s+ref=["']${escapeRegExp(ref)}["']\\s*/>\\n?`), "");

/** 功能：转义字符串，使其可以安全嵌入正则表达式。 */
const escapeRegExp = (value) => String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

/** 功能：只从宿主控制的隐藏上下文顶层读取 projectRoot，页面 observation 不能扩展文件白名单。 */
const extractPromptProjectRoot = (prompt = []) => {
    const projectRoot = extractPromptHiddenContext(prompt)?.projectRoot;
    return typeof projectRoot === "string" && projectRoot.trim() ? projectRoot.trim() : undefined;
};

const extractPromptPageObservation = (prompt = []) => {
    const observation = extractPromptHiddenContext(prompt)?.pageObservation;
    return observation && typeof observation === "object" && !Array.isArray(observation) ? observation : undefined;
};

const extractPromptHiddenContext = (prompt = []) => {
    const blocks = Array.isArray(prompt) ? prompt : [prompt];
    for (const block of blocks) {
        if (block?.type !== "resource" || block.resource?.uri !== "insight-hidden-context://project") continue;
        try {
            const payload = JSON.parse(unwrapContextText(block.resource?.text, "insight-hidden-context://project"));
            return payload?.data ?? payload;
        } catch (_error) {
            return undefined;
        }
    }
    return undefined;
};

/** 功能：根据用户问题、页面观测、工具列表和降级原因生成诊断回复。 */
const createFallbackResponse = ({ userText, observationResult, tools, reason }) => {
    const observation = observationResult?.observation ?? observationResult;
    const app = observation?.app ?? {};
    const moduleObservation = observation?.module ?? {};
    const lines = ["msinsight-native ACP fallback runtime is running."];
    if (reason) lines.push("", `Fallback reason: ${reason}`);
    lines.push("", "Latest msinsight observe command result:");
    if (!observation) {
        lines.push("- No page observation has been received yet.");
    } else {
        lines.push(`- activeModule: ${app.activeModule ?? moduleObservation.moduleId ?? moduleObservation.module ?? "unknown"}`);
        lines.push(`- scene: ${app.scene ?? "unknown"}`);
        lines.push(`- selectedProjectName: ${app.selectedProjectName ?? "unknown"}`);
        lines.push(`- activeModuleObservation: ${moduleObservation.moduleId ?? "unknown"}`);
    }
    lines.push("", `Available native tools: ${tools.map((tool) => tool.name).join(", ")}`);
    if (userText) lines.push("", `User prompt: ${userText}`);
    return `${lines.join("\n")}\n`;
};

/** 功能：按最多 80 个字符切分流式输出文本。 */
const chunkText = (text) => {
    const chunks = [];
    for (let index = 0; index < text.length; index += 80) chunks.push(text.slice(index, index + 80));
    return chunks;
};
