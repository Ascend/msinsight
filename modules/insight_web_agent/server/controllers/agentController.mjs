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
import { isBusy } from "../services/agentConfigService.mjs";

export const createAgentController = ({ agentService, state }) => ({
    list(_req, res) {
        return json(res, agentService.list());
    },

    async switch(_req, res, body) {
        console.log(`Switch agent requested: ${String(body?.name ?? "")}`);
        if (isBusy(state)) return json(res, { error: "agent_busy", message: "Agent is busy" }, 409);
        const result = await agentService.switchAgent(body?.name);
        if (result.error) console.warn(`Switch agent failed: ${result.error}`);
        else console.log(`Switch agent completed: ${result.activeAgentName}`);
        return json(res, result, result.status ?? 200);
    },

    async refresh(_req, res) {
        console.log("Refreshing agent servers");
        const result = await agentService.refreshAgents();
        if (result.error) console.warn(`Refresh agents failed: ${result.error}`);
        return json(res, result, result.status ?? 200);
    },
});
