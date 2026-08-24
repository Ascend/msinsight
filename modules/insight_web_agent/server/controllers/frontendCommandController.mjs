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
            const code = String(error.code ?? "command_execution_failed").toLowerCase();
            return json(res, {
                error: code,
                message: error.message || "The frontend command could not be completed",
                details: {
                    retryable: Boolean(error.retryable),
                    commandDetails: error.details,
                    state: error.state,
                },
            }, commandErrorStatus(error.code));
        }
    },

    claim(_req, res, body) {
        if (!body?.requestId) {
            return json(res, {
                error: "request_id_required",
                message: "requestId is required to claim a frontend command",
            }, 400);
        }
        return json(res, frontendCommandService.claim(body.requestId));
    },

    respond(_req, res, body) {
        if (!body?.requestId || !body?.claimToken || !["completed", "failed", "cancelled"].includes(body?.status)) {
            return json(res, {
                error: "invalid_command_response",
                message: "requestId, claimToken, and status must be provided; status must be completed, failed, or cancelled",
            }, 400);
        }
        return json(res, frontendCommandService.respond(body));
    },

    cancel(_req, res, body) {
        if (!body?.requestId) {
            return json(res, {
                error: "request_id_required",
                message: "requestId is required to cancel a frontend command",
            }, 400);
        }
        frontendCommandService.cancel(body.requestId, body.reason);
        return json(res, { ok: true });
    },
});

const commandErrorStatus = (code) => {
    if (code === "COMMAND_INVALID") {
        return 400;
    }
    if (code === "COMMAND_TIMEOUT") {
        return 408;
    }
    return 409;
};
