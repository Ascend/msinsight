/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { createHash } from "node:crypto";
import { basename } from "node:path";

export const BUILTIN_AGENT_NAME = "msinsight-native";

export const agentLaunchKey = (agent) => JSON.stringify({
    command: normalizeCommand(agent?.command),
    args: Array.isArray(agent?.args) ? agent.args.map((arg) => String(arg).trim()) : [],
});

export const agentWorkspaceKey = (agent) => {
    if (agent?.kind === "builtin" || agent?.name === BUILTIN_AGENT_NAME) return BUILTIN_AGENT_NAME;
    const commandName = basename(String(agent?.command ?? "agent")).replace(/\.(?:cmd|bat|exe)$/i, "");
    const slug = commandName.toLowerCase().replace(/[^a-z0-9._-]+/g, "-").replace(/^-+|-+$/g, "") || "agent";
    const hash = createHash("sha256").update(agentLaunchKey(agent)).digest("hex").slice(0, 8);
    return `${slug}-${hash}`;
};

export const withAgentIdentity = (agent, kind = agent?.kind ?? "configured") => ({
    ...agent,
    kind,
    launchKey: agentLaunchKey(agent),
    workspaceKey: agentWorkspaceKey({ ...agent, kind }),
});

const normalizeCommand = (command) => {
    const value = String(command ?? "").trim().replaceAll("\\", "/");
    return process.platform === "win32" ? value.toLowerCase().replace(/\.(?:cmd|bat|exe)$/i, "") : value;
};
