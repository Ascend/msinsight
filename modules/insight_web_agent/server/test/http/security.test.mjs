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
    let agentSwitches = 0;
    const state = createRuntimeState();
    const server = createApp({
        capabilityToken: "test-capability",
        allowedOrigins: ["http://127.0.0.1:9000"],
        state,
        agentService: {
            list: () => ({ agentServers: [] }),
            switchAgent: async () => {
                agentSwitches += 1;
                return { ok: true, activeAgentName: "next" };
            },
        },
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
        state,
        agentSwitches: () => agentSwitches,
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

test("rejects agent switching while a prompt is busy", async (t) => {
    const fixture = await startFixture();
    t.after(() => fixture.server.close());
    fixture.state.sessionContexts.set("session-1", { pendingPrompt: true });

    const response = await fetch(`${fixture.url}/api/agents/switch?capabilityToken=test-capability`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ name: "next" }),
    });

    assert.equal(response.status, 409);
    assert.deepEqual(await response.json(), { error: "agent_busy", message: "Agent is busy" });
    assert.equal(fixture.agentSwitches(), 0);
});

test("context accepts Framework fields but rejects a client-supplied projectRoot", async (t) => {
    const fixture = await startFixture();
    t.after(() => fixture.server.close());
    const request = (body) => fetch(`${fixture.url}/api/context?capabilityToken=test-capability`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
    });

    const accepted = await request({ profileId: "profile-1", activeModule: "Timeline", ignored: "value" });
    assert.equal(accepted.status, 200);
    assert.deepEqual(fixture.state.activeContext, { profileId: "profile-1", activeModule: "Timeline" });

    const rejected = await request({ profileId: "profile-1", activeModule: "Timeline", projectRoot: "/tmp/attacker" });
    assert.equal(rejected.status, 400);
    assert.deepEqual(fixture.state.activeContext, { profileId: "profile-1", activeModule: "Timeline" });
});
