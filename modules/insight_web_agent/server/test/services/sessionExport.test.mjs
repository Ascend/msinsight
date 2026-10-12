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
import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { buildSessionExport, resolveExportDirectory, SESSION_EXPORT_FORMAT, writeSessionExport } from "../../services/sessionExport.mjs";

test("builds a portable session document with markdown and a safe filename", () => {
    const result = buildSessionExport({
        sessionId: "ses_f67280f22ffeBia7jgqkmCLkzG",
        title: "我当前选中的这一段区间为什么内存在持续叠加?",
        updatedAt: "2026-09-13T06:31:33.000Z",
        exportedAt: "2026-09-13T06:41:00.000Z",
        agent: { name: "OpenCode", version: "1.18.30" },
        messages: [
            {
                id: "m1",
                role: "user",
                content: [{ id: "t1", type: "text", text: "Why is memory growing?" }],
            },
            {
                id: "m2",
                role: "assistant",
                completionStatus: "completed",
                content: [
                    { id: "th1", type: "thinking", text: "Need observe first." },
                    {
                        id: "tool1",
                        type: "tool",
                        toolCall: { name: "msinsight.observe", status: "completed", input: "{}", output: "unavailable" },
                    },
                    { id: "t2", type: "text", text: "MemScope is not ready." },
                ],
            },
        ],
    });

    assert.equal(result.format, SESSION_EXPORT_FORMAT);
    assert.equal(result.agent.name, "OpenCode");
    assert.equal(result.session.sessionId, "ses_f67280f22ffeBia7jgqkmCLkzG");
    assert.equal(
        result.filename,
        "insight-session-我当前选中的这一段区间为什么内存在持续叠加-a7jgqkmCLkzG-2026-09-13-06-41-00.json",
    );
    assert.match(result.markdown, /# /);
    assert.match(result.markdown, /## User/);
    assert.match(result.markdown, /## Assistant/);
    assert.match(result.markdown, /### Thinking/);
    assert.match(result.markdown, /### Tool: msinsight.observe/);
    assert.match(result.markdown, /MemScope is not ready/);
});

test("uses the session id when the title is empty", () => {
    const result = buildSessionExport({
        sessionId: "session-empty",
        title: "   ",
        exportedAt: "2026-09-13T00:00:00.000Z",
        messages: [],
    });
    assert.equal(result.session.title, "session-empty");
    assert.match(result.markdown, /no messages/);
});

test("resolves the user download directory for each desktop platform", () => {
    assert.equal(
        resolveExportDirectory(undefined, { platform: "darwin", home: "/Users/demo", env: {} }),
        "/Users/demo/Downloads",
    );
    assert.equal(
        resolveExportDirectory(undefined, {
            platform: "win32",
            home: "C:\\Users\\demo",
            env: { USERPROFILE: "C:\\Users\\demo" },
        }),
        join("C:\\Users\\demo", "Downloads"),
    );
    assert.equal(
        resolveExportDirectory(undefined, {
            platform: "linux",
            home: "/home/demo",
            env: { XDG_DOWNLOAD_DIR: "/home/demo/下载" },
        }),
        "/home/demo/下载",
    );
    assert.equal(
        resolveExportDirectory("/tmp/exports", { platform: "darwin", home: "/Users/demo", env: {} }),
        "/tmp/exports",
    );
});

test("writes the export document to the requested directory", async () => {
    const directory = await mkdtemp(join(tmpdir(), "insight-session-export-"));
    try {
        const document = buildSessionExport({
            sessionId: "session-write",
            title: "Write test",
            exportedAt: "2026-09-13T00:00:00.000Z",
            messages: [],
        });
        const savedPath = await writeSessionExport(document, directory);
        assert.equal(savedPath, join(directory, document.filename));
        const saved = JSON.parse(await readFile(savedPath, "utf8"));
        assert.equal(saved.session.sessionId, "session-write");
    } finally {
        await rm(directory, { recursive: true, force: true });
    }
});
