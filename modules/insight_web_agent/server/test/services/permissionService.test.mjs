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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { mkdtemp, mkdir, realpath, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { strict as assert } from "node:assert";
import test from "node:test";
import { createRuntimeState } from "../../state/runtimeState.mjs";
import { createPermissionService } from "../../services/permissionService.mjs";
import { createFileReadService, createPermissionHostHandler } from "../../services/fileReadService.mjs";

const createFixture = async () => {
    const root = await mkdtemp(join(tmpdir(), "insight-permission-"));
    const docs = join(root, "docs");
    const cwd = join(root, "agent-workspace");
    const agentRoot = join(cwd, "opencode");
    const external = join(root, "external");
    await Promise.all([mkdir(docs, { recursive: true }), mkdir(agentRoot, { recursive: true }), mkdir(external, { recursive: true })]);
    return { root, docs, cwd, agentRoot, external };
};

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const writeText = (path, text) => writeFile(path, text, "utf8");
const permissionRequests = (events) => events.filter((event) => event.type === "permission_request");
const expectNoPrompt = (fixture) => assert.deepEqual(fixture.events, []);
const evaluatePath = (fixture, path) => fixture.service.evaluate({ sessionId: "s1", path });
const realExternalPath = async (fixture) => realpath(fixture.external);
const waitForPermissionRequest = async (events) => {
    for (let index = 0; index < 100; index += 1) {
        const request = events.findLast((event) => event.type === "permission_request");
        if (request) return request;
        await delay(5);
    }
    throw new Error("permission request was not emitted");
};

const waitForRequestCount = async (events, count) => {
    for (let index = 0; index < 100; index += 1) {
        if (permissionRequests(events).length >= count) return;
        await delay(5);
    }
    throw new Error(`expected ${count} permission requests, got ${permissionRequests(events).length}`);
};

const ensureProjectRoot = async (fixture) => {
    const projectRoot = join(fixture.root, "project");
    await mkdir(projectRoot, { recursive: true });
    fixture.state.activeContext = { projectRoot };
    return projectRoot;
};

const allowRequest = async (fixture, decision) => {
    const request = await waitForPermissionRequest(fixture.events);
    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision });
    return request;
};

const createService = async (options = {}) => {
    const fixture = await createFixture();
    const events = [];
    const state = createRuntimeState();
    state.activeAgentName = "opencode";
    const service = createPermissionService({
        state,
        eventBus: { broadcast: (event) => events.push(event) },
        config: {
            rootDir: fixture.root,
            cwd: fixture.cwd,
            activeAgentName: "opencode",
            permissionRequestTimeoutMs: options.permissionRequestTimeoutMs,
            defaultAllowlist: {
                includeDocsRoot: true,
                includeAgentWorkspaceRoot: true,
                includeProjectRoot: true,
                ...(options.defaultAllowlist ?? {}),
            },
            extraAllowlistPaths: typeof options.extraAllowlistPaths === "function"
                ? options.extraAllowlistPaths(fixture)
                : (options.extraAllowlistPaths ?? []),
        },
        timeoutMs: options.timeoutMs ?? 1000,
    });
    return { ...fixture, events, state, service };
};

test("default allowlist allows docs, agent workspace, and known project root without approval", async () => {
    const fixture = await createService();
    const projectRoot = await ensureProjectRoot(fixture);
    const docsFile = join(fixture.docs, "guide.md");
    const workspaceFile = join(fixture.agentRoot, "prompt.md");
    const projectFile = join(projectRoot, "source.py");
    await writeText(docsFile, "docs");
    await writeText(workspaceFile, "workspace");
    await writeText(projectFile, "project");

    assert.equal((await evaluatePath(fixture, docsFile)).action, "allow");
    assert.equal((await evaluatePath(fixture, workspaceFile)).action, "allow");
    assert.equal((await evaluatePath(fixture, projectFile)).action, "allow");
    expectNoPrompt(fixture);
});

test("session config extra allowlist paths are honored", async () => {
    const fixture = await createService({ extraAllowlistPaths: (ctx) => [ctx.external] });
    const target = join(fixture.external, "extra.txt");
    await writeText(target, "extra");

    assert.equal((await evaluatePath(fixture, target)).action, "allow");
    expectNoPrompt(fixture);
});

test("session config can disable docs root allowlist", async () => {
    const fixture = await createService({ defaultAllowlist: { includeDocsRoot: false } });
    const target = join(fixture.docs, "guide.md");
    await writeText(target, "docs");

    assert.equal((await evaluatePath(fixture, target)).action, "prompt");
});

test("session config can disable agent workspace root allowlist", async () => {
    const fixture = await createService({ defaultAllowlist: { includeAgentWorkspaceRoot: false } });
    const target = join(fixture.agentRoot, "prompt.md");
    await writeText(target, "workspace");

    assert.equal((await evaluatePath(fixture, target)).action, "prompt");
});

test("session config can disable project root allowlist", async () => {
    const fixture = await createService({ defaultAllowlist: { includeProjectRoot: false } });
    const projectRoot = await ensureProjectRoot(fixture);
    const target = join(projectRoot, "source.py");
    await writeText(target, "project");

    assert.equal((await evaluatePath(fixture, target)).action, "prompt");
});

test("allow always stores the parent directory for runtime prefix matching", async () => {
    const fixture = await createService();
    const first = join(fixture.external, "first.txt");
    const second = join(fixture.external, "second.txt");
    await writeText(first, "one");
    await writeText(second, "two");

    const pending = fixture.service.ensureReadAllowed({ sessionId: "s1", path: first });
    await allowRequest(fixture, "allow_always");
    await pending;

    assert.equal((await evaluatePath(fixture, second)).action, "allow");
    assert.deepEqual([...fixture.state.permissionRuntimeAllowlist.get("s1")], [await realExternalPath(fixture)]);
});

test("allow once resolves only the current request", async () => {
    const fixture = await createService();
    const target = join(fixture.external, "target.txt");
    await writeText(target, "content");

    const pending = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    await allowRequest(fixture, "allow_once");
    await pending;

    assert.equal((await evaluatePath(fixture, target)).action, "prompt");
    assert.equal(fixture.state.permissionRuntimeAllowlist.get("s1")?.size ?? 0, 0);
});

test("concurrent requests in one session are shown in FIFO order", async () => {
    const fixture = await createService({ timeoutMs: 1000 });
    const target = join(fixture.external, "same.txt");
    await writeText(target, "content");

    const first = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    const second = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    await waitForRequestCount(fixture.events, 1);
    assert.equal(permissionRequests(fixture.events).length, 1);
    assert.equal(fixture.state.pendingPermissions.size, 2);

    const firstRequest = permissionRequests(fixture.events)[0];
    await fixture.service.respond({ sessionId: "s1", requestId: firstRequest.requestId, decision: "allow_once" });
    await first;
    await waitForRequestCount(fixture.events, 2);
    const secondRequest = permissionRequests(fixture.events)[1];
    assert.notEqual(firstRequest.requestId, secondRequest.requestId);

    await fixture.service.respond({ sessionId: "s1", requestId: secondRequest.requestId, decision: "deny" });
    await assert.rejects(second, /denied/);
});

test("permission queues are independent between sessions", async () => {
    const fixture = await createService();
    const first = fixture.service.requestApproval({ sessionId: "s1", path: "first" });
    const queued = fixture.service.requestApproval({ sessionId: "s1", path: "queued" });
    const other = fixture.service.requestApproval({ sessionId: "s2", path: "other" });
    await waitForRequestCount(fixture.events, 2);

    assert.deepEqual(permissionRequests(fixture.events).map((request) => request.sessionId), ["s1", "s2"]);
    fixture.service.rejectSessionRequests(undefined);
    await Promise.all([first, queued, other]);
});

test("queued requests start timing out only after they are shown", async () => {
    const fixture = await createService({ timeoutMs: 100 });
    const first = fixture.service.requestApproval({ sessionId: "s1", path: "first" });
    fixture.service.updateTimeout(10);
    const second = fixture.service.requestApproval({ sessionId: "s1", path: "second" });

    await delay(20);
    assert.equal(permissionRequests(fixture.events).length, 1);
    assert.equal(fixture.state.pendingPermissions.size, 2);

    const firstRequest = permissionRequests(fixture.events)[0];
    await fixture.service.respond({ sessionId: "s1", requestId: firstRequest.requestId, decision: "allow_once" });
    await first;
    const secondRequest = permissionRequests(fixture.events)[1];
    await fixture.service.respond({ sessionId: "s1", requestId: secondRequest.requestId, decision: "allow_once" });
    await second;
});

test("timeout expires pending requests and does not add allowlist entries", async () => {
    const fixture = await createService({ timeoutMs: 5 });
    const target = join(fixture.external, "slow.txt");
    await writeText(target, "content");

    await assert.rejects(() => fixture.service.ensureReadAllowed({ sessionId: "s1", path: target }), /expired/);

    assert.equal(fixture.events.at(-1).type, "permission_resolved");
    assert.equal(fixture.events.at(-1).state, "expired");
    assert.equal(fixture.state.permissionRuntimeAllowlist.get("s1")?.size ?? 0, 0);
});

test("permission service uses configured timeout when no explicit override is passed", async () => {
    const fixture = await createService({
        permissionRequestTimeoutMs: 5,
        defaultAllowlist: {
            includeDocsRoot: false,
            includeAgentWorkspaceRoot: false,
            includeProjectRoot: false,
        },
    });
    const target = join(fixture.external, "timeout-by-config.txt");
    await writeText(target, "content");

    await assert.rejects(() => fixture.service.ensureReadAllowed({ sessionId: "s1", path: target }), /expired/);
    assert.equal(fixture.events.at(-1).state, "expired");
});

test("invalid and already resolved responses return explicit statuses", async () => {
    const fixture = await createService();
    const target = join(fixture.external, "target.txt");
    await writeText(target, "content");

    const missing = await fixture.service.respond({ sessionId: "s1", requestId: "missing", decision: "allow_once" });
    assert.equal(missing.status, 404);
    assert.equal(missing.error, "permission_request_not_found");
    const invalid = await fixture.service.respond({ sessionId: "s1", requestId: "", decision: "nope" });
    assert.equal(invalid.status, 400);
    assert.equal(invalid.error, "invalid_permission_response");

    const pending = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    const request = await allowRequest(fixture, "deny");
    await assert.rejects(pending, /denied/);
    const resolved = await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "deny" });
    assert.equal(resolved.status, 409);
    assert.equal(resolved.error, "permission_request_resolved");
});

test("generic MCP Tool permission succeeds without a filesystem path", async () => {
    const fixture = await createService();
    const handler = createPermissionHostHandler({ permissionService: fixture.service, cwd: fixture.agentRoot });
    const pending = handler({
        sessionId: "s1",
        toolCall: {
            toolCallId: "call-1",
            title: "msinsight-capabilities_msinsight",
            kind: "other",
            rawInput: {},
        },
        options: [
            { optionId: "once", kind: "allow_once" },
            { optionId: "always", kind: "allow_always" },
            { optionId: "reject", kind: "reject_once" },
        ],
    });
    const request = await waitForPermissionRequest(fixture.events);

    assert.equal(request.kind, "tool");
    assert.equal(request.title, undefined);
    assert.equal(request.target, "msinsight-capabilities_msinsight");
    assert.equal(request.details.toolName, "msinsight-capabilities_msinsight");
    assert.deepEqual(request.actions, ["allow_once", "allow_always", "deny"]);

    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "allow_always" });
    assert.equal((await pending).result.outcome.optionId, "always");
    assert.equal(fixture.state.permissionRuntimeAllowlist.get("s1")?.size ?? 0, 0);
});

test("generic Tool input paths do not enter filesystem permission policy", async () => {
    const fixture = await createService();
    const handler = createPermissionHostHandler({ permissionService: fixture.service, cwd: fixture.agentRoot });
    const pending = handler({
        sessionId: "s1",
        toolCall: { toolCallId: "call-1", title: "remote_tool", kind: "other", rawInput: { path: "virtual://resource" } },
        options: [
            { optionId: "once", kind: "allow_once" },
            { optionId: "reject", kind: "reject_once" },
        ],
    });
    const request = await waitForPermissionRequest(fixture.events);

    assert.equal(request.kind, "tool");
    assert.equal(request.target, "remote_tool");
    assert.deepEqual(request.details.input, { path: "virtual://resource" });

    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "deny" });
    assert.equal((await pending).result.outcome.optionId, "reject");
});

test("generic Tool permission only exposes options provided by the Agent", async () => {
    const fixture = await createService();
    const handler = createPermissionHostHandler({ permissionService: fixture.service, cwd: fixture.agentRoot });
    const pending = handler({
        sessionId: "s1",
        toolCall: { toolCallId: "call-1", title: "remote_tool", kind: "other", rawInput: { value: 1 } },
        options: [
            { optionId: "once", kind: "allow_once" },
            { optionId: "reject-forever", kind: "reject_always" },
        ],
    });
    const request = await waitForPermissionRequest(fixture.events);

    assert.deepEqual(request.actions, ["allow_once", "deny"]);
    assert.deepEqual(request.details.input, { value: 1 });

    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "deny" });
    assert.equal((await pending).result.outcome.optionId, "reject-forever");
});

test("permission outcome cancels instead of selecting an unrelated option", async () => {
    const fixture = await createService();
    const handler = createPermissionHostHandler({ permissionService: fixture.service, cwd: fixture.agentRoot });
    const pending = handler({
        sessionId: "s1",
        toolCall: { toolCallId: "call-1", title: "remote_tool", kind: "other", rawInput: {} },
        options: [{ optionId: "once", kind: "allow_once" }],
    });
    const request = await waitForPermissionRequest(fixture.events);

    assert.deepEqual(request.actions, ["allow_once"]);
    assert.equal((await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "deny" })).status, 400);
    fixture.service.rejectSessionRequests("s1", "invalidated");

    assert.deepEqual((await pending).result.outcome, { outcome: "cancelled" });
});

test("ACP execute Tool calls are rendered as Bash permissions", async () => {
    const fixture = await createService();
    const handler = createPermissionHostHandler({ permissionService: fixture.service, cwd: fixture.agentRoot });
    const pending = handler({
        sessionId: "s1",
        toolCall: {
            toolCallId: "call-1",
            title: "python -V",
            kind: "execute",
            locations: [{ path: fixture.agentRoot }],
            rawInput: { command: "python -V", cwd: fixture.agentRoot },
        },
        options: [
            { optionId: "once", kind: "allow_once" },
            { optionId: "reject", kind: "reject_once" },
        ],
    });
    const request = await waitForPermissionRequest(fixture.events);

    assert.equal(request.kind, "bash");
    assert.equal(request.target, "python -V");

    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "deny" });
    assert.equal((await pending).result.outcome.optionId, "reject");
});

test("Bash allow always is isolated by session and namespaced remember key", async () => {
    const fixture = await createService();
    const handler = createPermissionHostHandler({ permissionService: fixture.service, cwd: fixture.agentRoot });
    const params = {
        sessionId: "s1",
        kind: "bash",
        target: "python -V",
        rememberKey: "bash:general:python -V",
        options: [
            { optionId: "allow_once", kind: "allow_once" },
            { optionId: "allow_always", kind: "allow_always" },
            { optionId: "deny", kind: "reject_once" },
        ],
    };

    const pending = handler(params);
    const request = await waitForPermissionRequest(fixture.events);
    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "allow_always" });
    assert.equal((await pending).result.outcome.optionId, "allow_always");
    assert.equal(fixture.service.isRemembered("s1", params.rememberKey), true);
    assert.equal(fixture.service.isRemembered("s2", params.rememberKey), false);
    assert.equal(fixture.service.isRemembered("s1", "python -V"), false);

    const remembered = await handler(params);
    assert.equal(remembered.result.outcome.optionId, "allow_once");
});

test("cancelling pending requests preserves remembered Session approvals", async () => {
    const fixture = await createService();
    fixture.state.permissionRuntimeAllowlist.set("s1", new Set(["bash:general:python -V"]));
    const pending = fixture.service.requestApproval({
        sessionId: "s1",
        kind: "bash",
        path: "python -c fail",
        rememberKey: "bash:general:python -c fail",
    });
    const queued = fixture.service.requestApproval({ sessionId: "s1", path: "queued" });
    fixture.service.rejectSessionRequests("s1", "invalidated");

    assert.deepEqual(await pending, { allowed: false, state: "invalidated", reason: "invalidated" });
    assert.deepEqual(await queued, { allowed: false, state: "invalidated", reason: "invalidated" });
    assert.equal(permissionRequests(fixture.events).length, 1);
    assert.equal(fixture.service.isRemembered("s1", "bash:general:python -V"), true);

    fixture.service.rejectSessionRequests("s1", "invalidated", true);
    assert.equal(fixture.service.isRemembered("s1", "bash:general:python -V"), false);
});

test("filesystem requests cannot inject Bash remember keys", async () => {
    const fixture = await createService();
    const pending = fixture.service.requestApproval({
        sessionId: "s1",
        path: join(fixture.external, "file.txt"),
        rememberKey: "bash:general:*",
    });
    const request = await waitForPermissionRequest(fixture.events);
    await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "allow_always" });
    await pending;

    assert.equal(fixture.service.isRemembered("s1", "bash:general:*"), false);
});

test("file read service returns content, permission denial, and distinct I/O errors", async () => {
    const fixture = await createService();
    const allowedFile = join(fixture.docs, "ok.txt");
    const deniedFile = join(fixture.external, "deny.txt");
    const missingFile = join(fixture.external, "missing.txt");
    await writeText(allowedFile, "hello");
    await writeText(deniedFile, "secret");
    fixture.state.permissionRuntimeAllowlist.set("s1", new Set([fixture.external]));
    const fileReadService = createFileReadService({ permissionService: fixture.service, cwd: fixture.agentRoot });

    assert.deepEqual(await fileReadService.readTextFile({ sessionId: "s1", path: allowedFile }), { result: { content: "hello" } });
    assert.equal((await fileReadService.readTextFile({ sessionId: "s1", path: missingFile })).error.message, "file_io_error");

    fixture.state.permissionRuntimeAllowlist.clear();
    const denied = fileReadService.readTextFile({ sessionId: "s1", path: deniedFile });
    await allowRequest(fixture, "deny");
    assert.equal((await denied).error.message, "permission_denied");
});
