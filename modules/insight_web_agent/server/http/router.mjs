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
import { json, readJson } from "./response.mjs";

export const createRouter = ({ agentController, chatController, sessionController, eventController }) => {
    return async (req, res) => {
        const url = new URL(req.url ?? "/", `http://${req.headers.host}`);

        if (req.method === "GET" && url.pathname === "/api/state") {
            return chatController.getState(req, res);
        }

        if (req.method === "GET" && url.pathname === "/api/sessions") {
            return sessionController.list(req, res);
        }

        if (req.method === "POST" && url.pathname === "/api/sessions/new") {
            return sessionController.create(req, res);
        }

        if (req.method === "GET" && url.pathname === "/api/events") {
            return eventController.connect(req, res);
        }

        if (req.method === "GET" && url.pathname === "/api/agents") {
            return agentController.list(req, res);
        }

        if (req.method === "POST" && url.pathname === "/api/agents/switch") {
            return agentController.switch(req, res, await readJson(req));
        }

        if (req.method === "POST" && url.pathname === "/api/prompt") {
            return chatController.prompt(req, res, await readJson(req));
        }

        if (req.method === "POST" && url.pathname === "/api/sessions/load") {
            return sessionController.load(req, res, await readJson(req));
        }

        if (req.method === "POST" && url.pathname === "/api/sessions/delete") {
            return sessionController.delete(req, res, await readJson(req));
        }

        if (req.method === "POST" && url.pathname === "/api/session-config/model") {
            return sessionController.setModel(req, res, await readJson(req));
        }

        if (req.method === "POST" && url.pathname === "/api/session-config/mode") {
            return sessionController.setMode(req, res, await readJson(req));
        }

        if (req.method === "POST" && url.pathname === "/api/cancel") {
            return chatController.cancel(req, res, await readJson(req));
        }

        return json(res, { error: "Not found" }, 404);
    };
};
