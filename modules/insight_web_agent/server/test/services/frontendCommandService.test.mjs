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
import { createCapabilityCenter } from "../../capability-center/service.mjs";

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

for (const claimed of [false, true]) {
    test(`cancellation and timeout preserve claimed=${claimed} uncertainty`, async () => {
        for (const reason of ["user_cancelled", "session_cancelled", "service_disposed", "deadline_exceeded"]) {
            const bus = eventBusStub();
            const service = createFrontendCommandService({ eventBus: bus });
            const request = service.request({ requestId: "r", command: "observe", args: {} });
            const rejection = assert.rejects(request, (error) => {
                assert.equal(error.retryable, reason === "deadline_exceeded" && !claimed);
                assert.equal(error.details.outcome, claimed ? "unknown" : "not_started");
                assert.equal(error.details.reason, reason);
                assert.equal(JSON.stringify(error).includes("claimToken"), false);
                return true;
            });
            if (claimed) service.claim("r");
            service.cancel("r", reason);
            await rejection;
            service.dispose();
        }
    });
}

test("actual deadline distinguishes pending from claimed and never permits replay of claimed work", async () => {
    for (const claimed of [false, true]) {
        const service = createFrontendCommandService({ eventBus: eventBusStub() });
        const request = service.request({ requestId: "deadline", command: "observe", args: {}, deadline: Date.now() + 10 });
        if (claimed) service.claim("deadline");
        await assert.rejects(request, (error) => error.code === "COMMAND_TIMEOUT"
            && error.retryable === !claimed && error.details.outcome === (claimed ? "unknown" : "not_started"));
        assert.equal(service.claim("deadline").claimed, false);
        service.dispose();
    }
});

const receiptCases = [
    ["COMMAND_INVALID", undefined, "validate", "not_started", false, "correct_input"],
    ["COMMAND_NOT_FOUND", undefined, "validate", "not_started", false, "correct_input"],
    ["COMMAND_BUSY", undefined, "validate", "not_started", true, "retry_after_ready"],
    ["COMMAND_PERMISSION_DENIED", undefined, "authorize", "not_started", false, "do_not_retry"],
    ["COMMAND_APPROVAL_REQUIRED", undefined, "authorize", "not_started", false, "do_not_retry"],
    ...["module_not_active", "module_not_registered", "command_not_registered"].map((reason) =>
        ["COMMAND_UNAVAILABLE", reason, "validate", "not_started", true, "refresh_context"]),
    ["TABLE_BUSY", undefined, "validate", "not_started", true, "retry_after_ready"],
    ["TABLE_STATE_STALE", undefined, "validate", "not_started", true, "refresh_context"],
    ...["COMMAND_TIMEOUT", "COMMAND_CONNECTION_LOST", "TABLE_COMMAND_TIMEOUT", "TABLE_CONNECTION_LOST"].map((code) =>
        [code, undefined, "execute", "unknown", false, "inspect_before_retry"]),
    ["COMMAND_RESULT_INVALID", "result_not_serializable", "respond", "completed", false, "inspect_before_retry"],
    ["COMMAND_EXECUTION_FAILED", undefined, "execute", "failed", false, "inspect_before_retry"],
    ["COMMAND_UNAVAILABLE", "unknown_reason", "execute", "failed", false, "inspect_before_retry"],
];

for (const [code, reason, stage, outcome, retryable, retryAdvice] of receiptCases) {
    test(`real Broker receipt classification: ${code}/${reason ?? "none"}`, async (t) => {
        for (const requestId of ["00000000-0000-4000-8000-000000000001", "mcp:fixture:request"]) {
            const bus = eventBusStub();
            const broker = createFrontendCommandService({ eventBus: bus });
            t.after(() => broker.dispose());
            let brokerError;
            const center = createCapabilityCenter({
                frontendCommandService: {
                    ...broker,
                    request: (request) => broker.request(request).catch((error) => { brokerError = error; throw error; }),
                },
                logger: { warn() {} },
            });
            const pending = center.invoke({
                invocationId: requestId, name: "msinsight", input: { command: "observe", args: {} },
            }).catch((error) => error);
            const claim = broker.claim(requestId);
            const receipt = {
                requestId, claimToken: claim.claimToken, status: "failed",
                error: {
                    code, message: "Fixture command failure.", retryable: true,
                    details: { reason, outcome: "not_started", stage: "authorize", claimToken: "must-not-leak" },
                },
            };
            assert.equal(broker.respond({ ...receipt, claimToken: "wrong-owner" }).settled, false);
            assert.equal(broker.respond(receipt).settled, true);
            const error = await pending;
            assert.equal(error.code, code);
            assert.equal(error.details.stage, stage);
            assert.equal(error.details.outcome, outcome);
            assert.equal(error.retryable, retryable);
            assert.equal(error.details.retryAdvice, retryAdvice);
            assert.equal(error.details.diagnosticId, brokerError.details.diagnosticId);
            assert.doesNotMatch(JSON.stringify(error), /must-not-leak/);
            assert.equal(broker.claim(requestId).claimed, false);
        }
    });
}
