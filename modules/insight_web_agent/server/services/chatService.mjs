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
import { appendChunk, appendContentBlock, setLocalTitle } from "./messageService.mjs";

export const createChatService = ({ acpClient, eventBus, sessionService, skillService, state }) => {
    const serviceContext = { eventBus, state };

    const initialize = async () => {
        try {
            const init = await acpClient.request("initialize", {
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
            await sessionService.refreshSessions();
            sessionService.broadcastState();
            console.log(`Connected to ${init.agentInfo?.name ?? "ACP agent"} ${init.agentInfo?.version ?? ""}`.trim());
        } catch (error) {
            state.initialized = false;
            state.agentError = error.message;
            sessionService.broadcastState();
            console.error(`Failed to initialize ACP agent: ${error.message}`);
        }
    };

    const prompt = async (text, options = {}) => {
        const rawText = String(text ?? "").trim();
        const parsedPrompt = await skillService.extractFromPrompt(text);
        const promptText = parsedPrompt.text;
        const images = normalizeImages(options.images);
        const selectedSkills = parsedPrompt.skills;
        const incomingHiddenContext = normalizeHiddenContext(options.hiddenContext);
        if (!promptText && !images.length && !selectedSkills.length) return { error: "message cannot be empty", status: 400 };
        if (!state.initialized) return { error: state.agentError ?? "ACP agent is not initialized", status: 503 };
        try {
            let sessionId = String(options.sessionId ?? "").trim() || undefined;
            if (options.newSession) {
                sessionId = await sessionService.createSessionContext({ messages: [] });
                if (options.mode) await sessionService.setMode(options.mode, sessionId);
                sessionService.broadcastState();
            }

            if (!sessionId) return { error: "sessionId is required", status: 400 };
            const sessionContext = getSessionContext(state, sessionId);
            if (sessionContext.pendingPrompt) return { error: "another prompt is running", status: 409 };
            const hiddenContext = updateSessionHiddenContext(sessionContext, incomingHiddenContext);

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

            void runPrompt(sessionId, promptText, images, selectedSkills, hiddenContext, assistant);
            void sessionService.refreshSessions();
            return { ok: true, sessionId };
        } catch (error) {
            const sessionId = String(options.sessionId ?? "").trim();
            const sessionContext = getSessionContext(state, sessionId);
            if (sessionContext) sessionContext.pendingPrompt = false;
            appendChunk(serviceContext, sessionId, "assistant", "text", `Error: ${error.message}`);
            eventBus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: false });
            return { error: error.message, status: 500 };
        }
    };

    const runPrompt = async (sessionId, promptText, images, selectedSkills, hiddenContext, assistant) => {
        try {
            await acpClient.request("session/prompt", {
                sessionId,
                prompt: createPromptContent(promptText, images, selectedSkills, hiddenContext),
            });

            if (!assistant.text && !assistant.thinking) {
                const sessionContext = getSessionContext(state, sessionId);
                sessionContext.messages = sessionContext.messages.filter((message) => message !== assistant);
                eventBus.broadcast({ type: "message_removed", sessionId, id: assistant.id });
            }
        } catch (error) {
            appendChunk(serviceContext, sessionId, "assistant", "text", `Error: ${error.message}`);
        } finally {
            const sessionContext = getSessionContext(state, sessionId);
            sessionContext.pendingPrompt = false;
            eventBus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: false });
        }
    };

    const cancel = async (sessionId) => {
        if (!sessionId) return { error: "sessionId is required", status: 400 };
        try {
            await acpClient.request("session/cancel", { sessionId });
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

export const createPromptContent = (text, images, skills = [], hiddenContextValue) => {
    const content = [];
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

const normalizeHiddenContext = (value) => {
    if (!value || typeof value !== "object" || Array.isArray(value)) return undefined;
    return value;
};

const createHiddenContextBlock = (hiddenContext) => ({
    type: "resource",
    resource: {
        uri: "insight-hidden-context://project",
        mimeType: "application/json",
        text: JSON.stringify({
            contextPolicy: "replace_previous_hidden_context",
            instruction: "Use this hidden context as the authoritative project context for this turn and ignore any previous hidden project context in this session.",
            data: hiddenContext,
        }),
    },
});

const updateSessionHiddenContext = (sessionContext, incomingHiddenContext) => {
    if (!incomingHiddenContext) return sessionContext.hiddenContext;
    sessionContext.hiddenContext = incomingHiddenContext;
    return sessionContext.hiddenContext;
};

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
