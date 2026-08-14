/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { once } from "node:events";
import { createServer } from "node:http";
import test from "node:test";
import { createMsinsightTools } from "../../native-agent/tools/msinsightTools.mjs";

test("native msinsight command uses the host capability token", async (t) => {
    const capabilityToken = "native-command-capability";
    const server = createServer((req, res) => {
        const url = new URL(req.url, `http://${req.headers.host}`);
        if (url.pathname !== "/api/frontend-commands/request" || req.method !== "POST" || url.searchParams.get("capabilityToken") !== capabilityToken) {
            res.writeHead(401, { "content-type": "application/json" });
            res.end(JSON.stringify({ error: "Unauthorized" }));
            return;
        }
        res.writeHead(200, { "content-type": "application/json" });
        res.end(JSON.stringify({ result: { app: { activeModule: "Timeline" } } }));
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(() => server.close());
    const { port } = server.address();
    const msinsight = createMsinsightTools({
        baseUrl: `http://127.0.0.1:${port}`,
        capabilityToken,
    }).find(({ name }) => name === "msinsight");

    assert.deepEqual(
        await msinsight.execute({ command: "observe", args: {} }, { sessionId: "session-1" }),
        { app: { activeModule: "Timeline" } },
    );
});

test("native msinsight forwards structured command requests", async (t) => {
    let requestBody;
    const server = createServer((req, res) => {
        let body = "";
        req.on("data", (chunk) => { body += chunk; });
        req.on("end", () => {
            requestBody = JSON.parse(body);
            res.writeHead(200, { "content-type": "application/json" });
            res.end(JSON.stringify({ result: { commands: [] } }));
        });
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(() => server.close());
    const { port } = server.address();
    const tools = createMsinsightTools({ baseUrl: `http://127.0.0.1:${port}` });

    await tools[0].execute({ command: "help", args: { command: "observe" } }, { sessionId: "session-1" });

    assert.deepEqual(tools.map(({ name }) => name), ["msinsight"]);
    assert.equal(requestBody.command, "help");
    assert.deepEqual(requestBody.args, { command: "observe" });
    assert.equal(requestBody.sessionId, "session-1");
    assert.equal(typeof requestBody.requestId, "string");
    assert.equal(Number.isFinite(requestBody.deadline), true);
});
