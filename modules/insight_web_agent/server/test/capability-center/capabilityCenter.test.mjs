/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { resolve } from "node:path";
import test from "node:test";
import { registerConfiguredCapabilities } from "../../capability-center/configuredCapabilities.mjs";
import { createCapabilityCenter } from "../../capability-center/service.mjs";

const fixture = () => {
    const requests = [];
    const frontendCommandService = {
        async request(request) {
            requests.push(request);
            return { command: request.command };
        },
    };
    const capabilityCenter = createCapabilityCenter({ frontendCommandService });
    registerConfiguredCapabilities({
        capabilityCenter,
        definitions: [{
            type: "cli",
            name: "pt_snap",
            description: "Run pt-snap.",
            executable: resolve("pt-snap"),
        }],
        cwd: resolve("workspace"),
    });
    return { capabilityCenter, requests };
};

test("capability center lists registered tools and routes a global invocation", async () => {
    const { capabilityCenter, requests } = fixture();

    const capabilities = capabilityCenter.list();
    const result = await capabilityCenter.invoke({
        invocationId: "invocation-1",
        name: "msinsight",
        input: { command: "observe", args: {} },
    });

    assert.deepEqual(capabilities.map(({ name }) => name), ["msinsight", "rag_retrieve", "pt_snap"]);
    assert.deepEqual(result, { command: "observe" });
    assert.equal(requests[0].requestId, "invocation-1");
    assert.equal(requests[0].sessionId, "");
});

test("capability center exposes an unavailable RAG tool when the runtime is not loaded", async () => {
    const { capabilityCenter } = fixture();

    const result = await capabilityCenter.invoke({
        name: "rag_retrieve",
        input: { query: "MindStudio Insight 如何导入数据" },
    });

    assert.deepEqual(result, {
        schemaVersion: "1.0",
        status: "unavailable",
        query: "MindStudio Insight 如何导入数据",
        sources: [],
        reason: "unavailable",
    });
});

test("capability registry validates msinsight arguments", async () => {
    const { capabilityCenter } = fixture();

    await assert.rejects(
        capabilityCenter.invoke({ name: "msinsight", input: { command: "" } }),
        { code: "CAPABILITY_INVALID_ARGUMENT" },
    );
    await assert.rejects(
        capabilityCenter.invoke({ name: "msinsight", input: { command: "observe", args: [] } }),
        { code: "CAPABILITY_INVALID_ARGUMENT" },
    );
});
