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
    const connectListeners = new Set();
    const send = (client, event) => client.write(`data: ${JSON.stringify(event)}\n\n`);
    const broadcast = (event) => {
        for (const client of state.clients) send(client, event);
    };

    const connect = (req, res) => {
        res.writeHead(200, {
            "content-type": "text/event-stream",
            "cache-control": "no-cache",
            connection: "keep-alive",
            "x-accel-buffering": "no",
        });
        res.flushHeaders?.();
        state.clients.add(res);
        send(res, { type: "state", state: publicState(state) });
        connectListeners.forEach((listener) => listener((event) => send(res, event)));
        req.on("close", () => state.clients.delete(res));
    };

    const onConnect = (listener) => {
        connectListeners.add(listener);
        return () => connectListeners.delete(listener);
    };

    const close = () => {
        for (const client of state.clients) client.end();
        state.clients.clear();
        connectListeners.clear();
    };

    return { broadcast, connect, onConnect, close };
};
