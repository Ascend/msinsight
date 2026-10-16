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

// 内存优化助手提示词中的占位符，加载时替换为能力中心解析出的内置 pt-snap 命令说明。
export const PT_SNAP_COMMAND_PLACEHOLDER = "{{PT_SNAP_COMMAND}}";
export const PT_SNAP_CAPABILITY_NAME = "pt_snap";

const POSIX_SAFE_ARG = /^[A-Za-z0-9_\/.,:=@%+-]+$/;
const WINDOWS_SAFE_ARG = /^[A-Za-z0-9_\\\/.,:=@%+-]+$/;

/** 把 argv 拼成可直接粘贴到 Shell 的命令前缀；Windows 路径中的空格用双引号包裹。 */
export const formatShellCommand = (argv, platform = process.platform) => argv.map((arg) => {
    const value = String(arg);
    if (platform === "win32") {
        return WINDOWS_SAFE_ARG.test(value) ? value : `"${value.replace(/"/g, '\\"')}"`;
    }
    return POSIX_SAFE_ARG.test(value) ? value : `'${value.replace(/'/g, "'\\''")}'`;
}).join(" ");

const describeAvailable = (command, platform) => [
    `当前产品内置 pt-snap 的 Shell 命令前缀（下文记为 \`<pt-snap>\`）：`,
    "",
    "```text",
    command,
    "```",
    "",
    platform === "win32"
        ? "在 PowerShell 中执行时需在前面加调用运算符 `&`；cmd 和 Git Bash 可直接使用。"
        : "该前缀已按当前平台加引号，可直接在 Shell 中使用。",
].join("\n");

const UNAVAILABLE_NOTICE = [
    "当前构建未找到内置 pt-snap（能力中心未注册 `pt_snap`）。",
    "不要改用 Shell 默认的 `pt-snap`、`python`、`pip` 或其他 Python 环境代替，也不要自行安装；停止 pt-snap 分析，并告知用户当前构建中 pt-snap 不可用。",
].join("\n");

/** 用能力中心已解析的 pt_snap 命令渲染提示词；提示词为空时保持为空，以便上层返回 preset 不可用。 */
export const renderMemoryTuningPrompt = (prompt, { capabilities = [], platform = process.platform } = {}) => {
    const text = String(prompt ?? "");
    if (!text || !text.includes(PT_SNAP_COMMAND_PLACEHOLDER)) return text;
    const capability = capabilities.find((entry) => entry?.name === PT_SNAP_CAPABILITY_NAME && entry.executable);
    const replacement = capability
        ? describeAvailable(formatShellCommand([capability.executable, ...(capability.argsPrefix ?? [])], platform), platform)
        : UNAVAILABLE_NOTICE;
    return text.split(PT_SNAP_COMMAND_PLACEHOLDER).join(replacement);
};
