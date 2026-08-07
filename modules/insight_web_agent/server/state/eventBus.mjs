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
import { publicState } from "./runtimeState.mjs";

export const createEventBus = (state) => {
    const broadcast = (event) => {
        const data = `data: ${JSON.stringify(event)}\n\n`;
        for (const client of state.clients) client.write(data);
    };

    const connect = (req, res) => {
        res.writeHead(200, {
            "content-type": "text/event-stream",
            "cache-control": "no-cache",
            connection: "keep-alive",
            "access-control-allow-origin": "*",
            "x-accel-buffering": "no",
        });
        res.flushHeaders?.();
        state.clients.add(res);
        res.write(`data: ${JSON.stringify({ type: "state", state: publicState(state) })}\n\n`);
        req.on("close", () => state.clients.delete(res));
    };

    return { broadcast, connect };
};
