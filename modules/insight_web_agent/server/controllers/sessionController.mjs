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
        console.log("Create session requested");
        const result = await sessionService.createEmptySession();
        if (result.error) console.warn(`Create session failed: ${result.error}`);
        else console.log(`Create session completed: sessionId=${result.sessionId}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async list(_req, res) {
        console.log("List sessions requested");
        return json(res, { sessions: await sessionService.listSessions() });
    },

    async load(_req, res, body) {
        console.log(`Load session requested: sessionId=${String(body?.sessionId ?? "")}`);
        const result = await sessionService.loadSessionById(body?.sessionId);
        if (result.error) console.warn(`Load session failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        else console.log(`Load session completed: sessionId=${result.sessionId}, messages=${result.messages.length}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async delete(_req, res, body) {
        console.log(`Delete session requested: sessionId=${String(body?.sessionId ?? "")}`);
        const result = await sessionService.deleteSessionById(body?.sessionId);
        if (result.error) console.warn(`Delete session failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        else console.log(`Delete session completed: sessionId=${String(body?.sessionId ?? "")}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async setConfigOption(_req, res, body) {
        console.log(`Set config option requested: sessionId=${String(body?.sessionId ?? "")}, configId=${String(body?.configId ?? "")}`);
        const result = await sessionService.setConfigOption(body?.configId, body?.value, body?.sessionId);
        if (result.error) console.warn(`Set config option failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async setModel(_req, res, body) {
        console.log(`Set model requested: sessionId=${String(body?.sessionId ?? "")}, model=${String(body?.model ?? "")}`);
        const result = await sessionService.setModel(body?.model, body?.sessionId);
        if (result.error) console.warn(`Set model failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async setMode(_req, res, body) {
        console.log(`Set mode requested: sessionId=${String(body?.sessionId ?? "")}, mode=${String(body?.mode ?? "")}`);
        const result = await sessionService.setMode(body?.mode, body?.sessionId);
        if (result.error) console.warn(`Set mode failed: sessionId=${String(body?.sessionId ?? "")}, error=${result.error}`);
        return json(res, normalizeBody(result), result.status ?? 200);
    },
});

const normalizeBody = ({ status, ...body }) => body;
