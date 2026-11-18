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
import { randomUUID } from "node:crypto";
import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { setImmediate as yieldTurn } from "node:timers/promises";
import { buildSessionExport, resolveExportDirectory } from "./sessionExport.mjs";

export const SESSION_BATCH_EXPORT_FORMAT = "msinsight.agent-sessions.v1";
const DEFAULT_LIMITS = {
    jobTtlMs: 5 * 60_000,
    maxJobs: 8,
    maxSnapshotBytes: 64 * 1024 * 1024,
    maxHistoryBytes: 8 * 1024 * 1024,
    maxCandidates: 10_000,
    maxPagesPerAgent: 1_000,
    maxAgents: 64,
    maxIssues: 200,
    operationTimeoutMs: 30_000,
    queryTimeoutMs: 5 * 60_000,
};
const SOURCE_ERRORS = {
    session_export_list_unsupported: "Agent does not support listing sessions",
    session_export_load_unsupported: "Agent does not support loading exportable history (resume-only is unsupported)",
    session_export_readonly_unsupported: "Native Agent does not advertise read-only session export",
    session_export_history_limit: "Session history exceeded the export size limit",
    session_export_history_invalid: "Agent returned malformed session history",
    session_export_busy: "Session is busy and cannot be exported",
    session_busy: "Session is busy and cannot be exported",
    session_export_timeout: "Agent export operation timed out",
    session_export_source_closed: "Agent export source is closed",
};
const fail = (status, message, code) => Object.assign(new Error(message), { status, code });
const byteLength = (value) => Buffer.byteLength(JSON.stringify(value), "utf8");
const clone = (value) => structuredClone(value);
const safeString = (value) => typeof value === "string" ? value : undefined;

/** Independent, bounded snapshots: never reads or changes live chat state. */
export const createSessionBatchExportService = ({ listAgents, openSource, resolveTitle = (_agentId, _sessionId, fallback) => fallback, config = {}, now = () => new Date() }) => {
    const limits = Object.fromEntries(Object.entries(DEFAULT_LIMITS).map(([key, fallback]) => {
        const requested = config.sessionBatchExport?.[key];
        return [key, Number.isSafeInteger(requested) && requested > 0 ? requested : fallback];
    }));
    const jobs = new Map();
    const sources = new Set();
    const pendingOpens = new Set();
    const closings = new WeakMap();
    const tasks = new Set();
    const abort = new AbortController();
    let disposed = false;
    const clock = () => new Date(now()).getTime();
    const assertOpen = () => { if (disposed) throw fail(409, "Session export service is disposed"); };
    const retainedBytes = () => [...jobs.values()].reduce((sum, job) => sum + job.bytes, 0);
    const prune = () => {
        const time = clock();
        for (const [id, job] of jobs) {
            if (!["scanning", "exporting"].includes(job.phase) && time - job.finishedAt >= limits.jobTtlMs) jobs.delete(id);
        }
    };
    const getJob = (id) => {
        assertOpen();
        prune();
        const job = typeof id === "string" && jobs.get(id);
        if (!job) throw fail(404, "Session export job was not found or has expired", "session_export_job_expired");
        return job;
    };
    const issue = (job, agentId, sessionId, code, message) => {
        if (disposed) return;
        if (job.issues.length >= limits.maxIssues) {
            const last = job.issues.at(-1);
            if (last?.code !== "issue_limit") job.issues[job.issues.length - 1] = { agentId: "", code: "issue_limit", message: "Additional diagnostics omitted: issue limit reached" };
            return;
        }
        job.issues.push({ agentId, ...(sessionId === undefined ? {} : { sessionId }), code, message });
    };
    const sourceIssue = (job, agentId, sessionId, error, fallbackCode, fallbackMessage) => {
        const code = Object.hasOwn(SOURCE_ERRORS, error?.code) ? error.code : fallbackCode;
        issue(job, agentId, sessionId, code, SOURCE_ERRORS[code] ?? fallbackMessage);
    };
    const close = (source) => {
        if (closings.has(source)) return closings.get(source);
        const result = Promise.resolve().then(() => source.close()).finally(() => sources.delete(source));
        closings.set(source, result);
        return result;
    };
    const timed = (operation) => new Promise((resolve, reject) => {
        let timer;
        const onAbort = () => finish(reject, fail(409, "Session export service is disposed"));
        const finish = (fn, value) => {
            clearTimeout(timer);
            abort.signal.removeEventListener("abort", onAbort);
            fn(value);
        };
        if (abort.signal.aborted) return reject(fail(409, "Session export service is disposed"));
        abort.signal.addEventListener("abort", onAbort, { once: true });
        timer = setTimeout(() => finish(reject, Object.assign(new Error("Export operation timed out"), { code: "session_export_timeout" })), limits.operationTimeoutMs);
        Promise.resolve().then(operation).then((value) => finish(resolve, value), (error) => finish(reject, error));
    });
    const track = (operation) => {
        const task = Promise.resolve().then(operation);
        tasks.add(task);
        task.finally(() => tasks.delete(task)).catch(() => {});
    };
    const publicJob = (job) => clone({
        jobId: job.jobId, phase: job.phase, processed: job.processed, total: job.total,
        items: job.items, issues: job.issues, filters: job.filters,
        ...(job.document ? { document: job.document } : {}),
        ...(job.error ? { error: job.error } : {}),
    });

    const scan = async (job, agents) => {
        const ownedSources = new Map();
        const candidates = [];
        const seen = new Set();
        const started = Date.now();
        let metadataBytes = 0;
        const overTime = () => Date.now() - started >= limits.queryTimeoutMs;
        try {
            for (const agent of agents) {
                if (disposed) return;
                if (overTime()) { issue(job, agent.name, undefined, "query_timeout", "Query time limit reached; results are partial"); break; }
                let source;
                let abandoned = false;
                const ownerAbort = new AbortController();
                const opening = Promise.resolve().then(() => openSource(agent, { signal: AbortSignal.any([abort.signal, ownerAbort.signal]) }));
                pendingOpens.add(opening);
                opening.finally(() => pendingOpens.delete(opening)).catch(() => {});
                try {
                    source = await timed(async () => {
                        const opened = await opening;
                        sources.add(opened);
                        if (disposed || abandoned) { await close(opened); throw fail(409, "Export source no longer needed"); }
                        return opened;
                    });
                    ownedSources.set(agent.name, source);
                } catch (error) {
                    abandoned = true;
                    ownerAbort.abort();
                    sourceIssue(job, agent.name, undefined, error, "owner_unavailable", "Agent export source could not be opened");
                    continue;
                }
                let cursor;
                const cursors = new Set();
                for (let pageNumber = 0; ; pageNumber++) {
                    if (disposed) return;
                    if (pageNumber >= limits.maxPagesPerAgent || overTime()) {
                        issue(job, agent.name, undefined, overTime() ? "query_timeout" : "page_limit", "Session discovery limit reached; results are partial"); break;
                    }
                    let page;
                    try { page = await timed(() => source.list({ cursor })); }
                    catch (error) {
                        sourceIssue(job, agent.name, undefined, error, "list_page_failed", "A session list page could not be read; earlier pages are retained"); break;
                    }
                    if (!page || !Array.isArray(page.sessions)) {
                        issue(job, agent.name, undefined, "malformed_page", "Agent returned a malformed session list page"); break;
                    }
                    let capped = false;
                    for (const session of page.sessions) {
                        if (typeof session?.sessionId !== "string" || !session.sessionId.trim() || session.sessionId.length > 2048) {
                            issue(job, agent.name, undefined, "malformed_session", "Agent returned an invalid session identifier"); continue;
                        }
                        const sessionId = session.sessionId;
                        const key = JSON.stringify([agent.name, sessionId]);
                        if (seen.has(key)) continue;
                        if (seen.size >= limits.maxCandidates) {
                            issue(job, agent.name, undefined, "candidate_limit", "Session discovery capacity reached; results are partial"); capped = true; break;
                        }
                        seen.add(key);
                        const updatedTimestamp = parseTimestamp(session.updatedAt);
                        if (job.updatedFrom !== undefined && job.updatedTo !== undefined) {
                            if (updatedTimestamp === undefined) { issue(job, agent.name, sessionId, "unknown_updated_at", "Session has no valid updatedAt timestamp and was excluded"); continue; }
                            if (updatedTimestamp < job.updatedFrom || updatedTimestamp > job.updatedTo) continue;
                        }
                        const createdTimestamp = parseTimestamp(session.createdAt);
                        if (job.createdFrom !== undefined && job.createdTo !== undefined) {
                            if (createdTimestamp === undefined) { issue(job, agent.name, sessionId, "unknown_created_at", "Session has no valid createdAt timestamp and was excluded"); continue; }
                            if (createdTimestamp < job.createdFrom || createdTimestamp > job.createdTo) continue;
                        }
                        const fallback = safeString(session.title) || sessionId;
                        let title;
                        try { title = safeString(resolveTitle(agent.name, sessionId, fallback)) || fallback; }
                        catch { issue(job, agent.name, sessionId, "title_resolution_failed", "Session title could not be resolved and was excluded"); continue; }
                        // Filter discovery metadata before loading any conversation history.
                        if (!matchesKeyword(title, job.filters.keyword)) continue;
                        const item = {
                            key, agentId: agent.name, sessionId,
                            title,
                            ...(createdTimestamp !== undefined ? { createdAt: new Date(createdTimestamp).toISOString() } : {}),
                            updatedAt: new Date(updatedTimestamp ?? createdTimestamp ?? clock()).toISOString(),
                            ...(safeString(session.primaryAgentId) ? { primaryAgentId: safeString(session.primaryAgentId) } : {}),
                            ...(safeString(session.primaryAgentName) ? { primaryAgentName: safeString(session.primaryAgentName) } : {}),
                        };
                        const size = byteLength(item);
                        if (candidates.length >= limits.maxCandidates || metadataBytes + size > limits.maxSnapshotBytes - retainedBytes()) {
                            issue(job, agent.name, undefined, "candidate_limit", "Session discovery capacity reached; results are partial"); capped = true; break;
                        }
                        candidates.push({ item, source });
                        metadataBytes += size;
                    }
                    if (capped) break;
                    const next = page.nextCursor;
                    if (next === undefined || next === null) break;
                    if (typeof next !== "string" || !next.trim() || next.length > 4096) {
                        issue(job, agent.name, undefined, "malformed_cursor", "Agent returned an invalid pagination cursor"); break;
                    }
                    if (cursors.has(next)) { issue(job, agent.name, undefined, "repeated_cursor", "Agent repeated a pagination cursor; earlier pages are retained"); break; }
                    cursors.add(next);
                    cursor = next;
                }
            }
            if (disposed) return;
            job.total = candidates.length;
            const skippedAfterTimeout = new Map();
            for (const { item, source } of candidates) {
                if (disposed) return;
                if (overTime()) { issue(job, item.agentId, undefined, "query_timeout", "Query time limit reached; results are partial"); break; }
                if (skippedAfterTimeout.has(item.agentId)) {
                    skippedAfterTimeout.set(item.agentId, skippedAfterTimeout.get(item.agentId) + 1);
                    job.processed++;
                    continue;
                }
                try {
                    const messages = await timed(() => source.load(item.sessionId));
                    if (disposed) return;
                    if (!Array.isArray(messages)) throw Object.assign(new Error("Malformed history"), { code: "session_export_history_invalid" });
                    if (byteLength(messages) > limits.maxHistoryBytes) { issue(job, item.agentId, item.sessionId, "session_export_history_limit", SOURCE_ERRORS.session_export_history_limit); continue; }
                    const document = buildSessionExport({ ...item, messages: clone(messages), agent: source.agentInfo, exportedAt: new Date(clock()).toISOString() });
                    document.owner = { agentId: item.agentId };
                    for (const field of ["primaryAgentId", "primaryAgentName"]) if (item[field] !== undefined) document.session[field] = item[field];
                    const size = byteLength(document) + byteLength(item);
                    if (size + retainedBytes() > limits.maxSnapshotBytes) { issue(job, item.agentId, item.sessionId, "snapshot_limit", "Snapshot memory limit reached; this session was not retained"); continue; }
                    job.snapshots.set(item.key, document);
                    job.items.push(item);
                    job.bytes += size;
                } catch (error) {
                    sourceIssue(job, item.agentId, item.sessionId, error, "load_failed", "Session history could not be loaded");
                    if (error?.code === "session_export_timeout") {
                        // Do not overlap the next load with late replay from a timed-out request.
                        skippedAfterTimeout.set(item.agentId, 0);
                        await close(source).catch(() => {});
                    }
                } finally { if (!disposed) job.processed++; }
                await yieldTurn();
            }
            for (const [agentId, skipped] of skippedAfterTimeout) {
                if (skipped > 0) issue(job, agentId, undefined, "agent_load_aborted", `Skipped ${skipped} remaining sessions for this Agent after a history load timed out`);
            }
        } catch {
            if (!disposed) { job.phase = "failed"; job.error = "Session query failed"; }
        } finally {
            await Promise.all([...ownedSources.entries()].map(async ([agentId, source]) => {
                try { await close(source); } catch { issue(job, agentId, undefined, "source_close_failed", "Temporary Agent source could not be closed cleanly"); }
            }));
            if (!disposed) {
                if (job.phase === "scanning") { job.phase = "ready"; job.snapshotReady = true; }
                job.finishedAt = clock();
            }
        }
    };

    const startQuery = async (input = {}) => {
        assertOpen();
        const filters = validateFilters(input, clock());
        let configured;
        try { configured = await timed(listAgents); } catch { assertOpen(); throw fail(409, "Configured Agents could not be listed"); }
        assertOpen();
        if (!Array.isArray(configured)) throw fail(409, "Configured Agents could not be listed");
        const available = new Map(configured.filter((agent) => typeof agent?.name === "string" && agent.name.trim() && agent.name.length <= 1024).map((agent) => [agent.name, agent]));
        const requested = filters.agentIds.length ? filters.agentIds : [...available.keys()];
        if (requested.some((id) => !available.has(id))) throw fail(400, "Unknown Agent selection", "session_export_unknown_agent");
        prune();
        // Do not silently evict snapshots that are still within their advertised TTL.
        if (jobs.size >= limits.maxJobs) throw fail(409, "Session export job capacity reached; wait for older jobs to expire", "session_export_job_capacity");
        const jobId = randomUUID();
        const job = {
            jobId, phase: "scanning", processed: 0, total: null, items: [], issues: [],
            filters: { ...filters, agentIds: requested },
            updatedFrom: filters.from ? Date.parse(filters.from) : undefined,
            updatedTo: filters.to ? Date.parse(filters.to) : undefined,
            from: filters.from ? Date.parse(filters.from) : undefined,
            to: filters.to ? Date.parse(filters.to) : undefined,
            createdFrom: filters.createdFrom ? Date.parse(filters.createdFrom) : undefined,
            createdTo: filters.createdTo ? Date.parse(filters.createdTo) : undefined,
            snapshots: new Map(), snapshotReady: false, bytes: 0, finishedAt: clock()
        };
        jobs.set(jobId, job);
        if (requested.length > limits.maxAgents) issue(job, "", undefined, "agent_limit", "Agent discovery limit reached; results are partial");
        track(() => scan(job, requested.slice(0, limits.maxAgents).map((id) => available.get(id))));
        return { jobId };
    };
    const startExport = (input = {}) => {
        assertOpen();
        if (!input || typeof input !== "object" || Array.isArray(input)) throw fail(400, "Export options must be an object");
        const { jobId, keys, persist = false } = input;
        const job = getJob(jobId);
        if (!job.snapshotReady || job.phase === "exporting") throw fail(409, "Session export snapshot is not ready", "session_export_job_not_ready");
        if (typeof persist !== "boolean") throw fail(400, "persist must be a boolean");
        if (keys !== undefined && (!Array.isArray(keys) || keys.some((key) => typeof key !== "string" || !job.snapshots.has(key)))) throw fail(400, "Selected keys must belong to this query snapshot");
        const selected = keys === undefined ? job.items.map(({ key }) => key) : [...new Set(keys)];
        if (!selected.length) throw fail(400, "No matching sessions were selected for export");
        job.phase = "exporting";
        job.processed = 0;
        job.total = selected.length;
        delete job.error;
        delete job.document;
        track(async () => {
            try {
                const sessions = [];
                for (const key of selected) {
                    if (disposed) return;
                    sessions.push(job.snapshots.get(key));
                    job.processed++;
                    await yieldTurn();
                }
                if (disposed) return;
                const document = { format: SESSION_BATCH_EXPORT_FORMAT, filename: `insight-sessions-${randomUUID()}.json`, exportedAt: new Date(clock()).toISOString(), filters: clone(job.filters), issues: clone(job.issues), sessions };
                job.document = document;
                if (persist) {
                    const directory = resolveExportDirectory(config.sessionExportDir);
                    await mkdir(directory, { recursive: true });
                    if (disposed) return;
                    const savedPath = join(directory, document.filename);
                    await writeFile(savedPath, JSON.stringify(document, null, 2), { encoding: "utf8", flag: "wx" });
                    if (disposed) return;
                    job.document = { ...document, savedPath };
                }
                if (!disposed) job.phase = "completed";
            } catch {
                if (!disposed) { job.phase = "failed"; job.error = "Batch export could not be saved; the downloadable document is retained when available"; }
            } finally { if (!disposed) job.finishedAt = clock(); }
        });
        return { jobId };
    };
    const dispose = async () => {
        disposed = true;
        abort.abort();
        await Promise.allSettled([...sources].map(close));
        await Promise.allSettled([...tasks]);
        // The ACP factory rejects initialize only after its abort-triggered
        // disconnect finishes. Await that cleanup too; injected factories that
        // ignore cancellation remain bounded and their eventual sources close.
        await Promise.all([...pendingOpens].map((opening) => new Promise((resolve) => {
            const timer = setTimeout(resolve, 1500);
            const done = () => { clearTimeout(timer); resolve(); };
            opening.then(done, done);
        })));
        jobs.clear();
    };
    return { startQuery, getStatus: (jobId) => publicJob(getJob(jobId)), startExport, dispose };
};

const parseTimestamp = (value, maxFractionDigits = Infinity) => {
    if (typeof value === "number") return Number.isFinite(value) && Math.abs(value) <= 8.64e15 ? value : undefined;
    // Explicit RFC 3339 offsets only; never infer the host/browser timezone.
    const parts = typeof value === "string" && /^(\d{4})-(\d\d)-(\d\d)T(\d\d):(\d\d):(\d\d)(?:\.(\d+))?(Z|[+-](\d\d):(\d\d))$/i.exec(value);
    if (!parts) return undefined;
    const [, yearText, monthText, dayText, hourText, minuteText, secondText, fraction = "", , offsetHourText, offsetMinuteText] = parts;
    if (fraction.length > maxFractionDigits) return undefined;
    const [year, month, day, hour, minute, second] = [yearText, monthText, dayText, hourText, minuteText, secondText].map(Number);
    const leap = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
    const monthDays = [31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    // Date.parse normalizes invalid calendar days and 24:00, so check fields first.
    if (month < 1 || month > 12 || day < 1 || day > monthDays[month - 1] || hour > 23 || minute > 59 || second > 59) return undefined;
    if (offsetHourText !== undefined && (Number(offsetHourText) > 23 || Number(offsetMinuteText) > 59)) return undefined;
    const parsed = Date.parse(value);
    return Number.isFinite(parsed) ? parsed : undefined;
};
const validateFilters = (input, time) => {
    if (!input || typeof input !== "object" || Array.isArray(input)) throw fail(400, "Query filters must be an object");
    const {
        agentIds = [],
        days, from, to,
        updatedDays, updatedFrom, updatedTo,
        createdDays, createdFrom, createdTo,
        allTime = false,
        keyword = "",
    } = input;

    const uDays = updatedDays !== undefined ? updatedDays : days;
    const uFrom = updatedFrom !== undefined ? updatedFrom : from;
    const uTo = updatedTo !== undefined ? updatedTo : to;
    const hasUDays = uDays !== undefined;
    const hasURange = uFrom !== undefined || uTo !== undefined;
    if (hasUDays && hasURange) throw fail(400, "Use either days or from/to, not both");
    let fromTime;
    let toTime;
    if (hasUDays) {
        if (!Number.isInteger(uDays) || uDays < 1 || uDays > 36500) throw fail(400, "days must be an integer from 1 to 36500");
        fromTime = time - uDays * 86_400_000;
        toTime = time;
    } else if (hasURange) {
        fromTime = typeof uFrom === "string" ? parseTimestamp(uFrom, 3) : undefined;
        toTime = typeof uTo === "string" ? parseTimestamp(uTo, 3) : undefined;
        if (fromTime === undefined || toTime === undefined) throw fail(400, "from and to must both be valid timestamps with an explicit timezone and at most 3 fractional digits");
        if (fromTime > toTime) throw fail(400, "from must not be later than to");
    }

    const hasCDays = createdDays !== undefined;
    const hasCRange = createdFrom !== undefined || createdTo !== undefined;
    if (hasCDays && hasCRange) throw fail(400, "Use either createdDays or createdFrom/createdTo, not both");
    let cFromTime;
    let cToTime;
    if (hasCDays) {
        if (!Number.isInteger(createdDays) || createdDays < 1 || createdDays > 36500) throw fail(400, "createdDays must be an integer from 1 to 36500");
        cFromTime = time - createdDays * 86_400_000;
        cToTime = time;
    } else if (hasCRange) {
        cFromTime = typeof createdFrom === "string" ? parseTimestamp(createdFrom, 3) : undefined;
        cToTime = typeof createdTo === "string" ? parseTimestamp(createdTo, 3) : undefined;
        if (cFromTime === undefined || cToTime === undefined) throw fail(400, "createdFrom and createdTo must both be valid timestamps with an explicit timezone and at most 3 fractional digits");
        if (cFromTime > cToTime) throw fail(400, "createdFrom must not be later than createdTo");
    }

    const hasUpdated = hasUDays || hasURange;
    const hasCreated = hasCDays || hasCRange;
    if (!hasUpdated && !hasCreated && !allTime) {
        throw fail(400, "Specify days or both from and to");
    }

    if (!Array.isArray(agentIds) || agentIds.some((id) => typeof id !== "string" || !id.trim() || id.length > 1024)) throw fail(400, "agentIds must be an array of Agent identifiers");
    if (typeof keyword !== "string" || keyword.length > 4096) throw fail(400, "keyword must be a string of at most 4096 characters");
    if (!Number.isFinite(time) ||
        (hasUpdated && (!Number.isFinite(new Date(fromTime).getTime()) || !Number.isFinite(new Date(toTime).getTime()))) ||
        (hasCreated && (!Number.isFinite(new Date(cFromTime).getTime()) || !Number.isFinite(new Date(cToTime).getTime())))) {
        throw fail(409, "Server clock is unavailable");
    }

    return {
        agentIds: [...new Set(agentIds)],
        ...(hasUDays ? { days: uDays } : {}),
        keyword: keyword.trim(),
        ...(hasUpdated ? { from: new Date(fromTime).toISOString(), to: new Date(toTime).toISOString(), timestamp: "updatedAt" } : {}),
        ...(hasCDays ? { createdDays } : {}),
        ...(hasCreated ? { createdFrom: new Date(cFromTime).toISOString(), createdTo: new Date(cToTime).toISOString() } : {}),
        ...(allTime ? { allTime: true } : {}),
    };
};
// Whitespace-separated tokens are ANDed; every literal character in each token
// must occur in title order, but need not be contiguous ("ds" matches "DeepSeek").
const matchesKeyword = (title, keyword) => {
    const haystack = title.toLowerCase();
    return keyword.toLowerCase().split(/\s+/u).filter(Boolean).every((token) => {
        let offset = 0;
        for (const character of token) {
            const found = haystack.indexOf(character, offset);
            if (found === -1) return false;
            offset = found + character.length;
        }
        return true;
    });
};
