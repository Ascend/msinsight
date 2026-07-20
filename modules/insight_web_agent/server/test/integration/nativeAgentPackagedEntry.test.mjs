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
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { access, mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const testDir = dirname(fileURLToPath(import.meta.url));
const packageRoot = resolve(testDir, "../../..");
const packagedNativeAgentPath = join(packageRoot, "dist-server", "native-agent", "index.mjs");

const waitForResponses = (child, expectedCount, stderr) => new Promise((resolveResponses, reject) => {
    const responses = [];
    let output = "";
    const timeout = setTimeout(() => reject(new Error(`Timed out after ${responses.length} responses. stderr: ${stderr()}`)), 5000);
    child.stdout.setEncoding("utf8");
    child.stdout.on("data", (chunk) => {
        output += chunk;
        while (output.includes("\n")) {
            const newline = output.indexOf("\n");
            const line = output.slice(0, newline).trim();
            output = output.slice(newline + 1);
            if (line) responses.push(JSON.parse(line));
            if (responses.length === expectedCount) {
                clearTimeout(timeout);
                resolveResponses(responses);
            }
        }
    });
    child.once("error", reject);
    child.once("exit", (code) => {
        if (responses.length < expectedCount) reject(new Error(`Native agent exited with code ${code}. stderr: ${stderr()}`));
    });
});

const writeJsonLine = (child, message) => {
    child.stdin.write(`${JSON.stringify(message)}\n`);
};

test("packaged native agent responds to ACP initialize and session/new", async () => {
    try {
        await access(packagedNativeAgentPath);
    } catch (error) {
        throw new Error(`Packaged native agent does not exist at ${packagedNativeAgentPath}. Run pnpm server:build first. ${error.message}`);
    }

    const cwd = await mkdtemp(join(tmpdir(), "msinsight-native-packaged-"));
    const child = spawn(process.execPath, [packagedNativeAgentPath], { cwd, stdio: ["pipe", "pipe", "pipe"] });
    let stderr = "";
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk) => {
        stderr += chunk;
    });

    try {
        const responsesPromise = waitForResponses(child, 2, () => stderr);
        writeJsonLine(child, { jsonrpc: "2.0", id: 1, method: "initialize", params: {} });
        writeJsonLine(child, { jsonrpc: "2.0", id: 2, method: "session/new", params: {} });
        const responses = await responsesPromise;
        const initialize = responses.find((response) => response.id === 1);
        const sessionNew = responses.find((response) => response.id === 2);

        assert.equal(initialize?.result?.agentInfo?.name, "msinsight-native");
        assert.equal(initialize?.result?.agentCapabilities?.loadSession, true);
        assert.match(sessionNew?.result?.sessionId, /^[0-9a-f-]{36}$/i);

        const store = JSON.parse(await readFile(join(cwd, ".msinsight-native", "sessions.json"), "utf8"));
        assert.equal(store.sessions.length, 1);
        assert.equal(store.sessions[0].sessionId, sessionNew.result.sessionId);
    } finally {
        if (child.exitCode === null) {
            child.kill();
            await once(child, "exit");
        }
        await rm(cwd, { recursive: true, force: true });
    }
});
