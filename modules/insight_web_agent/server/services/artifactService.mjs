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
import { mkdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";

const ID_PATTERN = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const MAX_HTML_BYTES = 512 * 1024;
const fail = (message, status = 400) => Object.assign(new Error(message), {
    status, code: status === 404 ? "CAPABILITY_NOT_FOUND" : status === 409 ? "COMMAND_BUSY" : "CAPABILITY_INVALID_ARGUMENT",
});

// Store one immutable snapshot per revision. The manifest is the atomic commit point.
export const createArtifactService = ({ storeDir }) => {
    const queues = new Map();
    const artifactPath = (id) => {
        if (typeof id !== "string" || !ID_PATTERN.test(id)) throw fail("Invalid artifact ID.");
        return join(storeDir, id);
    };
    const read = async (path) => {
        try { return JSON.parse(await readFile(path, "utf8")); } catch (error) {
            if (error.code === "ENOENT") throw fail("Report not found.", 404);
            throw error;
        }
    };
    const atomicWrite = async (path, value) => {
        const temp = `${path}.${randomUUID()}.tmp`;
        try {
            await writeFile(temp, JSON.stringify(value), { encoding: "utf8", mode: 0o600 });
            await rename(temp, path);
        } finally { await rm(temp, { force: true }); }
    };
    const get = async (id, version) => {
        const directory = artifactPath(id);
        const manifest = await read(join(directory, "manifest.json"));
        const revision = version === undefined ? manifest.version : Number(version);
        if (!Number.isSafeInteger(revision) || revision < 1) throw fail("Invalid report version.");
        if (revision > manifest.version) throw fail("Report version not found.", 404);
        return { ...await read(join(directory, `${revision}.json`)), versions: manifest.versions };
    };
    const publish = async (input, { signal } = {}) => {
        if (!input || typeof input !== "object" || Array.isArray(input)
            || Object.keys(input).some((key) => !["artifactId", "baseVersion", "title", "description", "html"].includes(key))) throw fail("Invalid report input.");
        if (typeof input.title !== "string" || !input.title.trim() || input.title.length > 120) throw fail("title must contain 1–120 characters.");
        if (input.description != null && (typeof input.description !== "string" || input.description.length > 500)) throw fail("description must not exceed 500 characters.");
        if (typeof input.html !== "string" || !input.html.trim() || Buffer.byteLength(input.html, "utf8") > MAX_HTML_BYTES) throw fail("html must contain 1–524288 UTF-8 bytes.");
        if (input.artifactId == null && input.baseVersion != null) throw fail("For a new report, set both artifactId and baseVersion to null.");
        if (input.artifactId != null && !ID_PATTERN.test(input.artifactId)) throw fail("For a new report, set artifactId and baseVersion to null. For updates, use the exact ID/version from the previous publish_report result.");
        const id = input.artifactId ?? randomUUID();
        const directory = artifactPath(id);
        const previous = queues.get(id) ?? Promise.resolve();
        const operation = previous.catch(() => {}).then(async () => {
            signal?.throwIfAborted();
            const manifest = input.artifactId ? await read(join(directory, "manifest.json")) : { version: 0, versions: [] };
            if (input.artifactId && input.baseVersion !== manifest.version) throw fail(`Report changed. Use baseVersion ${manifest.version} to update it.`, 409);
            const artifact = {
                kind: "visualization", artifactId: id, version: manifest.version + 1,
                title: input.title.trim(), description: input.description?.trim() ?? "", createdAt: new Date().toISOString(),
            };
            await mkdir(directory, { recursive: true, mode: 0o700 });
            await atomicWrite(join(directory, `${artifact.version}.json`), { ...artifact, html: input.html });
            await atomicWrite(join(directory, "manifest.json"), { version: artifact.version, versions: [...manifest.versions, artifact] });
            return artifact;
        });
        queues.set(id, operation);
        try { return await operation; } finally { if (queues.get(id) === operation) queues.delete(id); }
    };
    return { publish, get };
};
