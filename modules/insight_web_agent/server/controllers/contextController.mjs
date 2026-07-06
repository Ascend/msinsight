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

export const createContextController = ({ state, sessionManager }) => ({
    async update(_req, res, body) {
        const ctx = body ?? {};
        const activeContext = {
            profileId: ctx.profileId,
            activeModule: ctx.activeModule,
            selection: ctx.selection,
            projectRoot: ctx.projectRoot,
        };
        if (sessionManager?.updateContext) {
            await sessionManager.updateContext(activeContext);
        } else {
            state.activeContext = activeContext;
        }
        return json(res, { ok: true, activeContext: state.activeContext });
    },
});
