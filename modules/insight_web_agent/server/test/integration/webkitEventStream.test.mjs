/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";
import test from "node:test";
import { build } from "esbuild";
import { createEventBus } from "../../state/eventBus.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const exec = promisify(execFile);

// Requires a macOS GUI session and the Command Line Tools. Other hosts run the portable SSE tests.
test("system WebKit receives fast failures and completed replies on an otherwise idle SSE stream", {
    skip: process.platform !== "darwin",
    timeout: 30000,
}, async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-webkit-stream-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const bus = createEventBus(createRuntimeState());
    const server = createServer((req, res) => {
        res.setHeader("access-control-allow-origin", "wry://localhost");
        res.setHeader("access-control-allow-headers", "authorization");
        if (req.method === "OPTIONS") { res.writeHead(204); res.end(); return; }
        if (req.headers.authorization !== "Bearer test-token") { res.writeHead(401); res.end(); return; }
        bus.connect(req, res);
        // Multiple writes in one callback exercise WebKit's trailing-chunk buffering.
        const timer = setTimeout(() => {
            for (const sessionId of ["failed", "completed"]) {
                bus.broadcast({ type: "message_activity", sessionId });
                bus.broadcast({ type: "message_content_added", sessionId, block: {
                    type: "text", text: sessionId === "failed" ? "Missing API key" : "Done",
                } });
                bus.broadcast({ type: "prompt_status", sessionId, pendingPrompt: false,
                    completionStatus: sessionId });
            }
        }, 100);
        res.once("close", () => clearTimeout(timer));
    });
    t.after(() => { bus.close(); server.closeAllConnections(); server.close(); });
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
    const url = `http://127.0.0.1:${server.address().port}/api/events`;
    const entry = fileURLToPath(new URL("../../../src/eventStream.ts", import.meta.url));
    const bundled = await build({
        stdin: {
            contents: `import { subscribeEvents } from ${JSON.stringify(entry)};
                const events = [];
                const subscription = subscribeEvents(data => {
                    const event = JSON.parse(data);
                    events.push(event);
                    if (events.filter(e => e.type === 'prompt_status').length === 2) {
                        subscription.close();
                        window.webkit.messageHandlers.probe.postMessage(JSON.stringify(events));
                    }
                });`,
            resolveDir: root,
        },
        bundle: true, write: false, platform: "browser", format: "iife",
        plugins: [{ name: "test-endpoint", setup(build) {
            build.onResolve({ filter: /^\.\/env$/ }, () => ({ path: "env", namespace: "test" }));
            build.onLoad({ filter: /.*/, namespace: "test" }, () => ({ contents:
                `export const eventsUrl = () => ${JSON.stringify(url)};
                 export const capabilityAuthHeaders = () => ({ Authorization: 'Bearer test-token' });`,
            }));
        } }],
    });
    const html = join(root, "probe.html");
    await writeFile(html, `<script>${bundled.outputFiles[0].text}</script>`);
    const binary = join(root, "probe");
    await exec("clang", ["-fobjc-arc", "-framework", "Cocoa", "-framework", "WebKit",
        fileURLToPath(new URL("../fixtures/webkitEventStream.m", import.meta.url)), "-o", binary], { timeout: 15000 });
    const { stdout } = await exec(binary, [html], { timeout: 10000 });
    const events = JSON.parse(stdout.trim());
    assert.deepEqual(events.filter(({ type }) => type === "prompt_status").map(({ completionStatus }) => completionStatus), ["failed", "completed"]);
    assert.deepEqual(events.filter(({ type }) => type === "message_content_added").map(({ block }) => block.text), ["Missing API key", "Done"]);
});
