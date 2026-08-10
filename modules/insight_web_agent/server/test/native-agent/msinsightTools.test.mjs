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
import { createServer } from "node:http";
import test from "node:test";
import { createMsinsightTools } from "../../native-agent/tools/msinsightTools.mjs";

test("native observation includes the host capability token", async (t) => {
    const capabilityToken = "native-observation-capability";
    const server = createServer((req, res) => {
        const url = new URL(req.url, `http://${req.headers.host}`);
        if (url.searchParams.get("capabilityToken") !== capabilityToken) {
            res.writeHead(401, { "content-type": "application/json" });
            res.end(JSON.stringify({ error: "Unauthorized" }));
            return;
        }
        res.writeHead(200, { "content-type": "application/json" });
        res.end(JSON.stringify({ observation: { route: "timeline" }, updatedAt: 42 }));
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(() => server.close());
    const { port } = server.address();
    const observe = createMsinsightTools({
        baseUrl: `http://127.0.0.1:${port}`,
        capabilityToken,
    }).find(({ name }) => name === "msinsight_observe");

    assert.deepEqual(await observe.execute(), {
        observation: { route: "timeline" },
        updatedAt: 42,
        stale: false,
        message: undefined,
    });
});
