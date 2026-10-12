/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { cp, lstat, mkdir, readdir, realpath, rm, symlink } from "node:fs/promises";
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

    const skills = (await readdir(sourceRoot, { withFileTypes: true }))
        .filter((entry) => entry.isDirectory())
        .sort((left, right) => left.name.localeCompare(right.name));
    for (const skill of skills) {
        const target = join(targetRoot, skill.name);
        await rm(target, { recursive: true, force: true });
        await cp(join(sourceRoot, skill.name), target, { recursive: true });
    }

    return { workspaceRoot, targetRoot, installed: skills.map(({ name }) => name) };
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
        if (error.code === "ENOENT") return undefined;
        throw error;
    }
};

const isInside = (root, target) => {
    const relation = relative(root, target);
    return !relation || (!relation.startsWith("..") && !isAbsolute(relation));
};
