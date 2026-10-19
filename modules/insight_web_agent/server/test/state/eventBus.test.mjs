/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { EventEmitter } from "node:events";
import test from "node:test";
import { createEventBus } from "../../state/eventBus.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const response = () => Object.assign(new EventEmitter(), {
    frames: [],
    writeHead() {},
    flushHeaders() {},
    write(frame) { this.frames.push(frame); },
    end() { this.writableEnded = true; this.emit("close"); },
});

test("idle SSE connections receive comment heartbeats until their response closes", (t) => {
    t.mock.timers.enable({ apis: ["setInterval"] });
    const state = createRuntimeState();
    const bus = createEventBus(state);
    t.after(() => bus.close());
    const req = new EventEmitter();
    const first = response();
    const second = response();
    bus.connect(req, first);
    bus.connect(new EventEmitter(), second);
    // Request completion does not mean the streaming response has closed.
    req.emit("close");
    t.mock.timers.tick(1000);
    assert.equal(first.frames.at(-1), ": heartbeat\n\n");
    assert.equal(second.frames.at(-1), ": heartbeat\n\n");
    assert.equal(state.clients.size, 2);

    first.end();
    const frameCount = first.frames.length;
    t.mock.timers.tick(1000);
    assert.equal(first.frames.length, frameCount);
    assert.equal(state.clients.size, 1);
    assert.equal(second.frames.filter((frame) => frame.startsWith(":")).length, 2);
    second.end();
    assert.equal(state.clients.size, 0);

    const reconnected = response();
    bus.connect(new EventEmitter(), reconnected);
    t.mock.timers.tick(1000);
    assert.equal(reconnected.frames.at(-1), ": heartbeat\n\n");
    bus.close();
    const finalCount = reconnected.frames.length;
    t.mock.timers.tick(5000);
    assert.equal(reconnected.frames.length, finalCount);
    assert.equal(state.clients.size, 0);
});

test("SSE preserves fast failure and success events alongside its heartbeat", (t) => {
    t.mock.timers.enable({ apis: ["setInterval"] });
    const bus = createEventBus(createRuntimeState());
    t.after(() => bus.close());
    const client = response();
    bus.connect(new EventEmitter(), client);
    const events = [
        { type: "message_content_added", sessionId: "failed", block: { type: "text", text: "Missing API key" } },
        { type: "prompt_status", sessionId: "failed", pendingPrompt: false, completionStatus: "failed" },
        { type: "prompt_status", sessionId: "completed", pendingPrompt: false, completionStatus: "completed" },
    ];
    events.forEach(bus.broadcast);
    t.mock.timers.tick(1000);
    assert.deepEqual(client.frames.slice(1, -1).map((frame) => JSON.parse(frame.slice(6))), events);
    assert.equal(client.frames.at(-1), ": heartbeat\n\n");
});
