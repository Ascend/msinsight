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
import { createServer } from "node:http";
import { createAgentController } from "./controllers/agentController.mjs";
import { createChatController } from "./controllers/chatController.mjs";
import { createEventController } from "./controllers/eventController.mjs";
import { createSessionController } from "./controllers/sessionController.mjs";
import { createRouter } from "./http/router.mjs";
import { applyCors, json } from "./http/response.mjs";

export const createApp = ({ agentService, eventBus, chatService, sessionService, state }) => {
    const router = createRouter({
        agentController: createAgentController({ agentService }),
        chatController: createChatController({ chatService, state }),
        eventController: createEventController({ eventBus }),
        sessionController: createSessionController({ sessionService }),
    });

    return createServer(async (req, res) => {
        try {
            applyCors(res);
            if (req.method === "OPTIONS") {
                res.writeHead(204);
                res.end();
                return;
            }

            return await router(req, res);
        } catch (error) {
            console.error(error);
            return json(res, { error: String(error?.message ?? error) }, 500);
        }
    });
};
