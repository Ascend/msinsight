/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { json } from "../http/response.mjs";
import { tokensEqual } from "../http/security.mjs";

// Native Agent 使用独立进程级 Token 进入同一能力中心，不额外启动 MCP Client。
export const createCapabilityController = ({ capabilityCenter, accessToken }) => ({
    async invoke(req, res, body) {
        const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);
        if (!tokensEqual(accessToken, url.searchParams.get("capabilityToken"))) {
            return json(res, { error: "Unauthorized" }, 401);
        }
        // Native fetch 中断或连接提前关闭时，取消仍在执行的 Capability/Frontend Command。
        const controller = new AbortController();
        const abort = () => controller.abort();
        req.once("aborted", abort);
        res.once("close", abort);
        try {
            const result = await capabilityCenter.invoke({ ...body, signal: controller.signal });
            return json(res, { ok: true, result });
        } catch (error) {
            if (!res.destroyed) return capabilityErrorResponse(res, error);
        } finally {
            req.off("aborted", abort);
            res.off("close", abort);
        }
    },
});

const capabilityErrorResponse = (res, error) => json(res, {
    error: {
        code: error.code ?? "CAPABILITY_EXECUTION_FAILED",
        message: error.message,
        retryable: Boolean(error.retryable),
        details: error.details,
    },
}, capabilityErrorStatus(error.code));

const capabilityErrorStatus = (code) => {
    if (code === "CAPABILITY_NOT_FOUND") return 404;
    if (code === "CAPABILITY_INVALID_ARGUMENT") return 400;
    if (code === "COMMAND_TIMEOUT" || code === "CLI_TIMEOUT") return 408;
    if (code === "CLI_START_FAILED" || code === "CLI_EXIT_NONZERO" || code === "CLI_OUTPUT_LIMIT") return 422;
    return 409;
};
