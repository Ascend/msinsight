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
import { isAbsolute } from "node:path";
import { runBoundedProcess } from "../infrastructure/boundedProcess.mjs";
import { capabilityError } from "./registry.mjs";

const DEFAULT_TIMEOUT_MS = 30000;
const MAX_TIMEOUT_MS = 300000;
const DEFAULT_MAX_OUTPUT_BYTES = 200 * 1024;

/**
 * 将一个固定绝对路径的可执行文件注册成通用 CLI Tool。
 * args 原样映射为 argv，工厂不解析任何具体 CLI 的子命令和参数语义。
 */
export const createCliCapability = ({
    name,
    description,
    executable,
    cwd = process.cwd(),
    env = process.env,
    spawnProcess,
    defaultTimeoutMs = DEFAULT_TIMEOUT_MS,
    maxTimeoutMs = MAX_TIMEOUT_MS,
    maxOutputBytes = DEFAULT_MAX_OUTPUT_BYTES,
    maxConcurrency = 1,
    envKeys = defaultEnvironmentKeys(),
    extraEnv = {},
} = {}) => {
    const definition = createCliCapabilityDefinition({ name, description, maxTimeoutMs });
    if (!isAbsolute(String(executable ?? ""))) {
        throw capabilityError("CAPABILITY_INVALID", `CLI capability '${definition.name}' requires an absolute executable path.`);
    }
    if (!Number.isInteger(maxConcurrency) || maxConcurrency < 1) {
        throw capabilityError("CAPABILITY_INVALID", `CLI capability '${definition.name}' requires a positive maxConcurrency.`);
    }
    let active = 0;

    return {
        ...definition,
        validate(input) {
            validateCliInput(input, maxTimeoutMs);
        },
        async execute(input, context) {
            if (active >= maxConcurrency) {
                throw capabilityError("CLI_BUSY", `CLI capability '${definition.name}' reached its concurrency limit.`, true);
            }
            active += 1;
            try {
                const result = await runBoundedProcess({
                    executable,
                    args: [...input.args],
                    cwd,
                    env: createCliEnvironment(env, envKeys, extraEnv),
                    signal: context.signal,
                    timeoutMs: input.timeoutMs ?? defaultTimeoutMs,
                    maxOutputBytes,
                    spawnProcess,
                });
                if (result.exitCode !== 0) {
                    throw Object.assign(
                        capabilityError("CLI_EXIT_NONZERO", `CLI process exited with code ${result.exitCode}`),
                        result,
                    );
                }
                return result;
            } finally {
                active -= 1;
            }
        },
    };
};

export const createCliCapabilityDefinition = ({ name, description, maxTimeoutMs = MAX_TIMEOUT_MS } = {}) => {
    const toolName = String(name ?? "").trim();
    if (!toolName) throw capabilityError("CAPABILITY_INVALID", "CLI capability name is required.");
    return {
        name: toolName,
        description: String(description ?? ""),
        inputSchema: createCliInputSchema(maxTimeoutMs),
    };
};

const createCliInputSchema = (maxTimeoutMs) => Object.freeze({
    type: "object",
    properties: {
        args: {
            type: "array",
            items: { type: "string" },
            description: "Arguments passed after the executable. Each array item is one argv value.",
        },
        timeoutMs: { type: "integer", minimum: 1000, maximum: maxTimeoutMs },
    },
    required: ["args"],
    additionalProperties: false,
});

const validateCliInput = (input, maxTimeoutMs) => {
    if (!input || typeof input !== "object" || Array.isArray(input)) {
        throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "input must be an object.");
    }
    const unknownFields = Object.keys(input).filter((key) => !["args", "timeoutMs"].includes(key));
    if (unknownFields.length) throw capabilityError("CAPABILITY_INVALID_ARGUMENT", `Unknown input field '${unknownFields[0]}'.`);
    if (!Array.isArray(input.args) || input.args.some((arg) => typeof arg !== "string" || arg.includes("\0"))) {
        throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "args must be an array of strings without null bytes.");
    }
    if (input.timeoutMs !== undefined && (!Number.isInteger(input.timeoutMs) || input.timeoutMs < 1000 || input.timeoutMs > maxTimeoutMs)) {
        throw capabilityError("CAPABILITY_INVALID_ARGUMENT", `timeoutMs must be an integer between 1000 and ${maxTimeoutMs}.`);
    }
};

const createCliEnvironment = (env, envKeys, extraEnv) => ({
    ...Object.fromEntries(envKeys
        .filter((key) => env[key] !== undefined)
        .map((key) => [key, String(env[key])])),
    ...Object.fromEntries(Object.entries(extraEnv)
        .filter(([, value]) => value !== undefined)
        .map(([key, value]) => [key, String(value)])),
});

const defaultEnvironmentKeys = () => process.platform === "win32"
    ? ["PATH", "PATHEXT", "SYSTEMROOT", "WINDIR", "TEMP", "TMP", "USERPROFILE", "APPDATA", "LOCALAPPDATA"]
    : ["PATH", "HOME", "TMPDIR", "LANG", "LC_ALL"];
