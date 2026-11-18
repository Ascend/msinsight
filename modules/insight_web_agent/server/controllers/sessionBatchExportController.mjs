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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { json } from "../http/response.mjs";
import { errorBody } from "../http/errors.mjs";

export const createSessionBatchExportController = ({ sessionBatchExportService }) => {
    const respond = async (res, operation, accepted = false) => {
        try {
            return json(res, await operation(), accepted ? 202 : 200);
        } catch (error) {
            const status = Number.isInteger(error?.status) && error.status >= 400 && error.status <= 599
                ? error.status : 500;
            return json(res, errorBody({
                code: typeof error?.code === "string" ? error.code : status === 400 ? "invalid_session_export_request" : "session_batch_export_failed",
                message: error instanceof Error ? error.message : "Session export failed",
            }), status);
        }
    };
    return {
        query: (_req, res, body) => respond(res, () => sessionBatchExportService.startQuery(body), true),
        status: (_req, res, jobId) => respond(res, () => sessionBatchExportService.getStatus(jobId)),
        export: (_req, res, body) => respond(res, () => sessionBatchExportService.startExport(body), true),
    };
};
