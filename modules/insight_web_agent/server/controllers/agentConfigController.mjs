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

export const createAgentConfigController = ({ agentConfigService }) => ({
    async get(_req, res) {
        return json(res, { snapshot: await agentConfigService.readSnapshot() });
    },

    async saveAgentServers(_req, res, body) {
        const result = await agentConfigService.saveAgentServers(body);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async saveBuiltinAgent(_req, res, body) {
        const result = await agentConfigService.saveBuiltinAgent(body);
        return json(res, normalizeBody(result), result.status ?? 200);
    },

    async saveSessionConfig(_req, res, body) {
        const result = await agentConfigService.saveSessionConfig(body);
        return json(res, normalizeBody(result), result.status ?? 200);
    },
});

const normalizeBody = ({ status, ...body }) => body;
