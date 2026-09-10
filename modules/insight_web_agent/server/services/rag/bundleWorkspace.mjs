/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { chmod, lstat, mkdir, mkdtemp, readdir, realpath, rm, writeFile } from "node:fs/promises";
import { cp } from "node:fs/promises";
import { tmpdir } from "node:os";
import { isAbsolute, join, resolve } from "node:path";
import { syncPackagedSkills } from "../workspaceSkillSync.mjs";
import { PACKAGE_V5_PAYLOAD_PREFIX } from "./wire/packageBundleContracts.mjs";

const SKILLS_DIR = [".agents", "skills"];
const TOOLS_DIR = [".agents", "tools"];
const MCPS_DIR = [".agents", "mcps"];
const WINDOWS_MAX_PATH_CHARS = 259;

export class BundleWorkspaceError extends Error {
    constructor(code, message) {
        super(message);
        this.name = "BundleWorkspaceError";
        this.code = code;
    }
}

/**
 * Rebuild validated Bundle records into workspace-shaped file trees.
 *
 * Input records must already pass the wire validator and closure checks;
 * staging re-verifies digests against member bytes and enforces workspace
 * safety (relative paths, no escapes, no case collisions). It never executes
 * payload bytes: files are treated as opaque buffers.
 */
export const stageBundleForWorkspace = async ({ skills, mcps, files, skips, payload }) => {
    const records = new Map(files.map((record) => [record.packagePath, record]));
    for (const record of files) {
        if (record.packagePath !== `${PACKAGE_V5_PAYLOAD_PREFIX}${record.sourceId}/${record.sourcePath}`) {
            fail("workspace_path_unsafe", `Bundle packagePath does not derive from source identity: ${record.packagePath}`);
        }
        assertWorkspaceRelativePath(record.sourcePath, record.packagePath);
    }
    for (const [name, data] of Object.entries(payload)) {
        const record = records.get(name);
        if (!record) continue;
        if (!Buffer.isBuffer(data)) fail("payload_invalid", `Bundle payload is not bytes: ${name}`);
        if (createHash("sha256").update(data).digest("hex") !== record.sha256 || data.length !== record.sizeBytes) {
            fail("payload_mismatch", `Bundle payload bytes do not match its file record: ${name}`);
        }
    }
    const skillsByName = new Map();
    const skillFiles = new Map();
    const toolFiles = new Map();
    const toolConflicts = [];
    const overridden = detectOverriddenSkills(skills);
    for (const skill of [...skills].sort(compareSkillOrder)) {
        // Skill directories carry files owned in the skill role; declared
        // tools live once in the shared source-relative tools tree below.
        const owned = files.filter(({ owners }) => owners.some(({ kind, id }) => kind === "skill" && id === skill.skillId));
        const tree = new Map();
        for (const record of owned) {
            const relativePath = relativeToRoot(record, skill.rootPath);
            assertWorkspaceRelativePath(relativePath, record.packagePath);
            tree.set(relativePath, {
                data: payload[record.packagePath],
                sha256: record.sha256,
                sizeBytes: record.sizeBytes,
                gitMode: record.gitMode,
                packagePath: record.packagePath,
            });
        }
        skillsByName.set(skill.name, skill);
        skillFiles.set(skill.name, tree);
    }
    for (const record of files) {
        if (!record.owners.some(({ kind }) => kind === "skill-tool")) continue;
        assertWorkspaceRelativePath(record.sourcePath, record.packagePath);
        const entry = {
            data: payload[record.packagePath],
            sha256: record.sha256,
            sizeBytes: record.sizeBytes,
            gitMode: record.gitMode,
            packagePath: record.packagePath,
            owningSkills: [],
        };
        const previous = toolFiles.get(record.sourcePath);
        if (previous && (previous.sha256 !== entry.sha256 || previous.sizeBytes !== entry.sizeBytes)) {
            toolConflicts.push(record.sourcePath);
        }
        // Deterministic last-wins by source order; conflicts are recorded below.
        // Owner lists merge across sources so shared tools keep every declarer.
        if (!previous || previous.sha256 !== entry.sha256 || previous.sizeBytes !== entry.sizeBytes) {
            toolFiles.set(record.sourcePath, { ...entry, owningSkills: previous ? [...previous.owningSkills] : [] });
        }
        toolFiles.get(record.sourcePath).owningSkills.push(
            ...record.owners.filter(({ kind }) => kind === "skill-tool").map(({ id }) => id),
        );
    }
    const mcpsByName = new Map();
    const mcpFiles = new Map();
    for (const mcp of [...mcps].sort((left, right) => (left.mcpId < right.mcpId ? -1 : 1))) {
        const owned = files.filter(({ owners }) => owners.some(({ kind, id }) => kind === "mcp" && id === mcp.mcpId));
        const tree = new Map();
        for (const record of owned) {
            const relativePath = relativeToRoot(record, mcp.rootPath);
            assertWorkspaceRelativePath(relativePath, record.packagePath);
            tree.set(relativePath, {
                data: payload[record.packagePath],
                sha256: record.sha256,
                sizeBytes: record.sizeBytes,
                gitMode: record.gitMode,
                packagePath: record.packagePath,
            });
        }
        mcpsByName.set(mcp.name, mcp);
        mcpFiles.set(mcp.name, tree);
    }
    assertNoCaseCollisions([...skillFiles.keys()], "skill");
    assertNoCaseCollisions([...mcpFiles.keys()], "MCP");
    assertNoCaseCollisions([...toolFiles.keys()], "tool");
    for (const [name, tree] of [...skillFiles.entries(), ...mcpFiles.entries()]) {
        assertNoCaseCollisions([...tree.keys()], `files of ${name}`);
    }
    return {
        skills: skillsByName,
        mcps: mcpsByName,
        skillFiles,
        toolFiles,
        toolConflicts: [...new Set(toolConflicts)].sort(),
        mcpFiles,
        skipped: skips.map(({ kind, id, sourceId, rootPath, reasonCode, reasonPath }) => ({ kind, id, sourceId, rootPath, reasonCode, reasonPath })),
        overridden,
    };
};

export const detectOverriddenSkills = (skills) => {
    const seen = new Map();
    const overridden = new Set();
    for (const skill of skills) {
        if (seen.has(skill.name) && seen.get(skill.name) !== skill.skillId) overridden.add(skill.name);
        seen.set(skill.name, skill.skillId);
    }
    return [...overridden].sort();
};

/**
 * Install staged Bundle trees into the agent workspace.
 *
 * Skills reuse the reviewed packaged-skill sync into `.agents/skills`;
 * MCP servers land under `.agents/mcps` with the same safety properties.
 * Executable bits from Bundle git modes are restored off Windows; on
 * Windows the record mode is kept as the audit fact and nothing executes.
 */
export const installBundleToWorkspace = async ({ staged, agentWorkspacePath, platform = process.platform }) => {
    const workspaceRoot = await realpath(resolve(agentWorkspacePath));
    const workspaceInfo = await lstat(workspaceRoot);
    if (!workspaceInfo.isDirectory()) fail("workspace_invalid", "Agent workspace path is not a directory");
    for (const [name, tree] of staged.skillFiles) {
        assertTargetPathLength(join(workspaceRoot, ...SKILLS_DIR, name), platform);
        for (const relativePath of tree.keys()) {
            assertTargetPathLength(join(workspaceRoot, ...SKILLS_DIR, name, ...relativePath.split("/")), platform);
        }
    }
    const skillsSource = await writeStagingTree(staged.skillFiles);
    try {
        await syncPackagedSkills({ sourceSkillsDir: skillsSource, agentWorkspacePath: workspaceRoot });
    } finally {
        await rm(skillsSource, { recursive: true, force: true }).catch(() => {});
    }
    const mcpsRoot = join(workspaceRoot, ...MCPS_DIR);
    await ensureDirectory(mcpsRoot);
    const installedMcps = [];
    for (const name of [...staged.mcps.keys()].sort()) {
        const tree = staged.mcpFiles.get(name) ?? new Map();
        const target = join(mcpsRoot, name);
        await assertNoTargetCollision(mcpsRoot, name);
        assertTargetPathLength(target, platform);
        await rm(target, { recursive: true, force: true });
        await mkdir(target, { recursive: true });
        const entries = [...tree.entries()].sort(([left], [right]) => (left < right ? -1 : 1));
        for (const [relativePath, file] of entries) {
            const destination = join(target, ...relativePath.split("/"));
            assertTargetPathLength(destination, platform);
            await mkdir(join(target, ...relativePath.split("/").slice(0, -1)), { recursive: true });
            await writeFile(destination, file.data, { mode: 0o600 });
            if (platform !== "win32") await chmod(destination, file.gitMode === "100755" ? 0o755 : 0o644);
        }
        installedMcps.push(name);
    }
    const toolsRoot = join(workspaceRoot, ...TOOLS_DIR);
    await ensureDirectory(toolsRoot);
    const installedTools = [];
    for (const [sourcePath, file] of [...staged.toolFiles.entries()].sort(([left], [right]) => (left < right ? -1 : 1))) {
        const destination = join(toolsRoot, ...sourcePath.split("/"));
        assertTargetPathLength(destination, platform);
        await mkdir(join(toolsRoot, ...sourcePath.split("/").slice(0, -1)), { recursive: true });
        await writeFile(destination, file.data, { mode: 0o600 });
        if (platform !== "win32") await chmod(destination, file.gitMode === "100755" ? 0o755 : 0o644);
        installedTools.push(sourcePath);
    }
    if (platform !== "win32") {
        for (const [name, tree] of staged.skillFiles) {
            for (const [relativePath, file] of tree) {
                if (file.gitMode !== "100755") continue;
                await chmod(join(workspaceRoot, ...SKILLS_DIR, name, ...relativePath.split("/")), 0o755);
            }
        }
    }
    const skillsRoot = join(workspaceRoot, ...SKILLS_DIR);
    return {
        skills: [...staged.skills.keys()],
        mcps: installedMcps,
        tools: installedTools,
        skipped: staged.skipped,
        overridden: staged.overridden,
        toolConflicts: staged.toolConflicts,
        registry: buildRegistry(staged, workspaceRoot, skillsRoot, toolsRoot, mcpsRoot),
    };
};

const buildRegistry = (staged, workspaceRoot, skillsRoot, toolsRoot, mcpsRoot) => ({
    skills: [...staged.skills.values()].map((skill) => ({
        name: skill.name,
        sourceId: skill.sourceId,
        rootPath: skill.rootPath,
        entryPath: skill.entryPath,
        description: skill.description,
        workspacePath: join(skillsRoot, skill.name),
        workspaceRoot,
    })),
    tools: [...staged.toolFiles.entries()].map(([sourcePath, file]) => ({
        path: sourcePath,
        ownerSkillIds: [...new Set(file.owningSkills)].sort(),
        sha256: file.sha256,
        sizeBytes: file.sizeBytes,
        workspacePath: join(toolsRoot, ...sourcePath.split("/")),
        workspaceRoot,
    })),
    mcps: [...staged.mcps.values()].map((mcp) => ({
        name: mcp.name,
        sourceId: mcp.sourceId,
        rootPath: mcp.rootPath,
        entry: {
            path: mcp.entryPath,
            workspacePath: join(mcpsRoot, mcp.name, ...relativePosix(mcp.entryPath, mcp.rootPath).split("/")),
        },
        runtime: mcp.runtime,
        transport: mcp.transport,
        arguments: [...mcp.arguments],
        environment: [...mcp.environment],
        workspacePath: join(mcpsRoot, mcp.name),
        workspaceRoot,
    })),
    skipped: staged.skipped,
    overridden: staged.overridden,
});

const compareSkillOrder = (left, right) => {
    if (left.sourceId !== right.sourceId) return left.sourceId < right.sourceId ? -1 : 1;
    if (left.rootPath !== right.rootPath) return left.rootPath < right.rootPath ? -1 : 1;
    return 0;
};

const relativeToRoot = (record, root) => {
    const prefix = `${root}/`;
    if (record.sourcePath === root || !record.sourcePath.startsWith(prefix)) {
        fail("workspace_layout_invalid", `Bundle file is outside its component root: ${record.packagePath}`);
    }
    return record.sourcePath.slice(prefix.length);
};

const relativePosix = (path, root) => {
    const prefix = `${root}/`;
    if (!path.startsWith(prefix)) fail("workspace_layout_invalid", `MCP entry is outside its root: ${path}`);
    return path.slice(prefix.length);
};

const assertWorkspaceRelativePath = (relativePath, packagePath) => {
    if (!relativePath || isAbsolute(relativePath) || relativePath.includes("\\") || relativePath.includes("\0")
        || relativePath.split("/").some((part) => !part || part === "." || part === "..")) {
        fail("workspace_path_unsafe", `Bundle workspace path is unsafe: ${packagePath}`);
    }
};

const assertNoCaseCollisions = (names, label) => {
    const folded = new Map();
    for (const name of names) {
        const key = name.toLowerCase();
        if (folded.has(key)) fail("workspace_case_collision", `Workspace ${label} names collide without case: ${folded.get(key)} <> ${name}`);
        folded.set(key, name);
    }
};

const assertNoTargetCollision = async (parent, name) => {
    const entries = await readdir(parent);
    const folded = name.toLowerCase();
    for (const entry of entries) {
        if (entry !== name && entry.toLowerCase() === folded) {
            fail("workspace_case_collision", `Workspace target collides without case: ${entry} <> ${name}`);
        }
    }
};

const assertTargetPathLength = (target, platform) => {
    if (platform === "win32" && target.length > WINDOWS_MAX_PATH_CHARS) {
        fail("workspace_path_too_long", `Workspace target exceeds the Windows path limit: ${target.length} chars`);
    }
};

const ensureDirectory = async (path) => {
    await mkdir(path, { recursive: true });
    const info = await lstat(path);
    if (info.isSymbolicLink() || !info.isDirectory()) fail("workspace_invalid", `Workspace path is not a regular directory: ${path}`);
};

const writeStagingTree = async (trees) => {
    const staging = await mkdtemp(join(tmpdir(), "msinsight-bundle-skills-"));
    try {
        for (const [name, tree] of trees) {
            assertWorkspaceRelativePath(name, name);
            const root = join(staging, name);
            await mkdir(root, { recursive: true });
            for (const [relativePath, file] of tree) {
                const destination = join(root, ...relativePath.split("/"));
                await mkdir(join(root, ...relativePath.split("/").slice(0, -1)), { recursive: true });
                await writeFile(destination, file.data, { mode: 0o600 });
            }
        }
        return staging;
    } catch (error) {
        await rm(staging, { recursive: true, force: true }).catch(() => {});
        throw error;
    }
};

const fail = (code, message) => { throw new BundleWorkspaceError(code, message); };
