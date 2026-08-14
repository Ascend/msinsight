/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { json } from "../http/response.mjs";

export const createFrontendCommandController = ({ frontendCommandService }) => ({
    async request(_req, res, body) {
        try {
            const result = await frontendCommandService.request({
                requestId: body?.requestId,
                sessionId: body?.sessionId,
                command: body?.command,
                args: body?.args ?? {},
                deadline: body?.deadline,
            });
            return json(res, { ok: true, result });
        } catch (error) {
            return json(res, {
                error: {
                    code: error.code ?? "COMMAND_EXECUTION_FAILED",
                    message: error.message,
                    retryable: Boolean(error.retryable),
                    details: error.details,
                    state: error.state,
                },
            }, error.code === "COMMAND_TIMEOUT" ? 408 : 409);
        }
    },

    claim(_req, res, body) {
        if (!body?.requestId) return json(res, { error: "requestId is required" }, 400);
        return json(res, frontendCommandService.claim(body.requestId));
    },

    respond(_req, res, body) {
        if (!body?.requestId || !body?.claimToken || !["completed", "failed", "cancelled"].includes(body?.status)) {
            return json(res, { error: "requestId, claimToken, and a valid status are required" }, 400);
        }
        return json(res, frontendCommandService.respond(body));
    },

    cancel(_req, res, body) {
        if (!body?.requestId) return json(res, { error: "requestId is required" }, 400);
        frontendCommandService.cancel(body.requestId, body.reason);
        return json(res, { ok: true });
    },
});
