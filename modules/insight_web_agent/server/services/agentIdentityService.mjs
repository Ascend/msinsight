/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
export const BUILTIN_AGENT_NAME = "msinsight-native";

export const agentLaunchKey = (agent) => JSON.stringify({
    command: normalizeCommand(agent?.command),
    args: Array.isArray(agent?.args) ? agent.args.map((arg) => String(arg).trim()) : [],
});

export const withAgentIdentity = (agent, kind = agent?.kind ?? "configured") => ({
    ...agent,
    kind,
    launchKey: agentLaunchKey(agent),
});

const normalizeCommand = (command) => {
    const value = String(command ?? "").trim().replaceAll("\\", "/");
    return process.platform === "win32" ? value.toLowerCase().replace(/\.(?:cmd|bat|exe)$/i, "") : value;
};
