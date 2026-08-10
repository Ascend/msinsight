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
import { json } from "../http/response.mjs";
import { publicState } from "../state/runtimeState.mjs";

export const createChatController = ({ chatService, state }) => ({
    getState(_req, res) {
        console.log("Returning public state");
        return json(res, publicState(state));
    },

    async prompt(_req, res, body) {
        console.log(`Prompt requested: sessionId=${String(body?.sessionId ?? "")}, newSession=${Boolean(body?.newSession)}, images=${Array.isArray(body?.images) ? body.images.length : 0}`);
        const result = await chatService.prompt(body?.text, {
            newSession: Boolean(body?.newSession),
            sessionId: body?.sessionId,
            images: Array.isArray(body?.images) ? body.images : [],
            mode: body?.mode,
        });
        if (result.error) console.warn(`Prompt request failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        else console.log(`Prompt request accepted: sessionId=${result.sessionId}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async cancel(_req, res, body) {
        console.log(`Cancel prompt requested: sessionId=${String(body?.sessionId ?? "")}`);
        const result = await chatService.cancel(body?.sessionId);
        if (result.error) console.warn(`Cancel prompt failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        return json(res, result);
    },
});

const normalizeBody = ({ status, ...body }) => body;
