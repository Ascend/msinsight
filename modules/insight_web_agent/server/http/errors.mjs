/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export class HttpError extends Error {
    constructor(status, code, message, details) {
        super(message);
        this.name = "HttpError";
        this.status = status;
        this.code = code;
        this.details = details;
    }
}

export const errorBody = ({ code, message, details }) => ({
    error: code,
    code,
    message,
    ...(details === undefined ? {} : { details }),
});

export const normalizeHttpError = (error) => {
    if (error instanceof HttpError) {
        return {
            status: error.status,
            body: errorBody({ code: error.code, message: error.message, details: error.details }),
        };
    }
    if (error instanceof SyntaxError) {
        return {
            status: 400,
            body: errorBody({
                code: "invalid_json",
                message: "Request body must be valid JSON",
                details: error.message,
            }),
        };
    }
    return {
        status: 500,
        body: errorBody({
            code: "internal_error",
            message: "The backend failed to process the request",
            details: error instanceof Error ? error.message : String(error),
        }),
    };
};
