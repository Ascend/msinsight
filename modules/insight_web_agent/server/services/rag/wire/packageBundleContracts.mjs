/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { parseCanonicalJson, parseCanonicalJsonl, strictObjectKeys } from "./strictJsonParser.mjs";

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
export const PACKAGE_V5_FIXED_MEMBERS = Object.freeze([...PACKAGE_V5_METADATA_MEMBERS, ...PACKAGE_V5_SUFFIX_MEMBERS]);
export const PACKAGE_V5_PAYLOAD_PREFIX = "bundles/";

const SHA256_RE = /^[0-9a-f]{64}$/;
const SKILL_ID_RE = /^sk_[0-9a-f]{64}$/;
const MCP_ID_RE = /^mcp_[0-9a-f]{64}$/;
const FILE_ID_RE = /^f_[0-9a-f]{64}$/;
const SOURCE_ID_RE = /^[^/\\:\0]+\/[^/\\:\0]+$/;

export class BundleContractError extends Error {
    constructor(code, message) {
        super(message);
        this.name = "BundleContractError";
        this.code = code;
    }
}

export const bundleSkillId = (sourceId, rootPath) => `sk_${sha256(`skill\0${sourceId}\0${rootPath}`)}`;
export const bundleMcpId = (sourceId, name, rootPath) => `mcp_${sha256(`mcp\0${sourceId}\0${name}\0${rootPath}`)}`;
export const bundleFileId = (sourceId, sourcePath) => `f_${sha256(`file\0${sourceId}\0${sourcePath}`)}`;

export const validateBundleRecords = ({ skills, mcps, files, skips, checksums }) => {
    const skillRecords = parseRecordLines(skills, "skills.jsonl").map(validateSkillRecord);
    const mcpRecords = parseRecordLines(mcps, "mcps.jsonl").map(validateMcpRecord);
    const fileRecords = parseRecordLines(files, "bundle-files.jsonl").map(validateFileRecord);
    const skipRecords = parseRecordLines(skips, "bundle-skips.jsonl").map(validateSkipRecord);
    assertSortedIds(skillRecords.map(({ skillId }) => skillId), "skills.jsonl");
    assertSortedIds(mcpRecords.map(({ mcpId }) => mcpId), "mcps.jsonl");
    assertSortedIds(fileRecords.map(({ fileId }) => fileId), "bundle-files.jsonl");
    assertSortedIds(skipRecords.map(({ kind, id }) => `${kind}\0${id}`), "bundle-skips.jsonl");
    assertUnique(skillRecords.map(({ skillId }) => skillId), "skillId");
    assertUnique(mcpRecords.map(({ mcpId }) => mcpId), "mcpId");
    assertUnique(fileRecords.map(({ fileId }) => fileId), "fileId");
    assertUnique(fileRecords.map(({ packagePath }) => packagePath), "package path");
    assertUnique(skipRecords.map(({ kind, id }) => `${kind}\0${id}`), "skip record");
    const bundle = { skills: skillRecords, mcps: mcpRecords, files: fileRecords, skips: skipRecords };
    if (checksums !== undefined) bundle.checksums = validateBundleChecksums(parseCanonicalJson(checksums, "checksums.json"));
    return bundle;
};

export const validateBundleChecksums = (checksums, members) => {
    exactKeys(checksums, ["algorithm", "files", "schemaVersion"], "checksums.json");
    if (checksums.schemaVersion !== "4.0") fail("checksum_invalid", "checksums schemaVersion must be 4.0");
    if (checksums.algorithm !== "sha256") fail("checksum_invalid", "checksums algorithm must be sha256");
    if (!checksums.files || typeof checksums.files !== "object" || Array.isArray(checksums.files)) {
        fail("checksum_invalid", "checksums files must be an object");
    }
    const names = strictObjectKeys(checksums.files);
    if (names.includes("checksums.json")) fail("checksum_invalid", "checksums must not cover checksums.json itself");
    for (const name of PACKAGE_V5_FIXED_MEMBERS) {
        if (name !== "checksums.json" && !names.includes(name)) fail("checksum_invalid", `checksums must cover every fixed Package v5 member: ${name}`);
    }
    for (const name of names) {
        if (PACKAGE_V5_FIXED_MEMBERS.includes(name)) continue;
        if (!name.startsWith(PACKAGE_V5_PAYLOAD_PREFIX) || name.endsWith("/")) {
            fail("checksum_invalid", `dynamic checksum members must be Bundle payload files: ${name}`);
        }
        sha256String(checksums.files[name], `checksums ${name}`);
    }
    const sorted = [...names].sort(compareCodePoints);
    if (names.some((name, index) => name !== sorted[index])) fail("checksum_invalid", "checksum member names must be sorted by Unicode code point");
    for (const name of names) sha256String(checksums.files[name], `checksums ${name}`);
    if (members !== undefined) {
        const covered = new Set([...names, "checksums.json"]);
        const actual = new Set(members instanceof Map ? [...members.keys()] : Object.keys(members));
        if (covered.size !== actual.size || [...covered].some((name) => !actual.has(name))) {
            fail("checksum_invalid", "checksums must exactly cover Package members");
        }
        for (const name of names) {
            const bytes = members instanceof Map ? members.get(name) : members[name];
            if (sha256(bytes) !== checksums.files[name]) fail("checksum_mismatch", `Package checksum mismatch: ${name}`);
        }
    }
    return checksums;
};

export const validateBundleClosure = ({ skills, mcps, files, skips, payload, manifestBundle }) => {
    const skillIds = new Set(skills.map(({ skillId }) => skillId));
    const mcpIds = new Set(mcps.map(({ mcpId }) => mcpId));
    const skillsById = new Map(skills.map((skill) => [skill.skillId, skill]));
    for (const record of files) {
        for (const owner of record.owners) {
            if (owner.kind === "skill" && !skillIds.has(owner.id)) fail("owner_unknown", "bundle file owner references an unknown skill");
            if (owner.kind === "mcp" && !mcpIds.has(owner.id)) fail("owner_unknown", "bundle file owner references an unknown MCP");
            if (owner.kind === "skill-tool") {
                const ownerSkill = skillsById.get(owner.id);
                if (!ownerSkill) fail("owner_unknown", "bundle file owner references an unknown skill");
                if (!ownerSkill.toolPaths.includes(record.sourcePath)) {
                    fail("owner_unknown", "skill-tool owner file is not a declared tool of its skill");
                }
            }
        }
    }
    const filePaths = new Set(files.map(({ packagePath }) => packagePath));
    const payloadNames = Object.keys(payload);
    for (const name of payloadNames) {
        if (!filePaths.has(name)) fail("payload_orphan", `orphan Bundle payload without file record: ${name}`);
    }
    for (const record of files) {
        if (!Object.hasOwn(payload, record.packagePath)) fail("payload_missing", `bundle file record is missing Bundle payload: ${record.packagePath}`);
        const data = payload[record.packagePath];
        if (!Buffer.isBuffer(data)) fail("payload_missing", `Bundle payload is not bytes: ${record.packagePath}`);
        if (sha256(data) !== record.sha256 || data.length !== record.sizeBytes) {
            fail("payload_mismatch", `Bundle payload bytes do not match its file record: ${record.packagePath}`);
        }
    }
    for (const skill of skills) {
        requirePayload(filePaths, skill.sourceId, skill.entryPath, "skill entry");
        for (const toolPath of skill.toolPaths) requirePayload(filePaths, skill.sourceId, toolPath, "skill tool");
        for (const required of skill.requiredMcpIds) {
            if (!mcpIds.has(required)) fail("closure_gap", "skill requires an unknown MCP");
        }
    }
    for (const mcp of mcps) requirePayload(filePaths, mcp.sourceId, mcp.entryPath, "MCP entry");
    for (const skip of skips) {
        if (skip.kind !== "skill" && skip.kind !== "mcp") continue;
        const prefix = `${skip.rootPath}/`;
        for (const record of files) {
            if (record.sourcePath !== skip.rootPath && !record.sourcePath.startsWith(prefix)) continue;
            if (!referencedByIncluded(record, skillsById, new Map(mcps.map((mcp) => [mcp.mcpId, mcp])))) {
                fail("skipped_residue", `skipped component left payload residue: ${record.packagePath}`);
            }
        }
    }
    const bundle = manifestBundle ?? null;
    if (bundle) validateManifestBundle(bundle, { skills, mcps, files, skips, payload });
    const included = skills.length + mcps.length;
    const skipped = skips.filter(({ kind }) => kind !== "skill-tool").length + skips.filter(({ kind }) => kind === "skill-tool").length;
    return { status: included === 0 ? "empty" : skipped === 0 ? "complete" : "partial", skills, mcps, files, skips };
};

export const validateManifestBundle = (bundle, { skills, mcps, files, skips, payload }) => {
    object(bundle, "manifest bundle");
    const skillSkips = skips.filter(({ kind }) => kind === "skill").length;
    const mcpSkips = skips.filter(({ kind }) => kind === "mcp").length;
    const toolSkips = skips.filter(({ kind }) => kind === "skill-tool").length;
    const payloadNames = new Set(Object.keys(payload));
    const filesByPath = new Map(files.map((record) => [record.packagePath, record]));
    // Mirror the producer: tools and byte counts bind the staged payload, not the records.
    const toolsIncluded = [...payloadNames].filter((name) =>
        (filesByPath.get(name)?.owners ?? []).some(({ kind }) => kind === "skill-tool"),
    ).length;
    const bundleBytes = [...payloadNames].reduce((total, name) => total + payload[name].length, 0);
    const expected = {
        skillsIncluded: skills.length,
        skillsSkipped: skillSkips,
        mcpsIncluded: mcps.length,
        mcpsSkipped: mcpSkips,
        toolsIncluded,
        toolsSkipped: toolSkips,
        bundleFiles: payloadNames.size,
        bundleBytes,
    };
    for (const [field, value] of Object.entries(expected)) {
        if (bundle[field] !== value) fail("bundle_counts_invalid", `manifest bundle ${field} does not bind Bundle records`);
    }
    if (bundle.skillsAllowlisted !== bundle.skillsIncluded + bundle.skillsSkipped) {
        fail("bundle_counts_invalid", "allowlisted skills must equal included plus skipped skills");
    }
    if (bundle.mcpsAllowlisted !== bundle.mcpsIncluded + bundle.mcpsSkipped) {
        fail("bundle_counts_invalid", "allowlisted MCPs must equal included plus skipped MCPs");
    }
    return bundle;
};

const validateSkillRecord = (record) => {
    exactKeys(record, ["description", "entryPath", "externalRequirements", "name", "requiredMcpIds", "rootPath", "skillId", "sourceId", "toolPaths"], "skill record");
    match(record.skillId, SKILL_ID_RE, "skillId");
    match(record.sourceId, SOURCE_ID_RE, "sourceId");
    validateSourcePath(record.rootPath, "rootPath");
    validateSourcePath(record.entryPath, "entryPath");
    nonEmptyString(record.name, "skill name");
    nonEmptyText(record.description, "skill description");
    stringArray(record.toolPaths, "toolPaths");
    stringArray(record.requiredMcpIds, "requiredMcpIds");
    for (const id of record.requiredMcpIds) match(id, MCP_ID_RE, "required MCP ID");
    if (!Array.isArray(record.externalRequirements)) fail("bundle_schema_invalid", "externalRequirements must be an array");
    for (const item of record.externalRequirements) {
        exactKeys(item, ["name", "required", "type"], "external requirement");
        if (!["command", "python-package", "product-tool"].includes(item.type)) fail("bundle_schema_invalid", "external requirement type is invalid");
        nonEmptyString(item.name, "external requirement name");
        if (typeof item.required !== "boolean") fail("bundle_schema_invalid", "external requirement flag is invalid");
    }
    if (record.skillId !== bundleSkillId(record.sourceId, record.rootPath)) fail("identity_mismatch", "skillId identity does not match sourceId and rootPath");
    if (record.entryPath !== `${record.rootPath}/SKILL.md` && !record.entryPath.startsWith(`${record.rootPath}/`)) {
        fail("closure_gap", "skill entryPath must live under rootPath");
    }
    return record;
};

const validateMcpRecord = (record) => {
    exactKeys(record, ["arguments", "entryPath", "environment", "mcpId", "name", "rootPath", "runtime", "sourceId", "transport"], "MCP record");
    match(record.mcpId, MCP_ID_RE, "mcpId");
    match(record.sourceId, SOURCE_ID_RE, "sourceId");
    nonEmptyString(record.name, "MCP name");
    if (record.name.includes("/") || record.name.includes("\\")) fail("bundle_schema_invalid", "MCP name must be a single segment");
    validateSourcePath(record.rootPath, "rootPath");
    validateSourcePath(record.entryPath, "entryPath");
    if (record.runtime !== "python") fail("bundle_schema_invalid", "MCP runtime must be python");
    if (record.transport !== "stdio") fail("bundle_schema_invalid", "MCP transport must be stdio");
    stringArray(record.arguments, "arguments");
    stringArray(record.environment, "environment");
    for (const variable of record.environment) {
        if (!variable || variable !== variable.trim() || variable.includes("\0") || variable.includes("=") || /\s/.test(variable)) {
            fail("bundle_schema_invalid", "MCP environment entries must be bare variable names");
        }
    }
    if (record.mcpId !== bundleMcpId(record.sourceId, record.name, record.rootPath)) fail("identity_mismatch", "mcpId identity does not match sourceId, name, and rootPath");
    return record;
};

const validateFileRecord = (record) => {
    exactKeys(record, ["fileId", "gitMode", "mediaType", "owners", "packagePath", "sha256", "sizeBytes", "sourceId", "sourcePath"], "bundle file record");
    match(record.fileId, FILE_ID_RE, "fileId");
    match(record.sourceId, SOURCE_ID_RE, "sourceId");
    validateSourcePath(record.sourcePath, "sourcePath");
    sha256String(record.sha256, "file sha256");
    integer(record.sizeBytes, "sizeBytes", 0);
    if (record.gitMode !== "100644" && record.gitMode !== "100755") fail("bundle_schema_invalid", "git mode must be 100644 or 100755");
    nonEmptyString(record.mediaType, "media type");
    if (!Array.isArray(record.owners) || !record.owners.length) fail("bundle_schema_invalid", "bundle file owners must not be empty");
    for (const owner of record.owners) {
        exactKeys(owner, ["id", "kind"], "bundle file owner");
        if (!["skill", "mcp", "skill-tool"].includes(owner.kind)) fail("bundle_schema_invalid", "bundle file owner kind is invalid");
        const prefix = owner.kind === "mcp" ? "mcp_" : "sk_";
        const body = String(owner.id).startsWith(prefix) ? String(owner.id).slice(prefix.length) : "";
        if (!String(owner.id).startsWith(prefix) || body.length !== 64 || !/^[0-9a-f]{64}$/.test(body)) {
            fail("owner_unknown", "bundle file owner ID does not match its kind");
        }
    }
    if (record.fileId !== bundleFileId(record.sourceId, record.sourcePath)) fail("identity_mismatch", "fileId identity does not match sourceId and sourcePath");
    if (record.packagePath !== `${PACKAGE_V5_PAYLOAD_PREFIX}${record.sourceId}/${record.sourcePath}`) {
        fail("closure_gap", "packagePath must derive from sourceId and sourcePath");
    }
    return record;
};

const validateSkipRecord = (record) => {
    exactKeys(record, ["id", "kind", "reasonCode", "reasonPath", "rootPath", "sourceId"], "bundle skip record");
    if (!["skill", "mcp", "skill-tool"].includes(record.kind)) fail("bundle_schema_invalid", "skip kind is invalid");
    nonEmptyString(record.id, "skip id");
    match(record.sourceId, SOURCE_ID_RE, "sourceId");
    validateSourcePath(record.rootPath, "rootPath");
    validateSourcePath(record.reasonPath, "reasonPath");
    nonEmptyString(record.reasonCode, "reason code");
    const prefix = record.kind === "mcp" ? "mcp_" : "sk_";
    if (!String(record.id).startsWith(prefix)) fail("bundle_schema_invalid", "skip record ID does not match its kind");
    return record;
};

const parseRecordLines = (input, label) => {
    const text = Buffer.isBuffer(input) ? input.toString("utf8") : String(input ?? "");
    return parseCanonicalJsonl(text, label);
};

const requirePayload = (filePaths, sourceId, sourcePath, label) => {
    if (!filePaths.has(`${PACKAGE_V5_PAYLOAD_PREFIX}${sourceId}/${sourcePath}`)) {
        fail("closure_gap", `${label} has no Bundle payload: ${sourcePath}`);
    }
};

export const referencedByIncluded = (record, skillsById, mcpsById) => {
    for (const owner of record.owners) {

        if (owner.kind === "skill") {
            const skill = skillsById.get(owner.id);
            if (skill && (record.sourcePath === skill.entryPath || skill.toolPaths.includes(record.sourcePath))) return true;
        } else if (owner.kind === "mcp") {
            const mcp = mcpsById.get(owner.id);
            if (mcp && record.sourcePath === mcp.entryPath) return true;
        } else if (owner.kind === "skill-tool") {
            // Provenance already guarantees skill-tool owners reference known
            // skills, so reaching here means the file is claimed; no extra check.
            return true;
        }
    }
    return false;
};

const totalBytes = (payload) => Object.values(payload).reduce((total, data) => total + data.length, 0);

const assertSortedIds = (values, label) => {
    const sorted = [...values].sort(compareCodePoints);
    if (values.some((value, index) => value !== sorted[index])) fail("bundle_records_unsorted", `${label} records are not sorted`);
};

const assertUnique = (values, label) => {
    if (new Set(values).size !== values.length) fail("bundle_records_duplicate", `duplicate ${label}`);
};

const validateSourcePath = (value, label) => {
    if (typeof value !== "string" || !value || value.startsWith("/") || value.endsWith("/") || value.includes("\\") || value.includes("\0")
        || value.split("/").some((part) => !part || part === "." || part === "..")) {
        fail("bundle_schema_invalid", `${label} must be a normalized relative POSIX path`);
    }
};

const exactKeys = (value, keys, label) => {
    object(value, label);
    const actual = strictObjectKeys(value);
    const expected = [...keys].sort(compareCodePoints);
    const ordered = [...actual].sort(compareCodePoints);
    if (actual.length !== expected.length || ordered.some((key, index) => key !== expected[index])) {
        fail("bundle_schema_invalid", `${label} fields do not match the Package v5 contract`);
    }
};

const object = (value, label) => {
    if (!value || typeof value !== "object" || Array.isArray(value)) fail("bundle_schema_invalid", `${label} must be an object`);
};
const match = (value, expression, label) => {
    if (typeof value !== "string" || !expression.test(value)) fail("bundle_schema_invalid", `${label} has an invalid value`);
};
const nonEmptyString = (value, label) => {
    if (typeof value !== "string" || !value || value !== value.trim() || value.includes("\0")) fail("bundle_schema_invalid", `${label} must be a non-empty trimmed string`);
};
const nonEmptyText = (value, label) => {
    if (typeof value !== "string" || !value || value.includes("\0")) fail("bundle_schema_invalid", `${label} must be non-empty and NUL-free`);
};
const sha256String = (value, label) => match(value, SHA256_RE, label);
const integer = (value, label, min) => {
    if (!Number.isSafeInteger(value) || value < min) fail("bundle_schema_invalid", `${label} must be an integer in range`);
};
const stringArray = (value, label) => {
    if (!Array.isArray(value)) fail("bundle_schema_invalid", `${label} must be an array`);
    for (const item of value) {
        if (typeof item !== "string") fail("bundle_schema_invalid", `${label} must contain strings`);
    }
};
const compareCodePoints = (left, right) => (left < right ? -1 : left > right ? 1 : 0);
const sha256 = (value) => createHash("sha256").update(value).digest("hex");
const fail = (code, message) => { throw new BundleContractError(code, message); };
