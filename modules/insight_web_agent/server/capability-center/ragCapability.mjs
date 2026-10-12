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
import { RAG_RETRIEVE_CAPABILITY } from "./definitions.mjs";
import { capabilityError } from "./registry.mjs";

const MAX_QUERY_CHARS = 8000;

export const createRagCapability = ({ ragService } = {}) => ({
    ...RAG_RETRIEVE_CAPABILITY,
    validate(input) {
        if (!input || typeof input !== "object" || Array.isArray(input)) {
            throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "input must be an object.");
        }
        const unknownField = Object.keys(input).find((field) => field !== "query");
        if (unknownField) {
            throw capabilityError("CAPABILITY_INVALID_ARGUMENT", `Unknown input field '${unknownField}'.`);
        }
        const query = typeof input.query === "string" ? input.query.trim() : "";
        if (!query) throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "query is required.");
        if (query.length > MAX_QUERY_CHARS) {
            throw capabilityError("CAPABILITY_INVALID_ARGUMENT", `query must not exceed ${MAX_QUERY_CHARS} characters.`);
        }
    },
    async execute(input, context) {
        const query = input.query.trim();
        throwIfAborted(context.signal);
        const status = ragService?.getStatus?.() ?? { enabled: false, reason: "unavailable" };
        if (!ragService?.isEnabled?.()) return unavailableResult(query, status.reason);
        try {
            const result = await ragService.retrieve(query, { signal: context.signal, propagateBusy: true });
            throwIfAborted(context.signal);
            if (!result) return unavailableResult(query, "retrieval_failed");
            return projectResult(query, result);
        } catch (error) {
            if (context.signal?.aborted || error?.name === "AbortError") throw error;
            if (error?.code === "rag_busy" || error?.code === "RAG_BUSY") {
                throw capabilityError("RAG_BUSY", "RAG retrieval is busy.", true);
            }
            throw capabilityError("RAG_RETRIEVAL_FAILED", "RAG retrieval failed.", Boolean(error?.retryable));
        }
    },
});

const projectResult = (query, result) => {
    const sources = Array.isArray(result.retrievedChunks)
        ? result.retrievedChunks.map(projectSource).filter(Boolean)
        : [];
    const knowledgeBase = knowledgeBaseFrom(result);
    return {
        schemaVersion: "1.0",
        status: result.status === "ok" && sources.length ? "ok" : "no_match",
        query,
        ...(knowledgeBase ? { knowledgeBase } : {}),
        sources: result.status === "ok" ? sources : [],
    };
};

const projectSource = (source) => {
    if (!source || typeof source !== "object" || Array.isArray(source)) return undefined;
    return {
        sourceLabel: String(source.sourceLabel ?? ""),
        projectId: String(source.projectId ?? ""),
        documentCategory: String(source.documentCategory ?? ""),
        title: String(source.title ?? ""),
        section: String(source.section ?? ""),
        contentFormat: String(source.contentFormat ?? "text"),
        textSummary: String(source.textSummary ?? ""),
        ...(source.answerStatus ? { answerStatus: String(source.answerStatus) } : {}),
        knowledgeText: String(source.knowledgeText ?? ""),
    };
};

const knowledgeBaseFrom = (result) => {
    const id = String(result.kbId ?? "");
    const version = String(result.kbVersion ?? "");
    return id || version ? { id, version } : undefined;
};

const unavailableResult = (query, reason) => ({
    schemaVersion: "1.0",
    status: "unavailable",
    query,
    sources: [],
    reason: stableReason(reason),
});

const stableReason = (reason) => {
    const value = String(reason ?? "").trim().toLowerCase();
    return /^[a-z0-9_]{1,64}$/.test(value) ? value : "unavailable";
};

const throwIfAborted = (signal) => {
    if (!signal?.aborted) return;
    if (typeof signal.throwIfAborted === "function") signal.throwIfAborted();
    throw Object.assign(new Error("Capability invocation was aborted."), { name: "AbortError" });
};
