/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { test } from "node:test";
import { createFrontendCommandService } from "../../services/frontendCommandService.mjs";

test("request broadcasts, claims once, and resolves from a terminal receipt", async () => {
    const bus = eventBusStub();
    const service = createFrontendCommandService({ eventBus: bus, timeoutMs: 5000 });
    const pending = service.request({ sessionId: "session-1", command: "observe", args: { active: true } });
    const requestEvent = bus.events[0];

    assert.equal(requestEvent.type, "frontend_command_request");
    assert.equal(requestEvent.sessionId, "session-1");
    assert.equal(requestEvent.command, "observe");
    assert.deepEqual(requestEvent.args, { active: true });
    const claim = service.claim(requestEvent.requestId);
    assert.equal(claim.ok, true);
    assert.equal(claim.claimed, true);
    assert.equal(typeof claim.claimToken, "string");
    assert.deepEqual(service.claim(requestEvent.requestId), { ok: true, claimed: false });

    service.respond({ requestId: requestEvent.requestId, claimToken: claim.claimToken, status: "completed", result: { tables: [] } });
    assert.deepEqual(await pending, { tables: [] });
    service.dispose();
});

test("respond requires the claim token that owns the request", async () => {
    const bus = eventBusStub();
    const service = createFrontendCommandService({ eventBus: bus, timeoutMs: 5000 });
    const pending = service.request({ sessionId: "session-1", command: "observe", args: {} });
    const requestId = bus.events[0].requestId;
    const claim = service.claim(requestId);

    assert.deepEqual(service.respond({ requestId, claimToken: "stale-token", status: "completed", result: {} }), {
        ok: true,
        settled: false,
    });
    service.respond({ requestId, claimToken: claim.claimToken, status: "completed", result: { ok: true } });
    assert.deepEqual(await pending, { ok: true });
    service.dispose();
});

test("SSE reconnect replays only an unclaimed pending request", async () => {
    const bus = eventBusStub();
    const service = createFrontendCommandService({ eventBus: bus, timeoutMs: 5000 });
    const first = service.request({ sessionId: "session-1", command: "observe", args: {} });
    const firstId = bus.events[0].requestId;
    const replayed = [];

    bus.connect((event) => replayed.push(event));
    assert.deepEqual(replayed.map(({ requestId }) => requestId), [firstId]);

    const claim = service.claim(firstId);
    assert.equal(claim.claimed, true);
    replayed.length = 0;
    bus.connect((event) => replayed.push(event));
    assert.deepEqual(replayed, []);

    service.respond({ requestId: firstId, claimToken: claim.claimToken, status: "completed", result: {} });
    await first;
    service.dispose();
});

test("cancelSession rejects only requests owned by that session", async () => {
    const bus = eventBusStub();
    const service = createFrontendCommandService({ eventBus: bus, timeoutMs: 5000 });
    const first = service.request({ sessionId: "session-1", command: "MemScope.table.refresh", args: {} });
    const firstId = bus.events[0].requestId;
    const second = service.request({ sessionId: "session-2", command: "observe", args: {} });
    const secondId = bus.events[1].requestId;
    const secondClaim = service.claim(secondId);

    service.cancelSession("session-1");
    await assert.rejects(first, (error) => error.code === "COMMAND_CANCELLED");
    assert.equal(bus.events.some((event) => event.type === "frontend_command_cancel" && event.requestId === firstId), true);

    service.respond({ requestId: secondId, claimToken: secondClaim.claimToken, status: "completed", result: { ok: true } });
    assert.deepEqual(await second, { ok: true });
    service.dispose();
});

const eventBusStub = () => {
    const events = [];
    const connectListeners = new Set();
    return {
        events,
        broadcast(event) {
            events.push(event);
        },
        onConnect(listener) {
            connectListeners.add(listener);
            return () => connectListeners.delete(listener);
        },
        connect(send) {
            connectListeners.forEach((listener) => listener(send));
        },
    };
};
