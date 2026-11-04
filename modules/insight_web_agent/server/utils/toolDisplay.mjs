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
import { boundedDiagnosticText, readDiagnosticProperty as read } from "./toolErrorBudget.mjs";

export const processSummary = (value) => {
    const exitCode = read(value, "exitCode");
    if (exitCode !== null && !Number.isSafeInteger(exitCode)) return undefined;
    const signal = read(value, "signal");
    return { exitCode, ...(typeof signal === "string" && /^SIG[A-Z0-9]{1,20}$/.test(signal) ? { signal } : {}) };
};

export const processToolDisplay = (value) => {
    const process = processSummary(value);
    if (!process) return undefined;
    const stderr = boundedDiagnosticText(read(value, "stderr"), 2200, "tail");
    const stdout = boundedDiagnosticText(read(value, "stdout"), 900);
    const output = [
        `Exit code: ${process.exitCode ?? "unknown"}${process.signal ? `; signal: ${process.signal}` : ""}`,
        stderr ? `stderr:\n${stderr}` : "", stdout ? `stdout:\n${stdout}` : "",
    ].filter(Boolean).join("\n");
    return { output, process };
};

// ACP supports text blocks, not arbitrary resources or object stringification.
export const toolContentText = (value) => {
    const parts = [];
    let visited = 0;
    const visit = (block, depth) => {
        if (++visited > 32 || depth > 3) return;
        if (typeof block === "string") { parts.push(block); return; }
        if (Array.isArray(block)) {
            for (const child of block.slice(0, 32)) visit(child, depth + 1);
        } else if (read(block, "type") === "text") {
            const text = read(block, "text");
            if (typeof text === "string") parts.push(text);
        } else if (read(block, "type") === "content") visit(read(block, "content"), depth + 1);
    };
    visit(value, 0);
    return boundedDiagnosticText(parts.map((text) => boundedDiagnosticText(text, 3600)).join("\n"), 3600);
};
