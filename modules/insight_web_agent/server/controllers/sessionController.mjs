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

export const createSessionController = ({ sessionService }) => ({
    async create(_req, res) {
        const result = await sessionService.createEmptySession();
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async list(_req, res) {
        return json(res, { sessions: await sessionService.listSessions() });
    },

    async load(_req, res, body) {
        const result = await sessionService.loadSessionById(body?.sessionId);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async delete(_req, res, body) {
        const result = await sessionService.deleteSessionById(body?.sessionId);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async setModel(_req, res, body) {
        const result = await sessionService.setModel(body?.model, body?.sessionId);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async setMode(_req, res, body) {
        const result = await sessionService.setMode(body?.mode, body?.sessionId);
        return json(res, normalizeBody(result), result.status ?? 200);
    },
});

const normalizeBody = ({ status, ...body }) => body;
