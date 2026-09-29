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

import { mkdir, writeFile } from "node:fs/promises";
import { homedir } from "node:os";
import { join } from "node:path";

export const SESSION_EXPORT_FORMAT = "msinsight.agent-session.v1";

export const resolveExportDirectory = (override, runtime = {}) => {
    const env = runtime.env ?? process.env;
    const requested = String(override ?? env.INSIGHT_SESSION_EXPORT_DIR ?? "").trim();
    if (requested) return requested;

    const platform = runtime.platform ?? process.platform;
    const home = runtime.home ?? homedir();
    if (platform === "win32") {
        return join(env.USERPROFILE || env.HOME || home, "Downloads");
    }
    const xdgDownloadDir = String(env.XDG_DOWNLOAD_DIR ?? "").trim();
    if (xdgDownloadDir) return xdgDownloadDir;
    return join(env.HOME || home, "Downloads");
};

/** 功能：把导出文档写到当前平台的用户下载目录，仅桌面 App 使用。 */
export const writeSessionExport = async (document, directory, runtime) => {
    const targetDirectory = resolveExportDirectory(directory, runtime);
    await mkdir(targetDirectory, { recursive: true });
    const savedPath = join(targetDirectory, document.filename);
    await writeFile(savedPath, JSON.stringify(document, null, 2), "utf8");
    return savedPath;
};

/** 功能：把已归一化的会话消息打成可下载的 JSON 文档（内含 markdown 副本）。 */
export const buildSessionExport = ({
    sessionId,
    title,
    updatedAt,
    messages,
    agent,
    exportedAt = new Date().toISOString(),
}) => {
    const document = {
        format: SESSION_EXPORT_FORMAT,
        exportedAt,
        agent: {
            name: agent?.name ?? null,
            version: agent?.version ?? null,
        },
        session: {
            sessionId,
            title: String(title ?? "").trim() || sessionId,
            updatedAt: updatedAt ?? null,
        },
        messages: Array.isArray(messages) ? messages : [],
    };
    return {
        ...document,
        markdown: renderSessionMarkdown(document),
        filename: buildExportFilename(document),
    };
};

export const renderSessionMarkdown = (document) => {
    const lines = [
        `# ${document.session.title}`,
        "",
        `- Agent: ${document.agent.name ?? "unknown"}`,
        `- Session ID: ${document.session.sessionId}`,
        `- Exported: ${document.exportedAt}`,
        "",
    ];
    if (document.messages.length === 0) {
        lines.push("_This session has no messages._", "");
        return lines.join("\n");
    }
    for (const message of document.messages) {
        const role = message.role === "assistant" ? "Assistant" : "User";
        lines.push(`## ${role}`);
        if (message.completionStatus) lines.push(`- Status: ${message.completionStatus}`);
        const blocks = Array.isArray(message.content) ? message.content : [];
        if (blocks.length === 0 && typeof message.content === "string") {
            lines.push("", message.content, "");
            continue;
        }
        for (const block of blocks) {
            if (block.type === "thinking" && block.text) {
                lines.push("", "### Thinking", "", ...quoteLines(block.text));
                continue;
            }
            if (block.type === "tool" && block.toolCall) {
                const tool = block.toolCall;
                lines.push("", `### Tool: ${tool.name ?? "unknown"}`, "");
                if (tool.status) lines.push(`- Status: ${tool.status}`);
                if (tool.input) lines.push("", "Input:", "", fence(tool.input));
                if (tool.output) lines.push("", "Output:", "", fence(tool.output));
                continue;
            }
            if (block.type === "text" && block.text) lines.push("", block.text);
        }
        lines.push("");
    }
    return `${lines.join("\n").trim()}\n`;
};

export const buildExportFilename = (document) => {
    const title = sanitizeFilenamePart(document.session.title, 40);
    const id = sanitizeFilenamePart(String(document.session.sessionId).slice(-12), 12);
    const stamp = String(document.exportedAt).slice(0, 19).replace(/[:T]/g, "-");
    return `insight-session-${title}-${id}-${stamp}.json`;
};

const sanitizeFilenamePart = (value, maxLength) => {
    const cleaned = String(value ?? "")
        .normalize("NFC")
        .replace(/[^\p{L}\p{N}._-]+/gu, "-")
        .replace(/^-+|-+$/g, "")
        .slice(0, maxLength);
    return cleaned || "session";
};

const quoteLines = (text) => String(text).split(/\r?\n/).map((line) => `> ${line}`);

const fence = (text) => `\`\`\`\n${String(text).replace(/```/g, "'''")}\n\`\`\``;
