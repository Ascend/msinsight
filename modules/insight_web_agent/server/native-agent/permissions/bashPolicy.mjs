/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { resolve } from "node:path";

const MAX_TIMEOUT_MS = 5 * 60 * 1000;
const DEFAULT_TIMEOUT_MS = 30 * 1000;
const COMMON_DENIED_COMMANDS = [
    /(^|[\s;&|()`/])git(?=[\s;&|]|$)(?=[^;\r\n&|]*\bpush\b)(?=[^;\r\n&|]*(?:--force|-f)(?:\s|$))/i,
    /(^|[\s;&|()`/])git(?=[\s;&|]|$)(?=[^;\r\n&|]*\breset\s+--hard(?:\s|$))/i,
    /(^|[\s;&|()`/])git(?=[\s;&|]|$)(?=[^;\r\n&|]*\bclean\b)(?=[^;\r\n&|]*\s-[^\s]*f)/i,
];
const BASH_DENIED_COMMANDS = [
    /(^|[\s;&|()`/])sudo(?:\s|$)/i,
    /(^|[\s;&|()`/])shutdown(?:\s|$)/i,
    /(^|[\s;&|()`/])reboot(?:\s|$)/i,
    /(^|[\s;&|()`/])(?:nohup|disown|bg)(?:\s|$)/i,
    /(^|[\s;&|()`/])rm(?=[^;\r\n&|]*\s-[^\s]*r)(?=[^;\r\n&|]*\s-[^\s]*f)(?:\s|$)/i,
];
const POWERSHELL_DENIED_COMMANDS = [
    /(^|[\s;|&()])remove-item(?=[^;\r\n|&]*\s-recurse(?:\s|$))(?=[^;\r\n|&]*\s-force(?:\s|$))/i,
    /(^|[\s;|&()])(?:stop-computer|restart-computer)(?:\s|$)/i,
    /(^|[\s;|&()])start-process(?=[^;\r\n|&]*\s-verb\s+runas(?:\s|$))/i,
];
const SHELL_CONTROL_SYNTAX = /[\r\n;&|<>`$()]/;

/** 功能：校验并规范化 Bash 输入，强制首版前台和会话文件系统边界。 */
export const normalizeBashInput = async (input, session, defaultCwd) => {
    const command = String(input?.command ?? "").trim();
    if (!command) throw new Error("Bash command is required");
    if (input?.run_in_background === true) throw new Error("Background Bash is not supported");
    if (input?.env && Object.keys(input.env).length) throw new Error("Bash environment overrides are not supported");
    const timeout = Number(input?.timeout ?? DEFAULT_TIMEOUT_MS);
    if (!Number.isFinite(timeout) || timeout < 1000 || timeout > MAX_TIMEOUT_MS) {
        throw new Error(`Bash timeout must be between 1000 and ${MAX_TIMEOUT_MS} milliseconds`);
    }
    const requestedCwd = String(input?.cwd ?? "").trim();
    const cwd = resolve(defaultCwd, requestedCwd || ".");
    // 临时放开 Bash cwd 的 Session roots 限制；相对路径以 Native Agent workspace 为基准。
    return { command, timeout, cwd, run_in_background: false };
};

/** 功能：按产品硬策略和 Agent 命令模式裁决 Bash，并返回可记忆的规范规则。 */
export const evaluateBashPolicy = ({ command, rules = [], shellKind = "bash" }) => {
    const productCommand = normalizeForProductPolicy(command);
    const denyCommand = productCommand.replace(/["']/g, "");
    const deniedCommands = shellKind === "powershell"
        ? [...COMMON_DENIED_COMMANDS, ...POWERSHELL_DENIED_COMMANDS]
        : [...COMMON_DENIED_COMMANDS, ...BASH_DENIED_COMMANDS];
    if (hasUnquotedBackgroundOperator(productCommand) || deniedCommands.some((pattern) => pattern.test(denyCommand))) {
        const policyName = shellKind === "powershell" ? "PowerShell" : "Bash";
        return { behavior: "deny", message: `Command is denied by the product ${policyName} policy`, normalizedRule: command };
    }
    const matched = rules
        .map((rule, index) => ({ ...rule, index, specificity: rule.pattern === "*" ? 0 : rule.pattern.replaceAll("*", "").length }))
        .filter((rule) => matchesRule(command, rule.pattern))
        .sort((left, right) => right.specificity - left.specificity || left.index - right.index)[0];
    const normalizedCommand = normalizeCommand(command);
    if (!matched) return { behavior: "ask", normalizedRule: normalizedCommand };
    if (matched.behavior !== "deny" && hasShellControlSyntax(command)) {
        return { behavior: "ask", normalizedRule: normalizedCommand, message: "Compound shell commands require user approval" };
    }
    return {
        behavior: matched.behavior,
        normalizedRule: matched.behavior === "ask" && matched.pattern === "*" ? normalizedCommand : matched.pattern,
        message: matched.behavior === "deny" ? "Command is denied by the selected Primary Agent" : undefined,
    };
};

const matchesRule = (command, pattern) => {
    if (pattern === "*") return true;
    if (!pattern.includes("*")) return command === pattern;
    const expression = pattern.split("*").map(escapeRegExp).join(".*");
    return new RegExp(`^${expression}$`).test(command);
};

const escapeRegExp = (value) => value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

const hasShellControlSyntax = (command) => SHELL_CONTROL_SYNTAX.test(command);
const hasUnquotedBackgroundOperator = (command) => {
    let quote;
    for (let index = 0; index < command.length; index += 1) {
        const char = command[index];
        if (char === "\\") {
            index += 1;
            continue;
        }
        if (quote) {
            if (char === quote) quote = undefined;
            continue;
        }
        if (char === "'" || char === "\"") {
            quote = char;
            continue;
        }
        if (char === "&" && !["&", ">"].includes(command[index - 1]) && !["&", ">"].includes(command[index + 1])) return true;
    }
    return false;
};
const normalizeForProductPolicy = (command) => command.replace(/\\(?=[A-Za-z])/g, "");
const normalizeCommand = (command) => command.trim();
