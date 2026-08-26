/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { timingSafeEqual } from "node:crypto";

export const hasValidCapability = (req, expectedToken) => {
    const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);
    return tokensEqual(expectedToken, url.searchParams.get("capabilityToken"));
};

export const tokensEqual = (expectedToken, suppliedToken) => {
    const expected = Buffer.from(String(expectedToken ?? ""));
    const supplied = Buffer.from(String(suppliedToken ?? ""));
    return expected.length > 0 && expected.length === supplied.length && timingSafeEqual(expected, supplied);
};

export const normalizeRequestOrigin = (req) => String(req.headers.origin ?? "").replace(/\/$/, "");
