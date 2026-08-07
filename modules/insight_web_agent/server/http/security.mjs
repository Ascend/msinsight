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
    if (!expectedToken) return false;
    const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);
    const suppliedToken = url.searchParams.get("capabilityToken") ?? "";
    const expected = Buffer.from(expectedToken);
    const supplied = Buffer.from(suppliedToken);
    return expected.length === supplied.length && timingSafeEqual(expected, supplied);
};

export const normalizeRequestOrigin = (req) => String(req.headers.origin ?? "").replace(/\/$/, "");
