/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import { EventEmitter } from "node:events";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Readable } from "node:stream";
import test from "node:test";
import yauzl from "yauzl";
import { readInstalledBundlePayload, loadKnowledgePack } from "../../../services/rag/knowledgePackLoader.mjs";
import { createCanonicalZip, MODEL_CONTRACT } from "./packageFixture.mjs";
import { createPackageV5Members, RUNTIME_CONTRACT_V5 } from "./packageBundleFixture.mjs";

const fixture = async (t) => {
    const directory = await mkdtemp(join(tmpdir(), "insight-installed-bundle-"));
    t.after(() => rm(directory, { recursive: true, force: true }));
    return directory;
};

test("reads all Bundle bytes with exactly one ZIP open and one traversal", async (t) => {
    const directory = await fixture(t);
    const members = createPackageV5Members();
    for (let i = 0; i < 409; i++) members.set(`bundles/group/repo/extra/${i}.txt`, Buffer.from(String(i)));
    await writeFile(join(directory, "knowledge-pack-v5.zip"), createCanonicalZip(members, { names: [...members.keys()] }));
    let opens = 0;
    let entries = 0;
    const payload = await readInstalledBundlePayload(directory, (path, options, callback) => {
        opens++;
        yauzl.open(path, options, (error, zip) => {
            zip?.on("entry", () => entries++);
            callback(error, zip);
        });
    });
    assert.equal(opens, 1);
    assert.equal(entries, members.size);
    assert.equal(payload.size, 412);
    for (const [name, bytes] of payload) assert.deepEqual(bytes, members.get(name));
});

test("rejects duplicate Bundle paths instead of silently overwriting bytes", async (t) => {
    const directory = await fixture(t);
    const name = "bundles/group/repo/SKILL.md";
    await writeFile(join(directory, "knowledge-pack-v5.zip"), createCanonicalZip(new Map([[name, Buffer.from("demo")]]), { names: [name, name] }));
    await assert.rejects(readInstalledBundlePayload(directory), { code: "pack_unreadable" });
});

test("loaded Bundle bytes still undergo checksum validation", async (t) => {
    const directory = await fixture(t);
    const members = createPackageV5Members();
    for (const [name, bytes] of members) {
        if (!name.startsWith("bundles/")) await writeFile(join(directory, name), bytes);
    }
    await writeFile(join(directory, "knowledge-pack-v5.zip"), createCanonicalZip(members, { names: [...members.keys()] }));
    const options = { runtimeContract: RUNTIME_CONTRACT_V5, modelContract: MODEL_CONTRACT };
    assert.equal((await loadKnowledgePack(directory, options)).bundle.files.length, 3);
    members.set("bundles/group/repo/skills/demo/SKILL.md", Buffer.from("tampered content"));
    await writeFile(join(directory, "knowledge-pack-v5.zip"), createCanonicalZip(members, { names: [...members.keys()] }));
    await assert.rejects(loadKnowledgePack(directory, options), /checksum|mismatch/);
});

test("rejects missing or unreadable archives", async (t) => {
    const directory = await fixture(t);
    await assert.rejects(readInstalledBundlePayload(directory), { code: "pack_unreadable" });
    await writeFile(join(directory, "knowledge-pack-v5.zip"), "not a ZIP");
    await assert.rejects(readInstalledBundlePayload(directory), { code: "pack_unreadable" });
});

for (const failure of ["stream-error", "early-close", "archive-error", "oversized"]) {
    test(`closes the ZIP and rejects on ${failure}`, async () => {
        const zip = new EventEmitter();
        let closed = false;
        zip.close = () => { closed = true; };
        zip.readEntry = () => queueMicrotask(() => {
            if (failure === "early-close") zip.emit("close");
            else if (failure === "archive-error") zip.emit("error", new Error("broken archive"));
            else zip.emit("entry", { fileName: "bundles/group/repo/demo.txt", uncompressedSize: failure === "oversized" ? 513 * 1024 * 1024 : 1 });
        });
        zip.openReadStream = (_entry, callback) => {
            const stream = new Readable({ read() { this.destroy(new Error("broken stream")); } });
            callback(null, stream);
        };
        await assert.rejects(readInstalledBundlePayload("unused", (_path, _options, callback) => callback(null, zip)), { code: "pack_unreadable" });
        assert.equal(closed, true);
    });
}
