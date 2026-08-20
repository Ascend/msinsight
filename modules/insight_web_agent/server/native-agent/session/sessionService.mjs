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
    if (method === "initialize") return createInitializeResult(context);
    if (method === "session/new") return createSession(context);
    if (method === "session/list") return listSessions(context);
    if (method === "session/load" || method === "session/resume") return loadSession(context, params.sessionId);
    if (method === "session/delete") return deleteSession(context, params.sessionId);
    if (method === "session/prompt") return runPrompt(context, params);
    if (method === "session/cancel") return cancelPrompt(context, params.sessionId);
    if (method === "session/set_config_option") return setConfigOption(context, params);
    const error = new Error(`method not found: ${method}`);
    error.code = JSON_RPC_METHOD_NOT_FOUND;
    throw error;
};

/** 功能：返回 ACP initialize 响应，声明 native-agent 支持的会话能力。 */
const createInitializeResult = ({ agentRegistry, skillRegistry }) => ({
    protocolVersion: 1,
    agentInfo: { name: "msinsight-native", version: "0.1.0" },
    agentCapabilities: {
        loadSession: true,
        sessionCapabilities: { list: {}, delete: {}, resume: {} },
    },
    _meta: {
        "msinsight.dev/primaryAgents": agentRegistry.list(),
        "msinsight.dev/skills": skillRegistry.list(),
        "msinsight.dev/diagnostics": [...agentRegistry.diagnostics(), ...skillRegistry.diagnostics()],
        "msinsight.dev/setConfigOption": true,
    },
});

/** 功能：创建 ACP 会话，初始化文件系统根目录并持久化会话元数据。 */
const createSession = async ({ sessions, sessionStore, filesystem, agentRegistry }) => {
    const sessionId = randomUUID();
    const now = Date.now();
    const filesystemRoots = filesystem.createSessionFilesystemRoots();
    const primaryAgent = agentRegistry.getPrimary("general");
    sessions.set(sessionId, {
        sessionId,
        title: "New session",
        messages: [],
        runtimeSession: undefined,
        hostSystemPrompt: "",
        primaryAgentId: primaryAgent.id,
        primaryAgentFingerprint: primaryAgent.fingerprint,
        primaryAgentBody: primaryAgent.body,
        primaryAgentBashRules: primaryAgent.bashRules,
        primaryAgentError: undefined,
        promptStarted: false,
        projectRoot: undefined,
        lastPageObservationFingerprint: undefined,
        filesystemRoots,
        canonicalFilesystemRoots: await filesystem.canonicalizeFilesystemRoots(filesystemRoots),
        createdAt: now,
        updatedAt: now,
    });
    await sessionStore.save();
    return { sessionId, configOptions: createConfigOptions(agentRegistry, sessions.get(sessionId)) };
};

/** 功能：返回前端可展示的 ACP 会话标识、标题、Agent 名称和更新时间列表。 */
const listSessions = ({ sessions, agentRegistry }) => ({
    sessions: [...sessions.values()].map((session) => ({
        ...toSessionListItem(session),
        primaryAgentName: agentRegistry.getPrimary(session.primaryAgentId)?.name || undefined,
    })),
});

/** 功能：恢复指定 AI runtime 会话并向前端回放对应 ACP 历史。 */
const loadSession = async (context, sessionId) => {
    const session = ensureSession(context, sessionId);
    resolveSessionPrimaryAgent(context, session);
    await context.aiRuntime.restoreSession(session);
    await context.sessionStore.save();
    replaySessionHistory(context, session);
    return { sessionId: session.sessionId, configOptions: createConfigOptions(context.agentRegistry, session) };
};

/** 功能：删除指定 ACP 会话及其关联的 AI runtime 持久化上下文。 */
const deleteSession = async ({ sessions, running, aiRuntime, sessionStore }, sessionId) => {
    const id = String(sessionId ?? "").trim();
    const session = sessions.get(id);
    if (!session) return {};
    const activePrompt = running.get(id);
    activePrompt?.controller.abort();
    aiRuntime.abortSession(session);
    await activePrompt?.completion;
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
    running.get(id)?.controller.abort();
    aiRuntime.abortSession(session);
    return {};
};

/** 功能：执行一次 ACP prompt，更新会话上下文、运行 AI runtime 或诊断降级，并持久化最终状态。 */
const runPrompt = async (context, { sessionId, prompt }) => {
    const id = String(sessionId ?? "").trim();
    const session = ensureSession(context, id);
    resolveSessionPrimaryAgent(context, session);
    if (session.primaryAgentError) throw new Error(session.primaryAgentError);
    if (context.running.has(id)) throw new Error(`session already has a running prompt: ${id}`);
    session.promptStarted = true;
    session.updatedAt = Date.now();
    const controller = new AbortController();
    let resolveCompletion;
    const completion = new Promise((resolve) => {
        resolveCompletion = resolve;
    });
    context.running.set(id, { controller, completion });
    try {
        if (controller.signal.aborted) return { stopReason: "cancelled" };
        const { userText, pageObservation } = await prepareUserPrompt(context, session, prompt);
        try {
            const result = await context.aiRuntime.runPrompt({ session, sessionId: id, userText, pageObservation, controller });
            if (!result.ok && !controller.signal.aborted) throw new Error(result.reason);
        } catch (error) {
            if (!controller.signal.aborted) throw error;
        }
        session.updatedAt = Date.now();
        await context.sessionStore.save();
        return { stopReason: controller.signal.aborted ? "cancelled" : "end_turn" };
    } finally {
        context.running.delete(id);
        resolveCompletion();
    }
};

/** 功能：提取用户 prompt 元数据，更新 Host System Prompt、文件根、标题和用户消息。 */
const prepareUserPrompt = async ({ filesystem }, session, prompt) => {
    const hostSystemPrompt = await resolveHostSystemPrompt(prompt);
    if (hostSystemPrompt) session.hostSystemPrompt = hostSystemPrompt;
    await filesystem.updateSessionFilesystemRoots(session, extractPromptProjectRoot(prompt));
    const userText = extractPromptText(prompt);
    if (userText && session.title === "New session") session.title = userText.slice(0, 80);
    session.updatedAt = Date.now();
    return { userText, pageObservation: extractPromptPageObservation(prompt) };
};

/** 功能：在首次 Prompt 前绑定 Primary Agent，并持久化不可变会话配置。 */
const setConfigOption = async (context, { sessionId, configId, value }) => {
    const session = ensureSession(context, sessionId);
    if (String(configId ?? "") !== "primaryAgent") throw new Error(`unsupported config option: ${configId}`);
    if (session.promptStarted || session.messages.length || context.running.has(session.sessionId)) {
        throw new Error("Primary Agent cannot be changed because this session already has conversation history. Create a new session before selecting another Primary Agent.");
    }
    const primaryAgent = context.agentRegistry.getPrimary(value);
    if (!primaryAgent) throw new Error(`Primary Agent is unavailable: ${value}`);
    await context.aiRuntime.deleteSession(session);
    session.primaryAgentId = primaryAgent.id;
    session.primaryAgentFingerprint = primaryAgent.fingerprint;
    session.primaryAgentBody = primaryAgent.body;
    session.primaryAgentBashRules = primaryAgent.bashRules;
    session.primaryAgentError = undefined;
    session.updatedAt = Date.now();
    await context.sessionStore.save();
    return { configOptions: createConfigOptions(context.agentRegistry, session) };
};

/** 功能：按当前 Registry 恢复 Agent 内容，并标记缺失或变更的持久化会话。 */
const resolveSessionPrimaryAgent = ({ agentRegistry }, session) => {
    session.primaryAgentId = String(session.primaryAgentId ?? "general");
    const primaryAgent = agentRegistry.getPrimary(session.primaryAgentId);
    if (!primaryAgent) {
        session.primaryAgentBody = undefined;
        session.primaryAgentBashRules = [];
        session.primaryAgentError = `Primary Agent is unavailable: ${session.primaryAgentId}`;
        return false;
    }
    const changed = Boolean(session.primaryAgentFingerprint && session.primaryAgentFingerprint !== primaryAgent.fingerprint);
    session.primaryAgentFingerprint = primaryAgent.fingerprint;
    session.primaryAgentBody = primaryAgent.body;
    session.primaryAgentBashRules = primaryAgent.bashRules;
    session.primaryAgentError = undefined;
    return changed;
};

/** 功能：把当前可选 Primary Agents 转换为标准 ACP select config option。 */
const createConfigOptions = (agentRegistry, session) => [{
    id: "primaryAgent",
    name: "Primary Agent",
    description: "Select the analysis role for this native session.",
    category: "mode",
    type: "select",
    currentValue: session.primaryAgentId ?? "general",
    options: createPrimaryAgentOptions(agentRegistry, session.primaryAgentId),
    _meta: session.primaryAgentError ? { "msinsight.dev/error": session.primaryAgentError } : undefined,
}];

const createPrimaryAgentOptions = (agentRegistry, currentAgentId) => {
    const agents = agentRegistry.list();
    const definedIds = new Set(agents.map((agent) => agent.id));
    const validOptions = agents
        .filter((agent) => agent.mode === "primary" || agent.mode === "all")
        .map((agent) => ({
            value: agent.id,
            name: agent.source.kind === "development" ? `${agent.name || displayAgentName(agent.id)} [Development]` : agent.name || displayAgentName(agent.id),
            description: agent.description,
            _meta: createAgentOptionMetadata(agent.source, true, agent.diagnostics),
        }));
    const selectableIds = new Set(validOptions.map((option) => option.value));
    const invalidOptions = uniqueUnavailableAgents(agentRegistry.diagnostics(), definedIds);
    const currentMissing = currentAgentId && !selectableIds.has(currentAgentId) && !invalidOptions.some((option) => option.value === currentAgentId)
        ? [{
            value: currentAgentId,
            name: `${displayAgentName(currentAgentId)} [Unavailable]`,
            description: `Primary Agent is unavailable: ${currentAgentId}`,
            _meta: createAgentOptionMetadata(
                { id: "missing", kind: "bundled" },
                false,
                [{ code: "AGENT_MISSING", message: `Primary Agent is unavailable: ${currentAgentId}`, resourceId: currentAgentId }],
            ),
        }]
        : [];
    return [...validOptions, ...invalidOptions, ...currentMissing];
};

const createAgentOptionMetadata = (source, available, diagnostics) => ({
    "msinsight.dev/source": source,
    "msinsight.dev/available": available,
    "msinsight.dev/diagnostics": diagnostics,
});

const uniqueUnavailableAgents = (diagnostics, validIds) => {
    const options = new Map();
    for (const diagnostic of diagnostics) {
        if (diagnostic.code !== "AGENT_INVALID" || !diagnostic.resourceId || validIds.has(diagnostic.resourceId)) continue;
        const previous = options.get(diagnostic.resourceId);
        const sourceKind = String(diagnostic.sourceId ?? "").startsWith("development:") ? "development" : "bundled";
        const previousDiagnostics = previous?._meta?.["msinsight.dev/diagnostics"] ?? [];
        options.set(diagnostic.resourceId, {
            value: diagnostic.resourceId,
            name: `${displayAgentName(diagnostic.resourceId)} [Unavailable]`,
            description: diagnostic.message,
            _meta: createAgentOptionMetadata(
                { id: diagnostic.sourceId, kind: sourceKind },
                false,
                [...previousDiagnostics, diagnostic],
            ),
        });
    }
    return [...options.values()];
};

const displayAgentName = (id) => String(id).split("-").map((word) => word ? `${word[0].toUpperCase()}${word.slice(1)}` : word).join(" ");

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
        for (const block of message.content ?? []) {
            if (block.type === "text") {
                notifier.sendSessionContentChunk(session.sessionId, message.role === "user" ? "user_message_chunk" : "agent_message_chunk", block.text);
            } else if (message.role === "assistant" && block.type === "thinking") {
                notifier.sendSessionThinkingChunk(session.sessionId, block.text);
            } else if (message.role === "assistant" && block.type === "tool") {
                notifier.sendToolCallUpdate(session.sessionId, "tool_call", block.toolCall);
            }
        }
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

/** 功能：优先提取 ACP Host System Prompt，缺失时沿用原有 workspace 规则回退。 */
const resolveHostSystemPrompt = async (prompt = []) => extractHostSystemPrompt(prompt) || await readWorkspaceRules();

const extractHostSystemPrompt = (prompt = []) => {
    const blocks = Array.isArray(prompt) ? prompt : [prompt];
    return blocks
        .filter((block) => block?.type === "resource" && block.resource?.uri === "insight-system-prompt://project")
        .map((block) => unwrapContextText(block.resource?.text, "insight-system-prompt://project"))
        .join("\n\n")
        .trim();
};

const readWorkspaceRules = async () => {
    for (const fileName of ["AGENTS.md", "CLAUDE.md"]) {
        try {
            const content = (await readFile(join(process.cwd(), fileName), "utf8")).replace(/^﻿/, "").trim();
            if (content) return content;
        } catch (_error) {
            continue;
        }
    }
    return "";
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
