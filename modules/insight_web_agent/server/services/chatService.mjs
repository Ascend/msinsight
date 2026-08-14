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
import { getSessionContext } from "../state/runtimeState.mjs";
import { setAgentCapabilities } from "./capabilityService.mjs";
import { appendChunk, appendContentBlock, setAgentActivity, setLocalTitle, upsertToolCall } from "./messageService.mjs";

export const createChatService = ({ acpAdapter, acpClient, eventBus, sessionService, skillService, state, sessionManager, contextAssembler, frontendCommandService, systemPrompt = "" }) => {
    const adapter = acpAdapter ?? acpClient;
    const serviceContext = { eventBus, state };

    const initialize = async ({ targetAdapter = adapter, broadcast = true, refreshSessions = true } = {}) => {
        try {
            console.log("Initializing ACP agent");
            const init = await targetAdapter.request("initialize", {
                protocolVersion: 1,
                clientCapabilities: {
                    fs: { readTextFile: false, writeTextFile: false },
                    terminal: false,
                    auth: { terminal: false },
                },
                clientInfo: { name: "insight-web-agent", version: "0.1.0" },
            });
            console.log(JSON.stringify(init));
            state.initialized = true;
            state.agentError = undefined;
            state.agentInfo = init.agentInfo ?? init.agent_info;
            setAgentCapabilities(state, init.agentCapabilities ?? init.agent_capabilities ?? {});
            state.availableSkills = await skillService.list();
            if (refreshSessions) await sessionService.refreshSessions();
            if (broadcast) sessionService.broadcastState();
            console.log(`Connected to ${init.agentInfo?.name ?? "ACP agent"} ${init.agentInfo?.version ?? ""}`.trim());
        } catch (error) {
            state.initialized = false;
            state.agentError = error.message;
            if (broadcast) sessionService.broadcastState();
            console.error(`Failed to initialize ACP agent: ${error.message}`);
        }
    };

    const prompt = async (text, options = {}) => {
        const rawText = String(text ?? "").trim();
        const parsedPrompt = await skillService.extractFromPrompt(text);
        const promptText = parsedPrompt.text;
        const images = normalizeImages(options.images);
        const selectedSkills = parsedPrompt.skills;
        if (!promptText && !images.length && !selectedSkills.length) {
            console.warn("Prompt rejected: message is empty");
            return { error: "message cannot be empty", status: 400 };
        }
        if (!state.initialized) {
            console.warn(`Prompt rejected: ACP agent is not initialized, error=${state.agentError ?? ""}`);
            return { error: state.agentError ?? "ACP agent is not initialized", status: 503 };
        }
        try {
            let sessionId = String(options.sessionId ?? "").trim() || undefined;
            if (options.newSession) {
                console.log("Prompt is creating a new session");
                sessionId = await sessionService.createSessionContext({ messages: [], mode: options.mode });
                if (options.mode) await sessionService.setMode(options.mode, sessionId);
                sessionService.broadcastState();
            }

            if (!sessionId) {
                console.warn("Prompt rejected: sessionId is required");
                return { error: "sessionId is required", status: 400 };
            }
            const sessionContext = getSessionContext(state, sessionId);
            if (sessionContext.pendingPrompt) {
                console.warn(`Prompt rejected: another prompt is running, sessionId=${sessionId}`);
                return { error: "another prompt is running", status: 409 };
            }
            const hiddenContext = await contextAssembler?.assemble?.(sessionContext, options.pageObservation);

            sessionContext.pendingPrompt = true;
            await sessionService.applyPreferredModel(sessionId);

            const displayText = rawText || imagePromptTitle(images);
            if (!sessionContext.messages.some((message) => message.role === "user")) {
                setLocalTitle(state, sessionId, displayText);
            }

            const assistant = { id: crypto.randomUUID(), role: "assistant", text: "", thinking: "" };
            const userMessage = { id: crypto.randomUUID(), role: "user", text: displayText, images };
            sessionContext.messages.push(userMessage, assistant);
            eventBus.broadcast({ type: "message_added", sessionId, message: userMessage });
            eventBus.broadcast({ type: "message_added", sessionId, message: assistant });
            eventBus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: true });

            runPrompt(sessionId, promptText, images, selectedSkills, hiddenContext, assistant);
            sessionService.refreshSessions();
            return { ok: true, sessionId };
        } catch (error) {
            const sessionId = String(options.sessionId ?? "").trim();
            const sessionContext = getSessionContext(state, sessionId);
            if (sessionContext) sessionContext.pendingPrompt = false;
            appendChunk(serviceContext, sessionId, "assistant", "text", `Error: ${error.message}`);
            eventBus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: false });
            console.error(`Prompt setup failed: sessionId=${sessionId}, error=${error.message}`);
            return { error: error.message, status: 500 };
        }
    };

    const runPrompt = async (sessionId, promptText, images, selectedSkills, hiddenContext, assistant) => {
        try {
            console.log(`Prompt execution started: sessionId=${sessionId}, textLength=${promptText.length}, images=${images.length}, skills=${selectedSkills.length}, hiddenContext=${Boolean(hiddenContext)}`);
            await adapter.request("session/prompt", {
                sessionId,
                prompt: createPromptContent(promptText, images, selectedSkills, hiddenContext),
            });
            console.log(`Prompt execution completed: sessionId=${sessionId}`);

            if (!assistant.text && !assistant.thinking && !assistant.toolCalls?.length) {
                const sessionContext = getSessionContext(state, sessionId);
                sessionContext.messages = sessionContext.messages.filter((message) => message !== assistant);
                eventBus.broadcast({ type: "message_removed", sessionId, id: assistant.id });
            }
        } catch (error) {
            console.error(`Prompt execution failed: sessionId=${sessionId}, error=${error.message}`);
            appendChunk(serviceContext, sessionId, "assistant", "text", `Error: ${error.message}`);
        } finally {
            const sessionContext = getSessionContext(state, sessionId);
            sessionContext.pendingPrompt = false;
            eventBus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: false });
        }
    };

    const cancel = async (sessionId) => {
        if (!sessionId) {
            console.warn("Cancel rejected: sessionId is required");
            return { error: "sessionId is required", status: 400 };
        }
        frontendCommandService?.cancelSession?.(sessionId);
        try {
            await adapter.request("session/cancel", { sessionId });
            console.log(`ACP session cancel completed: sessionId=${sessionId}`);
        } catch (error) {
            console.warn(`Failed to cancel session ${sessionId} with ACP: ${error.message}`);
        }
        const sessionContext = getSessionContext(state, sessionId);
        sessionContext.pendingPrompt = false;
        eventBus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: false });
        return { ok: true };
    };

    const handleAcpNotification = (message) => {
        if (message.method !== "session/update") return;
        const params = message.params ?? {};
        const sessionId = params.sessionId ?? params.session_id;
        const update = params.update ?? params.sessionUpdate ?? params.session_update ?? params;
        const kind = normalizeUpdateKind(update.sessionUpdate ?? update.session_update ?? update.type ?? update.kind);

        if (kind === "available_commands_update") {
            state.availableCommands = normalizeAvailableCommands(update.availableCommands ?? update.available_commands ?? []);
            sessionService.broadcastState();
            return;
        }

        if (!sessionId) return;
        getSessionContext(state, sessionId);
        sessionManager?.handleAgentEvent?.(sessionId, { kind, update, params });

        if (kind === "session_info_update") {
            const title = update.title;
            if (typeof title === "string" && title) {
                setLocalTitle(state, params.sessionId, title);
            }
            return;
        }

        if (kind === "config_option_update") {
            sessionService.setConfigOptions(update.configOptions ?? update.config_options ?? [], sessionId);
            return;
        }

        const sessionContext = getSessionContext(state, sessionId);
        if (!sessionContext.pendingPrompt && !sessionContext.replayingHistory) return;

        if (kind === "tool_call" || kind === "tool_call_update") {
            const toolCall = normalizeToolCall(update);
            if (toolCall) upsertToolCall(serviceContext, sessionId, toolCall);
            return;
        }

        if (kind === "agent_status_update") {
            setAgentActivity(serviceContext, sessionId, normalizeAgentActivity(update.activity));
            return;
        }

        const content = extractContent(update);
        if (!content.length) return;

        if (kind === "user_message_chunk") {
            for (const block of content) appendContentBlock(serviceContext, sessionId, "user", block);
            return;
        }

        if (kind === "agent_thought_chunk") {
            for (const block of content) appendContentBlock(serviceContext, sessionId, "assistant", block, "thinking");
            return;
        }

        for (const block of content) appendContentBlock(serviceContext, sessionId, "assistant", block);
    };

    return { cancel, handleAcpNotification, initialize, prompt };
};

const normalizeImages = (images = []) => images
    .map((image) => ({
        id: String(image?.id ?? crypto.randomUUID()),
        name: String(image?.name ?? "image"),
        data: String(image?.data ?? "").trim(),
        mimeType: String(image?.mimeType ?? image?.mime_type ?? "image/png").trim(),
    }))
    .filter((image) => image.data && image.mimeType.startsWith("image/"));

export const createPromptContent = (text, images, skills = [], hiddenContextValue, systemPromptValue) => {
    const content = [];
    const systemPrompt = normalizeSystemPrompt(systemPromptValue);
    if (systemPrompt) {
        content.push(createSystemPromptBlock(systemPrompt));
    }
    const hiddenContext = normalizeHiddenContext(hiddenContextValue);
    if (hiddenContext) {
        content.push(createHiddenContextBlock(hiddenContext));
    }
    for (const skill of skills) {
        content.push({
            type: "text",
            text: `<skill name="${escapeXml(skill.name)}">\n${skill.content}\n</skill>`,
        });
    }
    if (text) content.push({ type: "text", text });
    for (const image of images) {
        content.push({
            type: "image",
            data: image.data,
            mimeType: image.mimeType,
            uri: pastedImageUri(image.name),
        });
    }
    return content;
};

const normalizeSystemPrompt = (value) => String(value ?? "").trim();

const normalizeHiddenContext = (value) => {
    if (!value || typeof value !== "object" || Array.isArray(value)) return undefined;
    return value;
};

const createSystemPromptBlock = (systemPrompt) => ({
    type: "resource",
    resource: {
        uri: "insight-system-prompt://project",
        mimeType: "text/plain",
        text: wrapContextText("insight-system-prompt://project", systemPrompt),
    },
});

const createHiddenContextBlock = (hiddenContext) => ({
    type: "resource",
    resource: {
        uri: "insight-hidden-context://project",
        mimeType: "application/json",
        text: wrapContextText("insight-hidden-context://project", JSON.stringify({
            contextPolicy: "replace_previous_hidden_context",
            instruction: "Use this hidden context as the authoritative project context for this turn and ignore any previous hidden project context in this session.",
            data: hiddenContext,
        })),
    },
});

const wrapContextText = (ref, text) => `<context ref="${ref}"/>\n${text}`;

const escapeXml = (value) => String(value).replace(/[&<>"]/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;" })[char]);

const pastedImageUri = (name) => `zed:///agent/pasted-image?name=${encodeURIComponent(name || "Image")}`;

const imagePromptTitle = (images) => images.length === 1 ? "Image" : `${images.length} images`;

const extractContent = (update) => {
    if (Array.isArray(update.content)) return update.content;
    if (update.content !== undefined) return [update.content];
    if (update.text !== undefined) return [{ type: "text", text: update.text }];
    return [];
};

const normalizeUpdateKind = (kind) => String(kind ?? "")
    .replace(/[A-Z]/g, (letter) => `_${letter.toLowerCase()}`)
    .replace(/^_/, "");

const normalizeAvailableCommands = (commands) => Array.isArray(commands)
    ? commands
        .map((command) => ({
            name: String(command?.name ?? "").trim(),
            description: String(command?.description ?? "").trim(),
            input: command?.input,
        }))
        .filter((command) => command.name)
    : [];

const normalizeAgentActivity = (activity) => {
    if (activity === "analyzing_tool_results") return activity;
    if (activity?.type !== "model_retry") return undefined;
    return {
        type: "model_retry",
        attempt: finiteNumber(activity.attempt),
        maxAttempts: finiteNumber(activity.maxAttempts),
        retryAfterSeconds: finiteNumber(activity.retryAfterSeconds),
    };
};

const normalizeToolCall = (update) => {
    const source = update.toolCall ?? update.tool_call ?? update;
    const toolCallId = String(source.toolCallId ?? source.tool_call_id ?? source.id ?? "").trim();
    if (!toolCallId) return undefined;
    const status = source.status === undefined ? undefined : normalizeToolStatus(source.status);
    const progressValue = source.progress ?? source.content ?? source.message;
    const progress = progressValue === undefined ? undefined : textFromToolValue(progressValue);
    return {
        toolCallId,
        name: optionalToolName(source.name ?? source.title ?? source.toolName ?? source.tool_name),
        status,
        input: limitedToolValue(source.input ?? source.rawInput ?? source.raw_input),
        progress: progress ? progress.slice(0, 2000) : undefined,
        output: limitedToolValue(source.output ?? source.rawOutput ?? source.raw_output),
        startedAt: finiteNumber(source.startedAt ?? source.started_at),
        durationMs: finiteNumber(source.durationMs ?? source.duration_ms),
    };
};

const optionalToolName = (name) => {
    const value = String(name ?? "").trim();
    return value || undefined;
};

const finiteNumber = (value) => {
    const number = Number(value);
    return Number.isFinite(number) ? number : undefined;
};

const normalizeToolStatus = (status) => {
    const value = String(status).toLowerCase();
    if (["completed", "success", "succeeded", "done"].includes(value)) return "completed";
    if (["failed", "error", "cancelled", "canceled"].includes(value)) return "failed";
    return "in_progress";
};

const limitedToolValue = (value) => {
    if (value === undefined || value === null) return undefined;
    const text = textFromToolValue(value);
    return text.length > 4000 ? `${text.slice(0, 4000)}…` : text;
};

const textFromToolValue = (value) => {
    if (typeof value === "string") return value;
    try {
        return JSON.stringify(value, null, 2);
    } catch (_error) {
        return String(value);
    }
};
