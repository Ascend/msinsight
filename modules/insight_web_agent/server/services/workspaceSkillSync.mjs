/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { cp, lstat, mkdir, readFile, readdir, realpath, rm, symlink } from "node:fs/promises";
import { isAbsolute, join, relative, resolve } from "node:path";

const SHARED_SKILL_DIRECTORY = [".agents", "skills"];

export const syncPackagedSkills = async ({ sourceSkillsDir, agentWorkspacePath }) => {
    const sourceRoot = await realpath(resolve(sourceSkillsDir));
    const workspaceRoot = await realpath(resolve(agentWorkspacePath));
    if (isInside(sourceRoot, workspaceRoot) || isInside(workspaceRoot, sourceRoot)) {
        throw new Error("Packaged Skill root and Agent workspace must not overlap");
    }

    const agentsDirectory = join(workspaceRoot, ".agents");
    const targetRoot = join(workspaceRoot, ...SHARED_SKILL_DIRECTORY);
    const claudeDirectory = join(workspaceRoot, ".claude");
    await ensureDirectory(agentsDirectory);
    await ensureDirectory(targetRoot);
    await ensureDirectory(claudeDirectory);
    await ensureClaudeSkillLink(claudeDirectory, targetRoot);

    const candidates = (await readdir(sourceRoot, { withFileTypes: true }))
        .filter((entry) => entry.isDirectory())
        .sort((left, right) => left.name.localeCompare(right.name));
    const installed = [];
    const updated = [];
    const skipped = [];
    for (const skill of candidates) {
        const source = join(sourceRoot, skill.name);
        const entry = await optionalLstat(join(source, "SKILL.md"));
        if (!entry?.isFile() || entry.isSymbolicLink()) continue;
        installed.push(skill.name);
        const target = join(targetRoot, skill.name);
        if (await sameSkillTree(source, target)) {
            skipped.push(skill.name);
            continue;
        }
        // Only replace a product-managed name. rm unlinks target symlinks;
        // neither comparison nor copying may follow them outside the workspace.
        await rm(target, { recursive: true, force: true });
        await cp(source, target, { recursive: true, verbatimSymlinks: true });
        updated.push(skill.name);
    }

    return { workspaceRoot, targetRoot, installed, updated, skipped };
};

const sameSkillTree = async (source, target) => {
    const sourceInfo = await lstat(source);
    if (sourceInfo.isSymbolicLink()) throw new Error("Packaged Skill must not contain symbolic links");
    const targetInfo = target ? await optionalLstat(target) : undefined;
    if (sourceInfo.isDirectory()) {
        const names = (await readdir(source)).sort();
        // Validate the source even when the destination is missing, so an
        // unsafe source cannot slip through the first-install copy path.
        const comparable = targetInfo?.isDirectory() && !targetInfo.isSymbolicLink();
        const targetNames = comparable ? (await readdir(target)).sort() : [];
        let same = Boolean(comparable) && names.length === targetNames.length
            && names.every((name, index) => name === targetNames[index]);
        for (const name of names) {
            if (!await sameSkillTree(join(source, name), comparable ? join(target, name) : undefined)) same = false;
        }
        return same;
    }
    if (!sourceInfo.isFile()) throw new Error("Packaged Skill must contain only regular files and directories");
    if (!targetInfo?.isFile() || targetInfo.isSymbolicLink() || sourceInfo.size !== targetInfo.size) return false;
    if (process.platform !== "win32" && (sourceInfo.mode & 0o111) !== (targetInfo.mode & 0o111)) return false;
    const [sourceBytes, targetBytes] = await Promise.all([readFile(source), readFile(target)]);
    return sourceBytes.equals(targetBytes);
};

const ensureDirectory = async (path) => {
    await mkdir(path).catch((error) => {
        if (error.code !== "EEXIST") throw error;
    });
    const info = await lstat(path);
    if (info.isSymbolicLink() || !info.isDirectory()) {
        throw new Error(`Workspace path is not a regular directory: ${path}`);
    }
};

const ensureClaudeSkillLink = async (claudeDirectory, targetRoot) => {
    const linkPath = join(claudeDirectory, "skills");
    const existing = await optionalLstat(linkPath);
    if (!existing) {
        const target = process.platform === "win32" ? targetRoot : relative(claudeDirectory, targetRoot);
        await symlink(target, linkPath, process.platform === "win32" ? "junction" : "dir");
        return;
    }
    if (!existing.isSymbolicLink()) {
        throw new Error(`Claude Skill path is not a directory link: ${linkPath}`);
    }
    if (await realpath(linkPath) !== await realpath(targetRoot)) {
        throw new Error(`Claude Skill link does not target shared Skills: ${linkPath}`);
    }
};

const optionalLstat = async (path) => {
    try {
        return await lstat(path);
    } catch (error) {
        if (["ENOENT", "ENOTDIR"].includes(error.code)) return undefined;
        throw error;
    }
};

const isInside = (root, target) => {
    const relation = relative(root, target);
    return !relation || (!relation.startsWith("..") && !isAbsolute(relation));
};
