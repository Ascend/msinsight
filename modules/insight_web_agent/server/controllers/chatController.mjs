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
        return json(res, publicState(state));
    },

    async prompt(_req, res, body) {
        const result = await chatService.prompt(body?.text, {
            newSession: Boolean(body?.newSession),
            sessionId: body?.sessionId,
            images: Array.isArray(body?.images) ? body.images : [],
            mode: body?.mode,
            hiddenContext: body?.hiddenContext,
        });
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async cancel(_req, res, body) {
        return json(res, await chatService.cancel(body?.sessionId));
    },
});

const normalizeBody = ({ status, ...body }) => body;
