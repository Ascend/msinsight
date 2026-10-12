/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { test } from "node:test";
import { fileURLToPath } from "node:url";
import {
    bundleFileId,
    bundleMcpId,
    bundleSkillId,
    referencedByIncluded,
    validateBundleChecksums,
    validateBundleClosure,
    validateBundleRecords,
    validateManifestBundle,
} from "../../../services/rag/wire/packageBundleContracts.mjs";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";

const FIXTURE_DIR = join(dirname(fileURLToPath(import.meta.url)), "fixtures");

const readFixture = (name) => readFile(join(FIXTURE_DIR, name), "utf8").then(JSON.parse);
const sha256 = (value) => createHash("sha256").update(value).digest("hex");
const toJsonl = (records) => `${records.map((record) => JSON.stringify(record)).join("\n")}\n`;

test("vendored producer golden validates through the consumer v5 wire validator", async () => {
    const fixture = await readFixture("package-records.json");

    const bundle = validateBundleRecords({
        skills: toJsonl([fixture.skill]),
        mcps: toJsonl([fixture.mcp]),
        files: toJsonl([fixture.file]),
        skips: toJsonl([fixture.skip]),
        checksums: `${JSON.stringify(fixture.checksums)}\n`,
    });

    assert.equal(bundle.skills[0].skillId, fixture.skill.skillId);
    assert.equal(bundle.mcps[0].mcpId, fixture.mcp.mcpId);
    assert.equal(bundle.files[0].fileId, fixture.file.fileId);
    assert.equal(bundle.skips[0].id, fixture.skip.id);
    assert.deepEqual(Object.keys(bundle.checksums.files).sort(), Object.keys(fixture.checksums.files).sort());
});

test("conformance golden re-derives every identity and member digest", async () => {
    const fixture = await readFixture("package-input-v1.json");
    const expectedSidecar = (await readFile(join(FIXTURE_DIR, "package-input-v1.json.sha256"), "utf8")).trim();
    const raw = await readFile(join(FIXTURE_DIR, "package-input-v1.json"), "utf8");
    assert.equal(sha256(raw), expectedSidecar);

    assert.equal(fixture.skillId, bundleSkillId(fixture.skill.sourceId, fixture.skill.rootPath));
    assert.equal(
        fixture.mcpId,
        bundleMcpId(fixture.mcp.sourceId, fixture.mcp.name, fixture.mcp.rootPath),
    );
    for (const [name, text] of Object.entries(fixture.members)) {
        assert.equal(sha256(text), fixture.memberSha256[name]);
    }
    // The conformance member predates assembler sort order; sort file lines
    // before record validation (identities and digests above are unaffected).
    const sortedFiles = `${fixture.members["bundle-files.jsonl"].trimEnd().split("\n").sort().join("\n")}\n`;
    const bundle = validateBundleRecords({
        skills: fixture.members["skills.jsonl"],
        mcps: fixture.members["mcps.jsonl"],
        files: sortedFiles,
        skips: fixture.members["bundle-skips.jsonl"],
    });
    assert.equal(bundle.skills.length, 1);
    assert.equal(bundle.files.length, 3);
});

const buildVector = () => {
    const sourceId = "group/repo";
    const skillId = bundleSkillId(sourceId, "skills/demo");
    const mcpId = bundleMcpId(sourceId, "svc", "app/tools/mcp_servers");
    const skill = {
        skillId,
        sourceId,
        rootPath: "skills/demo",
        entryPath: "skills/demo/SKILL.md",
        name: "demo",
        description: "Demo skill.",
        toolPaths: ["tools/run.sh"],
        requiredMcpIds: [mcpId],
        externalRequirements: [{ type: "command", name: "bash", required: true }],
    };
    const mcp = {
        mcpId,
        sourceId,
        name: "svc",
        rootPath: "app/tools/mcp_servers",
        entryPath: "app/tools/mcp_servers/svc.py",
        runtime: "python",
        transport: "stdio",
        arguments: ["-m", "svc"],
        environment: ["SVC_HOME"],
    };
    const contents = {
        "skills/demo/SKILL.md": "# Demo skill.",
        "app/tools/mcp_servers/svc.py": "# svc",
        "tools/run.sh": "#!/bin/sh",
    };
    const owners = {
        "skills/demo/SKILL.md": [{ kind: "skill", id: skillId }],
        "app/tools/mcp_servers/svc.py": [{ kind: "mcp", id: mcpId }],
        "tools/run.sh": [{ kind: "skill-tool", id: skillId }],
    };
    const files = Object.entries(contents)
        .map(([sourcePath, text]) => ({
            fileId: bundleFileId(sourceId, sourcePath),
            sourceId,
            sourcePath,
            packagePath: `bundles/${sourceId}/${sourcePath}`,
            sha256: sha256(text),
            sizeBytes: Buffer.byteLength(text),
            gitMode: sourcePath.endsWith(".sh") ? "100755" : "100644",
            mediaType: sourcePath.endsWith(".md") ? "text/markdown" : "text/plain",
            owners: owners[sourcePath],
        }))
        .sort((left, right) => (left.fileId < right.fileId ? -1 : 1));
    const skip = {
        id: bundleSkillId(sourceId, "skills/other"),
        kind: "skill",
        sourceId,
        rootPath: "skills/other",
        reasonCode: "SKILL_FRONTMATTER_INVALID",
        reasonPath: "skills/other/SKILL.md",
    };
    const payload = Object.fromEntries(
        Object.entries(contents).map(([sourcePath, text]) => [
            `bundles/${sourceId}/${sourcePath}`,
            Buffer.from(text, "utf8"),
        ]),
    );
    const manifestBundle = {
        skillsAllowlisted: 2,
        skillsIncluded: 1,
        skillsSkipped: 1,
        mcpsAllowlisted: 1,
        mcpsIncluded: 1,
        mcpsSkipped: 0,
        toolsIncluded: 1,
        toolsSkipped: 0,
        bundleFiles: 3,
        bundleBytes: Object.values(payload).reduce((total, data) => total + data.length, 0),
    };
    const lines = {
        skills: canonicalJsonBytes(skill).toString("utf8"),
        mcps: canonicalJsonBytes(mcp).toString("utf8"),
        files: files.map((record) => canonicalJsonBytes(record).toString("utf8")).join(""),
        skips: canonicalJsonBytes(skip).toString("utf8"),
    };
    return { skill, mcp, files, skip, payload, manifestBundle, lines };
};

test("forged identities are rejected", async () => {
    const fixture = await readFixture("package-input-v1.json");

    assert.throws(
        () => validateBundleRecords({
            skills: fixture.members["skills.jsonl"].replace('"sk_', '"sk0'),
            mcps: fixture.members["mcps.jsonl"],
            files: fixture.members["bundle-files.jsonl"],
            skips: fixture.members["bundle-skips.jsonl"],
        }),
        /skillId|strict schema|identity/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: fixture.members["skills.jsonl"],
            mcps: fixture.members["mcps.jsonl"],
            files: fixture.members["bundle-files.jsonl"].replace('"f_', '"f0', 1),
            skips: fixture.members["bundle-skips.jsonl"],
        }),
        /fileId|strict schema|identity/,
    );
});

test("unsorted records are rejected", async () => {
    const fixture = await readFixture("package-input-v1.json");
    const lines = fixture.members["bundle-files.jsonl"].trimEnd().split("\n");
    const reordered = `${lines[1]}\n${lines[0]}\n${lines[2]}\n`;

    assert.throws(
        () => validateBundleRecords({
            skills: fixture.members["skills.jsonl"],
            mcps: fixture.members["mcps.jsonl"],
            files: reordered,
            skips: fixture.members["bundle-skips.jsonl"],
        }),
        /sorted/,
    );
});

test("owners must reference included components", () => {
    const vector = buildVector();
    const bundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
    });
    const unknown = `sk_${"9".repeat(64)}`;
    const tampered = vector.lines.files.replace(vector.skill.skillId, unknown, 1);
    const tamperedBundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: tampered,
        skips: vector.lines.skips,
    });

    assert.throws(
        () => validateBundleClosure({ ...tamperedBundle, payload: vector.payload, manifestBundle: vector.manifestBundle }),
        /owner/,
    );
    const wrongKind = vector.lines.files.replace('"skill-tool"', '"mcp"', 1);
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: wrongKind,
            skips: vector.lines.skips,
        }),
        /owner|kind/,
    );
});

test("empty bundle members validate but blank lines do not", () => {
    const empty = validateBundleRecords({ skills: "", mcps: "", files: "", skips: "" });
    assert.deepEqual([empty.skills, empty.mcps, empty.files, empty.skips], [[], [], [], []]);
    assert.throws(
        () => validateBundleRecords({ skills: "\n", mcps: "", files: "", skips: "" }),
        /blank/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: "",
            mcps: "",
            files: "not-json\n",
            skips: "",
        }),
        /strict|json|literal|JSON/i,
    );
});

test("bundle closure binds entries tools counts and payload", () => {
    const vector = buildVector();
    const bundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
    });

    const closure = validateBundleClosure({ ...bundle, payload: vector.payload, manifestBundle: vector.manifestBundle });
    assert.equal(closure.status, "partial");

    assert.throws(
        () => validateBundleClosure({
            ...bundle,
            payload: { ...vector.payload, "bundles/group/repo/stray.txt": Buffer.from("stray") },
            manifestBundle: vector.manifestBundle,
        }),
        /orphan/,
    );
    const { [Object.keys(vector.payload)[0]]: _dropped, ...missing } = vector.payload;
    assert.throws(
        () => validateBundleClosure({ ...bundle, payload: missing, manifestBundle: vector.manifestBundle }),
        /missing/,
    );
});

test("manifest bundle counts must bind records", () => {
    const vector = buildVector();
    const bundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
    });
    validateManifestBundle(vector.manifestBundle, { ...bundle, payload: vector.payload });
    assert.throws(
        () => validateManifestBundle(
            { ...vector.manifestBundle, skillsIncluded: 9 },
            { ...bundle, payload: vector.payload },
        ),
        /bundle/,
    );
});

test("v5 checksums reject missing self-referencing and unsorted members", async () => {    const fixture = await readFixture("package-records.json");
    const { validateBundleChecksums } = await import("../../../services/rag/wire/packageBundleContracts.mjs");
    const members = new Map(Object.entries({
        "manifest.json": Buffer.from("{}"),
        "skills.jsonl": Buffer.from(""),
    }));
    assert.throws(() => validateBundleChecksums(fixture.checksums, members), /cover|checksum/);
    const selfRef = {
        ...fixture.checksums,
        files: { ...fixture.checksums.files, "checksums.json": "a".repeat(64) },
    };
    assert.throws(() => validateBundleChecksums(selfRef, members), /checksum|cover|itself/);
});

test("negative matrix pins every remaining branch", async () => {
    const fixture = await readFixture("package-records.json");
    const { validateBundleChecksums } = await import("../../../services/rag/wire/packageBundleContracts.mjs");
    const members = new Map(Object.entries({ "manifest.json": Buffer.from("{}") }));
    const badAlgorithm = { ...fixture.checksums, algorithm: "md5" };
    assert.throws(() => validateBundleChecksums(badAlgorithm, members), /checksum/);
    assert.throws(() => validateBundleChecksums({ ...fixture.checksums, files: [] }, members), /checksum/);
    assert.throws(
        () => validateBundleChecksums(
            { ...fixture.checksums, files: { ...fixture.checksums.files, "evil.txt": "a".repeat(64) } },
            members,
        ),
        /checksum/,
    );
    assert.throws(
        () => validateBundleChecksums(
            { ...fixture.checksums, files: { ...fixture.checksums.files, "bundles/": "a".repeat(64) } },
            members,
        ),
        /checksum/,
    );

    const vector = buildVector();
    const mutatedSkill = {
        ...JSON.parse(vector.lines.skills),
        entryPath: "other/SKILL.md",
    };
    assert.throws(
        () => validateBundleRecords({
            skills: canonicalJsonBytes(mutatedSkill).toString("utf8"),
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /closure|entry/,
    );
    const badEnvMcp = {
        ...JSON.parse(vector.lines.mcps),
        environment: ["SVC_HOME=x"],
    };
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: canonicalJsonBytes(badEnvMcp).toString("utf8"),
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /environment|schema/,
    );
    const badPath = {
        ...JSON.parse(vector.lines.files.split("\n")[0]),
        packagePath: "bundles/group/repo/elsewhere.md",
    };
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: [canonicalJsonBytes(badPath).toString("utf8").trimEnd(), ...vector.lines.files.trimEnd().split("\n").slice(1)].join("\n") + "\n",
            skips: vector.lines.skips,
        }),
        /packagePath|closure|fileId/,
    );
    const badSource = {
        ...JSON.parse(vector.lines.skills),
        sourceId: "/absolute",
    };
    assert.throws(
        () => validateBundleRecords({
            skills: canonicalJsonBytes(badSource).toString("utf8"),
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /sourceId|normalized|schema/,
    );
    const bundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
    });
    const drifted = { ...vector.payload };
    const firstKey = Object.keys(drifted)[0];
    drifted[firstKey] = Buffer.from(`${drifted[firstKey].toString("utf8")} `);
    assert.throws(
        () => validateBundleClosure({ ...bundle, payload: drifted }),
        /mismatch|payload/,
    );
    const undeclaredTool = {
        ...JSON.parse(vector.lines.skills),
        toolPaths: ["tools/run.sh", "tools/gone.sh"],
    };
    assert.throws(
        () => validateBundleClosure({
            ...validateBundleRecords({
                skills: canonicalJsonBytes(undeclaredTool).toString("utf8"),
                mcps: vector.lines.mcps,
                files: vector.lines.files,
                skips: vector.lines.skips,
            }),
            payload: vector.payload,
        }),
        /payload|closure|tool/,
    );
    assert.throws(
        () => validateManifestBundle(
            { ...vector.manifestBundle, mcpsAllowlisted: 99 },
            { ...bundle, payload: vector.payload },
        ),
        /allowlisted|bundle/,
    );
    const { bundleFileId: fileIdentity } = await import("../../../services/rag/wire/packageBundleContracts.mjs");    const residueText = "residue";
    const residue = {
        fileId: fileIdentity("group/repo", "skills/other/extra.txt"),
        sourceId: "group/repo",
        sourcePath: "skills/other/extra.txt",
        packagePath: "bundles/group/repo/skills/other/extra.txt",
        sha256: sha256(residueText),
        sizeBytes: Buffer.byteLength(residueText),
        gitMode: "100644",
        mediaType: "text/plain",
        owners: [{ kind: "skill", id: vector.skill.skillId }],
    };
    const residueLines = [...vector.lines.files.trimEnd().split("\n"), canonicalJsonBytes(residue).toString("utf8").trimEnd()]
        .sort()
        .join("\n") + "\n";    const residuePayload = {
        ...vector.payload,
        "bundles/group/repo/skills/other/extra.txt": Buffer.from(residueText),
    };
    assert.throws(
        () => validateBundleClosure({
            ...validateBundleRecords({
                skills: vector.lines.skills,
                mcps: vector.lines.mcps,
                files: residueLines,
                skips: vector.lines.skips,
            }),
            payload: residuePayload,
        }),
        /residue/,
    );
});

test("remaining branches pin arithmetic paths and key shapes", () => {    const vector = buildVector();
    const bundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
    });
    assert.throws(
        () => validateManifestBundle(
            { ...vector.manifestBundle, skillsAllowlisted: 99 },
            { ...bundle, payload: vector.payload },
        ),
        /allowlisted|bundle/,
    );
    const undeclaredText = "undeclared";
    const undeclared = {
        fileId: bundleFileId("group/repo", "skills/demo/undeclared.txt"),
        sourceId: "group/repo",
        sourcePath: "skills/demo/undeclared.txt",
        packagePath: "bundles/group/repo/skills/demo/undeclared.txt",
        sha256: sha256(undeclaredText),
        sizeBytes: Buffer.byteLength(undeclaredText),
        gitMode: "100644",
        mediaType: "text/plain",
        owners: [{ kind: "skill-tool", id: vector.skill.skillId }],
    };
    const lines = [...vector.lines.files.trimEnd().split("\n"), canonicalJsonBytes(undeclared).toString("utf8").trimEnd()]
        .sort()
        .join("\n") + "\n";
    assert.throws(
        () => validateBundleClosure({
            ...validateBundleRecords({
                skills: vector.lines.skills,
                mcps: vector.lines.mcps,
                files: lines,
                skips: vector.lines.skips,
            }),
            payload: {
                ...vector.payload,
                "bundles/group/repo/skills/demo/undeclared.txt": Buffer.from(undeclaredText),
            },
        }),
        /declared tool/,
    );
    const badRoot = { ...JSON.parse(vector.lines.skills), rootPath: "../evil" };
    assert.throws(
        () => validateBundleRecords({
            skills: canonicalJsonBytes(badRoot).toString("utf8"),
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /normalized|POSIX|path/,
    );
    const extraKey = { ...JSON.parse(vector.lines.skills), unexpected: 1 };
    assert.throws(
        () => validateBundleRecords({
            skills: canonicalJsonBytes(extraKey).toString("utf8"),
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /fields|contract|schema/,
    );
});

test("referenced files under skipped roots do not count as residue", () => {    const vector = buildVector();
    const mcpSkip = {
        id: vector.mcp.mcpId,
        kind: "mcp",
        sourceId: "group/repo",
        rootPath: vector.mcp.rootPath,
        reasonCode: "MCP_ENTRY_MISSING",
        reasonPath: vector.mcp.entryPath,
    };
    const toolSkip = {
        id: vector.skill.skillId,
        kind: "skill-tool",
        sourceId: "group/repo",
        rootPath: "tools/run.sh",
        reasonCode: "OPTIONAL_TOOL_SKIPPED",
        reasonPath: "tools/run.sh",
    };
    const skipsWithMcp = [vector.lines.skips.trimEnd(), canonicalJsonBytes(mcpSkip).toString("utf8").trimEnd(), canonicalJsonBytes(toolSkip).toString("utf8").trimEnd()]
        .sort((a, b) => {
            const ka = JSON.parse(a);
            const kb = JSON.parse(b);
            const left = `${ka.kind}\0${ka.id}`;
            const right = `${kb.kind}\0${kb.id}`;
            return left < right ? -1 : 1;
        })
        .join("\n") + "\n";
    const bundle = validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: skipsWithMcp,
    });
    const closure = validateBundleClosure({ ...bundle, payload: vector.payload });
    assert.equal(closure.status, "partial");
});

test("referencedByIncluded answers every owner shape directly", () => {
    const vector = buildVector();
    const skill = JSON.parse(vector.lines.skills);
    const mcp = JSON.parse(vector.lines.mcps);
    const skillsById = new Map([[skill.skillId, skill]]);
    const mcpsById = new Map([[mcp.mcpId, mcp]]);
    const entryFile = { sourcePath: skill.entryPath, owners: [{ kind: "skill", id: skill.skillId }] };
    const toolFile = { sourcePath: "tools/run.sh", owners: [{ kind: "skill-tool", id: skill.skillId }] };
    const mcpFile = { sourcePath: mcp.entryPath, owners: [{ kind: "mcp", id: mcp.mcpId }] };
    assert.equal(referencedByIncluded(entryFile, skillsById, mcpsById), true);
    assert.equal(referencedByIncluded(toolFile, skillsById, mcpsById), true);
    assert.equal(referencedByIncluded(mcpFile, skillsById, mcpsById), true);
    assert.equal(
        referencedByIncluded(
            { sourcePath: "other.md", owners: [{ kind: "skill", id: `sk_${"0".repeat(64)}` }] },
            skillsById,
            mcpsById,
        ),
        false,
    );
    assert.equal(
        referencedByIncluded(
            { sourcePath: "other.md", owners: [{ kind: "mcp", id: `mcp_${"0".repeat(64)}` }] },
            skillsById,
            mcpsById,
        ),
        false,
    );
    assert.equal(referencedByIncluded({ sourcePath: "other.md", owners: [] }, skillsById, mcpsById), false);
    assert.equal(
        referencedByIncluded(
            { sourcePath: "app/tools/mcp_servers/notes.md", owners: [{ kind: "mcp", id: mcp.mcpId }] },
            skillsById,
            mcpsById,
        ),
        false,
    );
});

test("value-level forgery and guard matrix", async () => {
    const { validateBundleChecksums } = await import("../../../services/rag/wire/packageBundleContracts.mjs");
    const vector = buildVector();
    const members = new Map(Object.entries({ "manifest.json": Buffer.from("{}") }));
    const good = JSON.parse(vector.lines.skills);
    const mutateSkill = (patch) => canonicalJsonBytes({ ...good, ...patch }).toString("utf8");
    const records = (skills, extra = {}) => validateBundleRecords({
        skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
        ...extra,
    });
    assert.throws(() => records(mutateSkill({ skillId: `sk_${"0".repeat(64)}` })), /identity/);
    assert.throws(() => records(mutateSkill({ name: "  " })), /name|trimmed|empty/);
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: canonicalJsonBytes({ ...JSON.parse(vector.lines.mcps), runtime: "node" }).toString("utf8"),
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /runtime|schema/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: canonicalJsonBytes({ ...JSON.parse(vector.lines.mcps), transport: "sse" }).toString("utf8"),
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /transport|schema/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: canonicalJsonBytes({ ...JSON.parse(vector.lines.mcps), mcpId: `mcp_${"0".repeat(64)}` }).toString("utf8"),
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /identity/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
            checksums: canonicalJsonBytes({ schemaVersion: "5.0", algorithm: "sha256", files: {} }).toString("utf8"),
        }),
        /checksum|schema/,
    );
});

test("guard coverage matrix", () => {
    const vector = buildVector();
    const skill = JSON.parse(vector.lines.skills);
    const mcp = JSON.parse(vector.lines.mcps);
    const files = vector.lines.files.trimEnd().split("\n").map((line) => JSON.parse(line));
    const skip = JSON.parse(vector.lines.skips);
    const line = (record) => canonicalJsonBytes(record).toString("utf8");
    const lines = (records) => `${records.map((record) => line(record).trimEnd()).join("\n")}\n`;
    const records = (overrides) => validateBundleRecords({
        skills: vector.lines.skills,
        mcps: vector.lines.mcps,
        files: vector.lines.files,
        skips: vector.lines.skips,
        ...overrides,
    });
    assert.throws(() => records({ skills: line({ ...skill, toolPaths: "nope" }) }), /array/);
    assert.throws(() => records({ skills: line({ ...skill, toolPaths: [42] }) }), /string/);
    assert.throws(
        () => records({ skills: line({ ...skill, externalRequirements: [{ type: "nope", name: "x", required: true }] }) }),
        /type|requirement/,
    );
    assert.throws(
        () => records({ skills: line({ ...skill, externalRequirements: [{ type: "command", name: "x", required: "yes" }] }) }),
        /required|boolean|flag/,
    );
    assert.throws(
        () => records({ skills: line({ ...skill, externalRequirements: "nope" }) }),
        /array|requirement/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: line({ ...mcp, name: "a/b" }),
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /segment|single|name/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: line({ ...mcp, environment: ["A B"] }),
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /environment|variable|space/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: lines([{ ...files[0], gitMode: "100600" }, ...files.slice(1)]),
            skips: vector.lines.skips,
        }),
        /git mode|mode/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: lines([{ ...files[0], owners: [] }, ...files.slice(1)]),
            skips: vector.lines.skips,
        }),
        /owners|empty/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: lines([{ ...files[0], owners: [{ kind: "nope", id: skill.skillId }] }, ...files.slice(1)]),
            skips: vector.lines.skips,
        }),
        /kind|owner/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: lines([{ ...files[0], sizeBytes: -1 }, ...files.slice(1)]),
            skips: vector.lines.skips,
        }),
        /integer|range|size/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: lines([{ ...files[0], mediaType: "" }, ...files.slice(1)]),
            skips: vector.lines.skips,
        }),
        /media|empty|trimmed/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: lines([skill, skill]),
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /duplicate/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: lines([skip, skip]),
        }),
        /duplicate/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: line({ ...skip, kind: "nope" }),
        }),
        /kind|skip/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: line({ ...skip, id: "bad" }),
        }),
        /kind|prefix|match|ID/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: vector.lines.skills,
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: line({ ...skip, reasonCode: "" }),
        }),
        /reason|empty/,
    );
    assert.throws(
        () => validateBundleRecords({
            skills: line({ ...skill, rootPath: "skills/demo/" }),
            mcps: vector.lines.mcps,
            files: vector.lines.files,
            skips: vector.lines.skips,
        }),
        /normalized|POSIX|path/,
    );
    const ghostMcp = `mcp_${"1".repeat(64)}`;
    assert.throws(
        () => validateBundleClosure({
            ...validateBundleRecords({
                skills: line({ ...skill, requiredMcpIds: [ghostMcp] }),
                mcps: vector.lines.mcps,
                files: vector.lines.files,
                skips: vector.lines.skips,
            }),
            payload: vector.payload,
        }),
        /unknown MCP|closure/,
    );
    assert.throws(
        () => validateBundleClosure({
            ...validateBundleRecords({
                skills: vector.lines.skills,
                mcps: vector.lines.mcps,
                files: vector.lines.files,
                skips: vector.lines.skips,
            }),
            payload: { ...vector.payload, [`bundles/group/repo/${files[0].sourcePath}`]: "not-bytes" },
        }),
        /bytes|payload/,
    );
    assert.throws(
        () => validateBundleChecksums(
            {
                schemaVersion: "4.0",
                algorithm: "sha256",
                files: {
                    "manifest.json": "zz",
                    "sources.jsonl": "a".repeat(64),
                    "documents.jsonl": "a".repeat(64),
                    "chunks.jsonl": "a".repeat(64),
                    "vectors.f32": "a".repeat(64),
                    "bm25-domain-dict.txt": "a".repeat(64),
                    "bm25.json": "a".repeat(64),
                    "skills.jsonl": "a".repeat(64),
                    "mcps.jsonl": "a".repeat(64),
                    "bundle-files.jsonl": "a".repeat(64),
                    "bundle-skips.jsonl": "a".repeat(64),
                    "build-audit.json": "a".repeat(64),
                },
            },
            new Map([["manifest.json", Buffer.from("{}")]]),
        ),
        /checksum|hex|lowercase/,
    );
});