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
import { mkdtemp, readFile, readdir, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
import test from "node:test";

const nativeAgentPath = resolve("modules/insight_web_agent/server/native-agent/index.mjs");

const waitForResponses = (child, expectedCount) => new Promise((resolveResponses, reject) => {
    const responses = [];
    let output = "";
    const timeout = setTimeout(() => reject(new Error(`Timed out after ${responses.length} responses`)), 5000);
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
        if (responses.length < expectedCount) reject(new Error(`Native agent exited with code ${code}`));
    });
});

test("native agent serializes concurrent session store writes", async () => {
    const cwd = await mkdtemp(resolve(tmpdir(), "msinsight-native-store-"));
    const storeDir = resolve(cwd, ".mindstudio_insight", ".msinsight_native_agent");
    const child = spawn(process.execPath, [nativeAgentPath], {
        cwd,
        env: { ...process.env, MSINSIGHT_NATIVE_STORE_DIR: storeDir },
        stdio: ["pipe", "pipe", "pipe"],
    });
    try {
        const requestCount = 20;
        const responsesPromise = waitForResponses(child, requestCount);
        for (let id = 1; id <= requestCount; id += 1) {
            child.stdin.write(`${JSON.stringify({ jsonrpc: "2.0", id, method: "session/new", params: {} })}\n`);
        }
        const responses = await responsesPromise;
        assert.equal(responses.every((response) => response.result?.sessionId && !response.error), true);

        const files = await readdir(resolve(storeDir, "sessions"));
        assert.equal(files.length, requestCount);
        const records = (await readFile(resolve(storeDir, "sessions", files[0]), "utf8")).trim().split(/\r?\n/).map(JSON.parse);
        assert.equal(records.length, 1);
        assert.equal(records[0].type, "session_metadata");
        assert.equal(records[0].version, 3);
    } finally {
        if (child.exitCode === null) {
            child.kill();
            await once(child, "exit");
        }
        await rm(cwd, { recursive: true, force: true });
    }
});

test("native agent exits successfully when stdin ends", async () => {
    const cwd = await mkdtemp(resolve(tmpdir(), "msinsight-native-eof-"));
    const child = spawn(process.execPath, [nativeAgentPath], {
        cwd,
        env: { ...process.env, MSINSIGHT_NATIVE_STORE_DIR: resolve(cwd, "store") },
        stdio: ["pipe", "pipe", "pipe"],
    });
    try {
        child.stdin.end();
        const [code, signal] = await Promise.race([
            once(child, "exit"),
            new Promise((_, reject) => setTimeout(() => reject(new Error("native agent did not exit after stdin EOF")), 3000)),
        ]);
        assert.equal(code, 0);
        assert.equal(signal, null);
    } finally {
        if (child.exitCode === null) {
            child.kill();
            await once(child, "exit");
        }
        await rm(cwd, { recursive: true, force: true });
    }
});
