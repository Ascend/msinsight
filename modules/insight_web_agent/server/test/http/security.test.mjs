/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { strict as assert } from "node:assert";
import test from "node:test";
import { createApp } from "../../app.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const startFixture = async () => {
    let sseConnections = 0;
    const server = createApp({
        capabilityToken: "test-capability",
        allowedOrigins: ["http://127.0.0.1:9000"],
        state: createRuntimeState(),
        agentService: { list: () => ({ agentServers: [] }) },
        eventBus: {
            connect(_req, res) {
                sseConnections += 1;
                res.writeHead(200, { "content-type": "text/event-stream" });
                res.end();
            },
        },
    });
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
    const { port } = server.address();
    return {
        server,
        url: `http://127.0.0.1:${port}`,
        sseConnections: () => sseConnections,
    };
};

test("requires the capability for all API routes including SSE", async (t) => {
    const fixture = await startFixture();
    t.after(() => fixture.server.close());

    assert.equal((await fetch(`${fixture.url}/api/state`)).status, 401);
    assert.equal((await fetch(`${fixture.url}/api/events?capabilityToken=wrong`)).status, 401);
    assert.equal(fixture.sseConnections(), 0);
    assert.equal((await fetch(`${fixture.url}/api/events?capabilityToken=test-capability`)).status, 200);
    assert.equal(fixture.sseConnections(), 1);
});

test("emits CORS only for the explicit allowed origin and rejects other origins", async (t) => {
    const fixture = await startFixture();
    t.after(() => fixture.server.close());
    const path = `${fixture.url}/api/state?capabilityToken=test-capability`;

    const allowed = await fetch(path, { headers: { origin: "http://127.0.0.1:9000" } });
    assert.equal(allowed.status, 200);
    assert.equal(allowed.headers.get("access-control-allow-origin"), "http://127.0.0.1:9000");

    const rejected = await fetch(path, { headers: { origin: "https://attacker.invalid" } });
    assert.equal(rejected.status, 403);
    assert.equal(rejected.headers.get("access-control-allow-origin"), null);
});
