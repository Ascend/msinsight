/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { timingSafeEqual } from "node:crypto";

export const readCapabilityToken = (req) => {
    const header = String(req.headers.authorization ?? "");
    const bearer = /^Bearer\s+(\S+)/i.exec(header);
    return bearer?.[1] ?? null;
};

export const hasValidCapability = (req, expectedToken) => tokensEqual(expectedToken, readCapabilityToken(req));

export const tokensEqual = (expectedToken, suppliedToken) => {
    const expected = Buffer.from(String(expectedToken ?? ""));
    const supplied = Buffer.from(String(suppliedToken ?? ""));
    return expected.length > 0 && expected.length === supplied.length && timingSafeEqual(expected, supplied);
};

export const normalizeRequestOrigin = (req) => String(req.headers.origin ?? "").replace(/\/$/, "");
