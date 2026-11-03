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
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { once } from "node:events";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { createArtifactService } from "../../services/artifactService.mjs";
import { createCapabilityCenter } from "../../capability-center/service.mjs";
import { createCapabilityTools } from "../../native-agent/tools/capabilityTools.mjs";
import { PUBLISH_REPORT_CAPABILITY } from "../../capability-center/artifactCapability.mjs";
import { createApp } from "../../app.mjs";

const setup = async (t) => {
    const storeDir = await mkdtemp(join(tmpdir(), "insight-artifacts-"));
    t.after(() => rm(storeDir, { recursive: true, force: true }));
    return { storeDir, service: createArtifactService({ storeDir }) };
};
const input = { title: "分析报告", description: "数据快照", artifactId: null, baseVersion: null, html: '<!doctype html><h1>分析</h1><script>document.body.dataset.ready="yes"</script>' };

test("snapshots survive restart, retain history and reject concurrent stale updates", async (t) => {
    const { service, storeDir } = await setup(t);
    const first = await service.publish(input);
    assert.equal(first.version, 1);
    assert.equal(first.html, undefined, "tool result carries a reference, not HTML");
    const updates = await Promise.allSettled(["second", "third"].map((title) => service.publish({ ...input, title, artifactId: first.artifactId, baseVersion: 1 })));
    assert.equal(updates.filter((result) => result.status === "fulfilled").length, 1);
    assert.equal(updates.find((result) => result.status === "rejected").reason.status, 409);
    const restarted = createArtifactService({ storeDir });
    const original = await restarted.get(first.artifactId, 1);
    assert.equal(original.html, input.html);
    assert.equal(original.title, input.title);
    assert.deepEqual(original.versions.map(({ version }) => version), [1, 2]);
    assert.equal((await restarted.get(first.artifactId)).version, 2);
    await assert.rejects(restarted.get(first.artifactId, 3), { status: 404 });
});

test("rejects traversal, missing reports, oversized UTF-8 input and cancelled publication", async (t) => {
    const { service } = await setup(t);
    for (const id of ["../config", "..%2fconfig", "", null]) await assert.rejects(service.get(id), { status: 400 });
    await assert.rejects(service.get("00000000-0000-0000-0000-000000000000"), { status: 404 });
    for (const value of [{ ...input, html: "字".repeat(180000) }, { ...input, title: " " }, { ...input, baseVersion: 1 }, { ...input, extra: true }]) {
        await assert.rejects(service.publish(value), { status: 400 });
    }
    const controller = new AbortController();
    controller.abort();
    await assert.rejects(service.publish(input, { signal: controller.signal }), { name: "AbortError" });
    assert.equal((await service.publish(input)).version, 1);
    assert.equal((await service.publish({ ...input, description: null })).version, 1);
});

test("native tool publishes through the capability center and authenticated API returns inert JSON", async (t) => {
    const { service } = await setup(t);
    const capabilityCenter = createCapabilityCenter({ artifactService: service });
    const server = createApp({ artifactService: service, capabilityCenter, nativeCapabilityToken: "test-native", capabilityToken: "test-ui" });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(() => new Promise((resolve) => { server.close(resolve); server.closeAllConnections(); }));
    const baseUrl = `http://127.0.0.1:${server.address().port}`;
    const [tool] = createCapabilityTools({ definitions: [{ ...PUBLISH_REPORT_CAPABILITY, requiresApproval: false }], baseUrl, capabilityToken: "test-native" });
    const report = await tool.execute(input, { sessionId: "test-session" });
    const url = `${baseUrl}/api/artifacts/${report.artifactId}?version=1`;
    assert.equal((await fetch(url)).status, 401);
    assert.equal((await fetch(url, { headers: { Authorization: "Bearer test-native" } })).status, 401);
    const response = await fetch(url, { headers: { Authorization: "Bearer test-ui" } });
    assert.equal(response.status, 200);
    assert.match(response.headers.get("content-type"), /application\/json/);
    assert.equal(response.headers.get("cache-control"), "no-store");
    assert.equal((await response.json()).html, input.html);
});
