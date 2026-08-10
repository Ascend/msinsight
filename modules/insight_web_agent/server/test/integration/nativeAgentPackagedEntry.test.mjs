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
import { access, mkdtemp, readFile, readdir, rm } from "node:fs/promises";
import { createServer } from "node:http";
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

const waitForResponse = (child, responseId, stderr) => new Promise((resolveResponse, reject) => {
    const messages = [];
    let output = "";
    const cleanup = () => {
        clearTimeout(timeout);
        child.stdout.off("data", onData);
        child.off("error", onError);
        child.off("exit", onExit);
    };
    const finish = (callback, value) => {
        cleanup();
        callback(value);
    };
    const onData = (chunk) => {
        output += chunk;
        while (output.includes("\n")) {
            const newline = output.indexOf("\n");
            const line = output.slice(0, newline).trim();
            output = output.slice(newline + 1);
            if (!line) continue;
            const message = JSON.parse(line);
            messages.push(message);
            if (message.id === responseId) finish(resolveResponse, { response: message, messages });
        }
    };
    const onError = (error) => finish(reject, error);
    const onExit = (code) => finish(reject, new Error(`Native agent exited with code ${code}. stderr: ${stderr()}`));
    const timeout = setTimeout(
        () => finish(reject, new Error(`Timed out waiting for response ${responseId}. stderr: ${stderr()}`)),
        5000,
    );
    child.stdout.setEncoding("utf8");
    child.stdout.on("data", onData);
    child.once("error", onError);
    child.once("exit", onExit);
});

const writeJsonLine = (child, message) => {
    child.stdin.write(`${JSON.stringify(message)}\n`);
};

test("packaged native agent loads Blade SDK and reaches the Responses API", async () => {
    await access(packagedNativeAgentPath);
    const cwd = await mkdtemp(join(tmpdir(), "msinsight-native-packaged-sdk-"));
    const storeDir = join(cwd, ".mindstudio_insight", ".msinsight_native_agent");
    const requests = [];
    const modelServer = createServer((req, res) => {
        let body = "";
        req.setEncoding("utf8");
        req.on("data", (chunk) => { body += chunk; });
        req.on("end", () => {
            requests.push({ method: req.method, url: req.url, authorization: req.headers.authorization, body });
            res.writeHead(401, { "content-type": "application/json" });
            res.end(JSON.stringify({ error: "invalid test API key" }));
        });
    });
    modelServer.listen(0, "127.0.0.1");
    await once(modelServer, "listening");
    const { port } = modelServer.address();
    const child = spawn(process.execPath, [packagedNativeAgentPath], {
        cwd,
        env: {
            ...process.env,
            MSINSIGHT_NATIVE_PROVIDER: "openai",
            MSINSIGHT_NATIVE_MODEL: "test-model",
            MSINSIGHT_NATIVE_BASE_URL: `http://127.0.0.1:${port}/v1`,
            MSINSIGHT_NATIVE_API_KEY: "test-key",
            MSINSIGHT_NATIVE_STORE_DIR: storeDir,
        },
        stdio: ["pipe", "pipe", "pipe"],
    });
    let stderr = "";
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk) => { stderr += chunk; });

    try {
        let responsePromise = waitForResponse(child, 1, () => stderr);
        writeJsonLine(child, { jsonrpc: "2.0", id: 1, method: "session/new", params: {} });
        const sessionNew = (await responsePromise).response;
        const sessionId = sessionNew.result.sessionId;

        responsePromise = waitForResponse(child, 2, () => stderr);
        writeJsonLine(child, {
            jsonrpc: "2.0",
            id: 2,
            method: "session/prompt",
            params: { sessionId, prompt: [{ type: "text", text: "test" }] },
        });
        const promptResult = await responsePromise;

        assert.deepEqual(promptResult.response, { jsonrpc: "2.0", id: 2, result: {} });
        assert.equal(requests.length > 0, true);
        assert.equal(requests[0].method, "POST");
        assert.equal(requests[0].url, "/v1/responses");
        assert.equal(requests[0].authorization, "Bearer test-key");
        assert.equal(JSON.parse(requests[0].body).model, "test-model");
        assert.equal(stderr.includes("Failed to load @blade-ai/agent-sdk"), false);
        const chunks = promptResult.messages
            .filter((message) => message.method === "session/update")
            .map((message) => message.params?.update?.content?.text ?? "")
            .join("");
        assert.match(chunks, /Unauthorized|invalid test API key/i);
        const bladeSessions = await readdir(join(storeDir, "blade", "sessions"));
        assert.equal(bladeSessions.some((name) => name.endsWith(".jsonl")), true);
    } finally {
        if (child.exitCode === null) {
            child.kill();
            await once(child, "exit");
        }
        modelServer.close();
        await once(modelServer, "close");
        await rm(cwd, { recursive: true, force: true });
    }
});

test("packaged native agent responds to ACP initialize and session/new", async () => {
    try {
        await access(packagedNativeAgentPath);
    } catch (error) {
        throw new Error(`Packaged native agent does not exist at ${packagedNativeAgentPath}. Run pnpm server:build first. ${error.message}`);
    }

    const cwd = await mkdtemp(join(tmpdir(), "msinsight-native-packaged-"));
    const storeDir = join(cwd, ".mindstudio_insight", ".msinsight_native_agent");
    const child = spawn(process.execPath, [packagedNativeAgentPath], {
        cwd,
        env: { ...process.env, MSINSIGHT_NATIVE_STORE_DIR: storeDir },
        stdio: ["pipe", "pipe", "pipe"],
    });
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

        const store = JSON.parse(await readFile(join(storeDir, "sessions.json"), "utf8"));
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

const assertPackagedUnsupportedProvider = async (provider) => {
    await access(packagedNativeAgentPath);
    const cwd = await mkdtemp(join(tmpdir(), `msinsight-native-packaged-${provider}-`));
    const storeDir = join(cwd, ".mindstudio_insight", ".msinsight_native_agent");
    const child = spawn(process.execPath, [packagedNativeAgentPath], {
        cwd,
        env: {
            ...process.env,
            MSINSIGHT_NATIVE_PROVIDER: provider,
            MSINSIGHT_NATIVE_API_KEY: "test-key",
            MSINSIGHT_NATIVE_STORE_DIR: storeDir,
        },
        stdio: ["pipe", "pipe", "pipe"],
    });
    let stderr = "";
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk) => { stderr += chunk; });

    try {
        let responsePromise = waitForResponse(child, 1, () => stderr);
        writeJsonLine(child, { jsonrpc: "2.0", id: 1, method: "session/new", params: {} });
        const sessionId = (await responsePromise).response.result.sessionId;

        responsePromise = waitForResponse(child, 2, () => stderr);
        writeJsonLine(child, {
            jsonrpc: "2.0",
            id: 2,
            method: "session/prompt",
            params: { sessionId, prompt: [{ type: "text", text: "test" }] },
        });
        const promptResult = await responsePromise;
        const chunks = promptResult.messages
            .filter((message) => message.method === "session/update")
            .map((message) => message.params?.update?.content?.text ?? "")
            .join("");

        assert.deepEqual(promptResult.response, { jsonrpc: "2.0", id: 2, result: {} });
        assert.match(chunks, new RegExp(`Fallback reason: unsupported_provider:${provider}`));
    } finally {
        if (child.exitCode === null) {
            child.kill();
            await once(child, "exit");
        }
        await rm(cwd, { recursive: true, force: true });
    }
};

test("packaged native agent reports unsupported providers explicitly", async () => {
    for (const provider of ["azure", "google", "custom-provider"]) {
        await assertPackagedUnsupportedProvider(provider);
    }
});
