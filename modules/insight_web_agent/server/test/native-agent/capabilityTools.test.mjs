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
import { chmod, mkdtemp, rm, writeFile } from "node:fs/promises";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { createCliCapabilityDefinition } from "../../capability-center/cliCapability.mjs";
import { createCapabilityTools, loadNativeCapabilityDefinitions } from "../../native-agent/tools/capabilityTools.mjs";

const MSINSIGHT_DEFINITION = {
    name: "msinsight",
    description: "Operate the page",
    inputSchema: { type: "object", properties: {} },
    requiresApproval: false,
};
const PT_SNAP_DEFINITION = {
    ...createCliCapabilityDefinition({ name: "pt_snap", description: "Run pt-snap" }),
    requiresApproval: true,
};

const createCapabilityServer = async (t, capabilityToken = "native-command-capability") => {
    const requests = [];
    const server = createServer((req, res) => {
        const url = new URL(req.url, `http://${req.headers.host}`);
        if (url.pathname !== "/api/capabilities/invoke"
            || req.method !== "POST"
            || url.searchParams.get("capabilityToken") !== capabilityToken) {
            res.writeHead(401, { "content-type": "application/json" });
            res.end(JSON.stringify({ error: "Unauthorized" }));
            return;
        }
        let body = "";
        req.on("data", (chunk) => { body += chunk; });
        req.on("end", () => {
            requests.push(JSON.parse(body));
            res.writeHead(200, { "content-type": "application/json" });
            res.end(JSON.stringify({ result: { ok: true } }));
        });
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(() => server.close());
    const { port } = server.address();
    return { baseUrl: `http://127.0.0.1:${port}`, capabilityToken, requests };
};

test("native capability adapter exposes only msinsight without a product config", async (t) => {
    const resourceDir = await mkdtemp(join(tmpdir(), "native-capability-empty-"));
    t.after(() => rm(resourceDir, { recursive: true, force: true }));

    const definitions = loadNativeCapabilityDefinitions({ resourceDir });

    assert.deepEqual(definitions.map(({ name }) => name), ["msinsight"]);
    assert.equal(definitions[0].requiresApproval, false);
});

test("native capability adapter rejects an invalid product config", async (t) => {
    const resourceDir = await mkdtemp(join(tmpdir(), "native-capability-invalid-"));
    t.after(() => rm(resourceDir, { recursive: true, force: true }));
    await writeFile(join(resourceDir, "capability-center.json"), JSON.stringify({
        schemaVersion: 1,
        capabilities: [{ type: "cli", name: "invalid", executable: [] }],
    }), "utf8");

    assert.throws(
        () => loadNativeCapabilityDefinitions({ resourceDir }),
        /requires at least one executable candidate/,
    );
});

test("native capability adapter loads CLI definitions from the product config", async (t) => {
    const resourceDir = await mkdtemp(join(tmpdir(), "native-capability-config-"));
    const executable = join(resourceDir, process.platform === "win32" ? "pt-snap.exe" : "pt-snap");
    t.after(() => rm(resourceDir, { recursive: true, force: true }));
    await writeFile(executable, "fake", "utf8");
    if (process.platform !== "win32") await chmod(executable, 0o700);
    await writeFile(join(resourceDir, "capability-center.json"), `${JSON.stringify({
        schemaVersion: 1,
        capabilities: [{
            type: "cli",
            name: "pt_snap",
            description: "Run pt-snap",
            executable: process.platform === "win32" ? "./pt-snap.exe" : "./pt-snap",
        }],
    })}\n`, "utf8");

    const definitions = loadNativeCapabilityDefinitions({ resourceDir, env: { PATH: "" } });
    const tools = createCapabilityTools({ definitions });

    assert.deepEqual(tools.map(({ name }) => name), ["msinsight", "pt_snap"]);
    assert.equal(definitions[0].requiresApproval, false);
    assert.equal(definitions[1].requiresApproval, true);
    assert.deepEqual(tools[1].inputSchema, PT_SNAP_DEFINITION.inputSchema);
});

test("native msinsight forwards structured requests without approval", async (t) => {
    const fixture = await createCapabilityServer(t);
    const hostClient = { request: async () => { throw new Error("approval must not be requested"); } };
    const tools = createCapabilityTools({
        definitions: [MSINSIGHT_DEFINITION],
        sessions: new Map(),
        hostClient,
        baseUrl: fixture.baseUrl,
        capabilityToken: fixture.capabilityToken,
    });

    await tools[0].execute({ command: "help", args: { command: "observe" } }, { sessionId: "session-1" });

    assert.equal(fixture.requests[0].name, "msinsight");
    assert.deepEqual(fixture.requests[0].input, { command: "help", args: { command: "observe" } });
    assert.equal(fixture.requests[0].sessionId, "session-1");
    assert.equal(typeof fixture.requests[0].invocationId, "string");
});

test("native configured capability asks again after allow once", async (t) => {
    const fixture = await createCapabilityServer(t);
    const session = { sessionId: "session-1" };
    const permissionRequests = [];
    const hostClient = {
        async request(method, params) {
            permissionRequests.push({ method, params });
            return { outcome: { outcome: "selected", optionId: "allow_once" } };
        },
    };
    const [tool] = createCapabilityTools({
        definitions: [PT_SNAP_DEFINITION],
        sessions: new Map([[session.sessionId, session]]),
        hostClient,
        baseUrl: fixture.baseUrl,
        capabilityToken: fixture.capabilityToken,
    });

    await tool.execute({ args: ["query"] }, { sessionId: session.sessionId });
    await tool.execute({ args: ["query"] }, { sessionId: session.sessionId });

    assert.equal(permissionRequests.length, 2);
    assert.equal(fixture.requests.length, 2);
    assert.equal(permissionRequests[0].method, "session/request_permission");
    assert.equal(permissionRequests[0].params.kind, "tool");
    assert.equal(permissionRequests[0].params.title, "pt_snap");
    assert.deepEqual(permissionRequests[0].params.toolCall.rawInput, { args: ["query"] });
});

test("native configured capability remembers allow for this session", async (t) => {
    const fixture = await createCapabilityServer(t);
    const session = { sessionId: "session-1" };
    let permissionRequests = 0;
    const hostClient = {
        async request() {
            permissionRequests += 1;
            return { outcome: { outcome: "selected", optionId: "allow_always" } };
        },
    };
    const [tool] = createCapabilityTools({
        definitions: [PT_SNAP_DEFINITION],
        sessions: new Map([[session.sessionId, session]]),
        hostClient,
        baseUrl: fixture.baseUrl,
        capabilityToken: fixture.capabilityToken,
    });

    await tool.execute({ args: ["query"] }, { sessionId: session.sessionId });
    await tool.execute({ args: ["query"] }, { sessionId: session.sessionId });

    assert.equal(permissionRequests, 1);
    assert.equal(fixture.requests.length, 2);
});

test("native configured capability permits only one active invocation per session", async (t) => {
    const fixture = await createCapabilityServer(t);
    const session = { sessionId: "session-1" };
    let approve;
    const approval = new Promise((resolve) => { approve = resolve; });
    const hostClient = { request: () => approval };
    const [tool] = createCapabilityTools({
        definitions: [PT_SNAP_DEFINITION],
        sessions: new Map([[session.sessionId, session]]),
        hostClient,
        baseUrl: fixture.baseUrl,
        capabilityToken: fixture.capabilityToken,
    });
    const first = tool.execute({ args: ["query"] }, { sessionId: session.sessionId });

    await assert.rejects(
        tool.execute({ args: ["query"] }, { sessionId: session.sessionId }),
        /already running/,
    );
    approve({ outcome: { outcome: "selected", optionId: "allow_once" } });
    await first;
});

test("native configured capability does not invoke Host after denial", async (t) => {
    const fixture = await createCapabilityServer(t);
    const session = { sessionId: "session-1" };
    const hostClient = {
        async request() {
            return { outcome: { outcome: "selected", optionId: "deny" } };
        },
    };
    const [tool] = createCapabilityTools({
        definitions: [PT_SNAP_DEFINITION],
        sessions: new Map([[session.sessionId, session]]),
        hostClient,
        baseUrl: fixture.baseUrl,
        capabilityToken: fixture.capabilityToken,
    });

    await assert.rejects(
        tool.execute({ args: ["query"] }, { sessionId: session.sessionId }),
        /denied by the user/,
    );
    assert.equal(fixture.requests.length, 0);
});
