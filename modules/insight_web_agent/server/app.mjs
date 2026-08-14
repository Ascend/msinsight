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
import { createAgentConfigController } from "./controllers/agentConfigController.mjs";
import { createChatController } from "./controllers/chatController.mjs";
import { createContextController } from "./controllers/contextController.mjs";
import { createEventController } from "./controllers/eventController.mjs";
import { createSessionController } from "./controllers/sessionController.mjs";
import { createPermissionController } from "./controllers/permissionController.mjs";
import { createPageController } from "./controllers/pageController.mjs";
import { createFrontendCommandController } from "./controllers/frontendCommandController.mjs";
import { createRouter } from "./http/router.mjs";
import { applyCors, json } from "./http/response.mjs";
import { hasValidCapability, normalizeRequestOrigin } from "./http/security.mjs";

export const createApp = ({ agentService, eventBus, chatService, sessionService, state, permissionService, agentConfigService, pageContextService, frontendCommandService, capabilityToken, allowedOrigins = [] }) => {
    const router = createRouter({
        agentController: createAgentController({ agentService, state }),
        agentConfigController: createAgentConfigController({ agentConfigService }),
        chatController: createChatController({ chatService, state }),
        contextController: createContextController({ state }),
        eventController: createEventController({ eventBus }),
        sessionController: createSessionController({ sessionService }),
        permissionController: createPermissionController({ permissionService }),
        pageController: createPageController({ pageContextService }),
        frontendCommandController: createFrontendCommandController({ frontendCommandService }),
    });

    return createServer(async (req, res) => {
        try {
            applyCors(req, res, allowedOrigins);
            const requestOrigin = normalizeRequestOrigin(req);
            if (requestOrigin && !allowedOrigins.includes(requestOrigin)) {
                return json(res, { error: "Origin not allowed" }, 403);
            }
            if ((req.url ?? "").startsWith("/api/") && !hasValidCapability(req, capabilityToken)) {
                return json(res, { error: "Unauthorized" }, 401);
            }
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
