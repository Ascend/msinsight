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
import { createFileReadService } from "../../services/fileReadService.mjs";

const createFixture = async () => {
    const root = await mkdtemp(join(tmpdir(), "insight-permission-"));
    const docs = join(root, "docs");
    const cwd = join(root, "agent-workspace");
    const agentRoot = join(cwd, "opencode");
    const external = join(root, "external");
    await Promise.all([mkdir(docs, { recursive: true }), mkdir(agentRoot, { recursive: true }), mkdir(external, { recursive: true })]);
    return { root, docs, cwd, agentRoot, external };
};

const nextTick = () => new Promise((resolve) => setImmediate(resolve));
const writeText = (path, text) => writeFile(path, text, "utf8");
const permissionRequests = (events) => events.filter((event) => event.type === "permission_request");
const expectNoPrompt = (fixture) => assert.deepEqual(fixture.events, []);
const evaluatePath = (fixture, path) => fixture.service.evaluate({ sessionId: "s1", path });
const realExternalPath = async (fixture) => realpath(fixture.external);
const expectSettlesSoon = (promise) => Promise.race([
    promise.then(() => "resolved", (error) => `rejected:${error.message}`),
    new Promise((resolve) => setTimeout(() => resolve("pending"), 100)),
]);
const settleOutcome = (promise) => promise.then(() => "resolved", (error) => `rejected:${error.message}`);
const waitForPermissionRequest = async (events) => {
    for (let index = 0; index < 20; index += 1) {
        const request = events.findLast((event) => event.type === "permission_request");
        if (request) return request;
        await nextTick();
    }
    return undefined;
};

const waitForRequestCount = async (events, count) => {
    while (permissionRequests(events).length < count) {
        await nextTick();
    }
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
        timeoutMs: options.timeoutMs,
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

test("concurrent requests for the same path are independent by requestId", async () => {
    const fixture = await createService({ timeoutMs: 1000 });
    const target = join(fixture.external, "same.txt");
    await writeText(target, "content");

    const first = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    const second = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    await waitForRequestCount(fixture.events, 2);
    const requests = permissionRequests(fixture.events);

    assert.equal(requests.length, 2);
    assert.notEqual(requests[0].requestId, requests[1].requestId);
    await fixture.service.respond({ sessionId: "s1", requestId: requests[0].requestId, decision: "allow_once" });
    const firstOutcome = await expectSettlesSoon(first);
    const secondOutcome = await expectSettlesSoon(second);
    assert.deepEqual([firstOutcome, secondOutcome].sort(), ["pending", "resolved"]);
    assert.equal(fixture.state.pendingPermissions.size, 1);

    await fixture.service.respond({ sessionId: "s1", requestId: requests[1].requestId, decision: "deny" });
    const finalOutcomes = [await settleOutcome(first), await settleOutcome(second)].sort();
    assert.deepEqual(finalOutcomes, ["rejected:denied", "resolved"]);
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

    assert.equal((await fixture.service.respond({ sessionId: "s1", requestId: "missing", decision: "allow_once" })).status, 404);
    assert.equal((await fixture.service.respond({ sessionId: "s1", requestId: "", decision: "nope" })).status, 400);

    const pending = fixture.service.ensureReadAllowed({ sessionId: "s1", path: target });
    const request = await allowRequest(fixture, "deny");
    await assert.rejects(pending, /denied/);
    assert.equal((await fixture.service.respond({ sessionId: "s1", requestId: request.requestId, decision: "deny" })).status, 409);
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
