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

export const createContextController = ({ state }) => ({
    async update(_req, res, body) {
        if (!body || typeof body !== "object" || Array.isArray(body)) {
            return json(res, { error: "context must be an object" }, 400);
        }
        if (Object.hasOwn(body, "projectRoot")) {
            return json(res, { error: "projectRoot is host-owned" }, 400);
        }
        state.activeContext = {
            profileId: body.profileId,
            activeModule: body.activeModule,
        };
        return json(res, { ok: true, activeContext: state.activeContext });
    },
});
