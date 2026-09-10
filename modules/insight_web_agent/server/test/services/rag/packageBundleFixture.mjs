/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";
import {
    bundleFileId,
    bundleMcpId,
    bundleSkillId,
} from "../../../services/rag/wire/packageBundleContracts.mjs";
import { createCanonicalZip, createPackageV4Members, sha256 } from "./packageFixture.mjs";
import { RUNTIME_CONTRACT } from "./packageFixture.mjs";

export const RUNTIME_CONTRACT_V5 = Object.freeze({
    ...RUNTIME_CONTRACT,
    packageSchemaVersion: "5.0",
});

export const PACKAGE_V5_METADATA_MEMBERS = Object.freeze([
    "manifest.json",
    "sources.jsonl",
    "documents.jsonl",
    "chunks.jsonl",
    "vectors.f32",
    "bm25-domain-dict.txt",
    "bm25.json",
    "skills.jsonl",
    "mcps.jsonl",
    "bundle-files.jsonl",
    "bundle-skips.jsonl",
]);
export const PACKAGE_V5_SUFFIX_MEMBERS = Object.freeze(["build-audit.json", "checksums.json"]);

export const createBundleVector = () => {
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
        externalRequirements: [],
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
        "tools/run.sh": "#!/bin/sh\necho demo",
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
            sha256: sha256(Buffer.from(text, "utf8")),
            sizeBytes: Buffer.byteLength(text),
            gitMode: sourcePath.endsWith(".sh") ? "100755" : "100644",
            mediaType: "text/plain",
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
    const bundle = {
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
    return { skill, mcp, files, skip, payload, bundle };
};

export const createPackageV5Members = ({ bundle = createBundleVector(), kbVersion = "26.1.1" } = {}) => {
    const base = createPackageV4Members({ kbVersion });
    const manifest = { ...JSON.parse(base.get("manifest.json").toString("utf8")), schemaVersion: "5.0", bundle: bundle.bundle };
    const jsonl = (records) => Buffer.concat(records.map(canonicalJsonBytes));
    const members = new Map([
        ["manifest.json", canonicalJsonBytes(manifest)],
        ["sources.jsonl", base.get("sources.jsonl")],
        ["documents.jsonl", base.get("documents.jsonl")],
        ["chunks.jsonl", base.get("chunks.jsonl")],
        ["vectors.f32", base.get("vectors.f32")],
        ["bm25-domain-dict.txt", base.get("bm25-domain-dict.txt")],
        ["bm25.json", base.get("bm25.json")],
        ["skills.jsonl", bundle.skill ? jsonl([bundle.skill]) : Buffer.alloc(0)],
        ["mcps.jsonl", bundle.mcp ? jsonl([bundle.mcp]) : Buffer.alloc(0)],
        ["bundle-files.jsonl", jsonl(bundle.files)],
        ["bundle-skips.jsonl", bundle.skip ? jsonl([bundle.skip]) : Buffer.alloc(0)],
    ]);
    for (const [packagePath, data] of Object.entries(bundle.payload).sort(([left], [right]) => (left < right ? -1 : 1))) {
        members.set(packagePath, data);
    }
    members.set("build-audit.json", base.get("build-audit.json"));
    const digests = {};
    for (const name of [...members.keys()].sort()) digests[name] = sha256(members.get(name));
    members.set("checksums.json", canonicalJsonBytes({ schemaVersion: "4.0", algorithm: "sha256", files: digests }));
    return members;
};

export const packageV5MemberModes = (members) => {
    const modes = {};
    for (const name of members.keys()) modes[name] = 0o100644 << 16;
    modes["bundles/group/repo/tools/run.sh"] = 0o100755 << 16;
    return modes;
};

export const writePackageV5Handoff = async (root, options = {}) => {
    const handoffDir = join(root, options.directory ?? "26.1.1");
    await mkdir(handoffDir, { recursive: true });
    const members = options.members ?? createPackageV5Members(options);
    const modes = options.modes ?? packageV5MemberModes(members);
    const archive = createCanonicalZip(members, {
        names: [...PACKAGE_V5_METADATA_MEMBERS, ...[...members.keys()].filter((name) => name.startsWith("bundles/")).sort(), ...PACKAGE_V5_SUFFIX_MEMBERS],
        externalAttributesByName: modes,
    });
    const digest = sha256(archive);
    const archivePath = join(handoffDir, "knowledge-pack-v5.zip");
    const sidecarPath = join(handoffDir, "knowledge-pack-v5.zip.sha256");
    await writeFile(archivePath, archive);
    await writeFile(sidecarPath, options.sidecarBytes ?? Buffer.from(`${digest}  knowledge-pack-v5.zip\n`, "ascii"));
    return { archive, archivePath, sidecarPath, digest, members, handoffDir };
};

export const emptyBundleVector = () => ({
    skill: null,
    mcp: null,
    files: [],
    skip: null,
    payload: {},
    bundle: {
        skillsAllowlisted: 0,
        skillsIncluded: 0,
        skillsSkipped: 0,
        mcpsAllowlisted: 0,
        mcpsIncluded: 0,
        mcpsSkipped: 0,
        toolsIncluded: 0,
        toolsSkipped: 0,
        bundleFiles: 0,
        bundleBytes: 0,
    },
});
