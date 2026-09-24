/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { lstatSync } from "node:fs";
import { win32 } from "node:path";

const POWERSHELL_ARGS = [
    "-NoLogo",
    "-NoProfile",
    "-NonInteractive",
    "-Command",
];
const POWERSHELL_ENCODING_PREAMBLE = "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false); "
    + "$OutputEncoding = [System.Text.UTF8Encoding]::new($false); ";

const defaultCandidateExists = (candidate) => {
    try {
        const stat = lstatSync(candidate);
        return stat.isFile() || stat.isSymbolicLink();
    } catch (_error) {
        return false;
    }
};

const candidatePowerShellPaths = (env) => {
    const programFiles = env.ProgramFiles ?? "C:\\Program Files";
    const systemRoot = env.SystemRoot ?? "C:\\Windows";
    const pathCandidates = String(env.PATH ?? "").split(";")
        .map((entry) => entry.trim().replace(/^"|"$/g, ""))
        .filter(Boolean)
        .map((entry) => win32.join(entry, "pwsh.exe"));
    return [
        win32.join(programFiles, "PowerShell", "7", "pwsh.exe"),
        ...pathCandidates,
        win32.join(systemRoot, "System32", "WindowsPowerShell", "v1.0", "powershell.exe"),
    ];
};

export const resolvePowerShellExecutable = ({
    configured,
    platform = process.platform,
    env = process.env,
    candidateExists = defaultCandidateExists,
} = {}) => {
    if (configured) return configured;
    if (platform === "win32") {
        const candidate = candidatePowerShellPaths(env).find(candidateExists);
        if (candidate) return candidate;
    }
    return platform === "win32" ? "powershell.exe" : "pwsh";
};

export const resolveShellRuntime = ({
    platform = process.platform,
    env = process.env,
    resolvePowerShell = resolvePowerShellExecutable,
} = {}) => {
    if (platform === "win32") {
        return {
            kind: "powershell",
            executable: resolvePowerShell({
                configured: env.MSINSIGHT_NATIVE_SHELL_PATH,
                platform,
                env,
            }),
            commandArgs: POWERSHELL_ARGS,
            prepareCommand: (command) => `${POWERSHELL_ENCODING_PREAMBLE}${command}`,
            displayName: "PowerShell",
            description: "Run one foreground, non-interactive PowerShell command inside an allowed filesystem root. Use PowerShell syntax and Windows path conventions. Commands are subject to product policy and may require user approval.",
            modelGuidance: "Use the compatibility tool name \"Bash\" only for foreground, non-interactive commands when host command execution is required. Commands execute in Windows PowerShell, so use PowerShell syntax and Windows path conventions. The tool remains subject to product policy, filesystem boundaries, and user approval.",
        };
    }
    return {
        kind: "bash",
        executable: env.MSINSIGHT_NATIVE_SHELL_PATH ?? env.MSINSIGHT_NATIVE_BASH_PATH ?? "bash",
        commandArgs: ["-lc"],
        prepareCommand: (command) => command,
        displayName: "Bash",
        description: "Run one foreground, non-interactive Bash command inside an allowed filesystem root. Commands are subject to product policy and may require user approval.",
        modelGuidance: "Use Bash only for foreground, non-interactive commands when host command execution is required. Use Bash syntax. Bash remains subject to product policy, filesystem boundaries, and user approval.",
    };
};
