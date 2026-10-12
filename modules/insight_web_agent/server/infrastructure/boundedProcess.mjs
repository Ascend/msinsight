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
import { spawn } from "node:child_process";

export const runBoundedProcess = ({ executable, args, cwd, env, signal, timeoutMs, maxOutputBytes, spawnProcess = spawn }) => new Promise((resolve, reject) => {
    let child;
    try {
        child = spawnProcess(executable, args, {
            cwd,
            env,
            stdio: ["ignore", "pipe", "pipe"],
            windowsHide: true,
            detached: process.platform !== "win32",
            shell: false,
        });
    } catch (error) {
        reject(processError("CLI_START_FAILED", `Failed to start process: ${error.message}`));
        return;
    }
    let stdout = "";
    let stderr = "";
    let outputBytes = 0;
    let terminationError;
    let terminationFallback;
    let settled = false;

    const cleanup = () => {
        clearTimeout(timeout);
        clearTimeout(terminationFallback);
        signal?.removeEventListener("abort", abort);
    };
    const finish = (callback, value) => {
        if (settled) return;
        settled = true;
        cleanup();
        callback(value);
    };
    const terminate = (error) => {
        if (terminationError) return;
        terminationError = error;
        terminateProcessTree(child, spawnProcess);
        terminationFallback = setTimeout(() => {
            terminationError.stdout = stdout;
            terminationError.stderr = stderr;
            finish(reject, terminationError);
        }, 5000);
    };
    const append = (channel, chunk) => {
        const data = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
        const remaining = Math.max(0, maxOutputBytes - outputBytes);
        if (remaining) {
            const text = data.subarray(0, remaining).toString("utf8");
            if (channel === "stdout") stdout += text;
            else stderr += text;
        }
        outputBytes += data.length;
        if (outputBytes > maxOutputBytes) {
            terminate(processError("CLI_OUTPUT_LIMIT", `Process output exceeded ${maxOutputBytes} bytes`));
        }
    };
    const abort = () => terminate(processError(
        "CLI_CANCELLED",
        signal?.reason instanceof Error ? signal.reason.message : "Process was cancelled",
    ));
    const timeout = setTimeout(
        () => terminate(processError("CLI_TIMEOUT", `Process timed out after ${timeoutMs} milliseconds`, true)),
        timeoutMs,
    );

    child.stdout?.on("data", (chunk) => append("stdout", chunk));
    child.stderr?.on("data", (chunk) => append("stderr", chunk));
    child.once("error", (error) => finish(reject, processError("CLI_START_FAILED", `Failed to start process: ${error.message}`)));
    child.once("close", (code, childSignal) => {
        if (terminationError) {
            terminationError.stdout = stdout;
            terminationError.stderr = stderr;
            finish(reject, terminationError);
            return;
        }
        finish(resolve, { exitCode: code, signal: childSignal ?? undefined, stdout, stderr });
    });
    signal?.addEventListener("abort", abort, { once: true });
    if (signal?.aborted) abort();
});

const processError = (code, message, retryable = false) => Object.assign(new Error(message), { code, retryable });

const terminateProcessTree = (child, spawnProcess) => {
    if (!child.pid) {
        child.kill?.();
        return;
    }
    if (process.platform === "win32") {
        let fallbackUsed = false;
        const fallback = () => {
            if (fallbackUsed) return;
            fallbackUsed = true;
            child.kill?.();
        };
        try {
            const killer = spawnProcess("taskkill", ["/pid", String(child.pid), "/t", "/f"], {
                stdio: "ignore",
                windowsHide: true,
            });
            killer.once?.("error", fallback);
            killer.once?.("close", (code) => { if (code !== 0) fallback(); });
        } catch (_error) {
            fallback();
        }
        return;
    }
    try {
        process.kill(-child.pid, "SIGKILL");
    } catch (_error) {
        child.kill?.("SIGKILL");
    }
};
