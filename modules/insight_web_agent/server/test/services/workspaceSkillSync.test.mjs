/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { lstat, mkdtemp, mkdir, readFile, realpath, rm, symlink, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { syncPackagedSkills } from "../../services/workspaceSkillSync.mjs";

const createFixture = async (t) => {
    const root = await mkdtemp(join(tmpdir(), "insight-workspace-skills-"));
    const sourceSkillsDir = join(root, "packaged-skills");
    const agentWorkspacePath = join(root, "workspace");
    await mkdir(sourceSkillsDir);
    await mkdir(agentWorkspacePath);
    t.after(() => rm(root, { recursive: true, force: true }));
    return { root, sourceSkillsDir, agentWorkspacePath };
};

const writeSkill = async (skillsRoot, name, instructions) => {
    const directory = join(skillsRoot, name);
    await mkdir(join(directory, "references"), { recursive: true });
    await writeFile(join(directory, "SKILL.md"), instructions, "utf8");
    await writeFile(join(directory, "references", "guide.txt"), instructions, "utf8");
};

test("syncPackagedSkills copies packaged Skills and preserves other workspace Skills", async (t) => {
    const fixture = await createFixture(t);
    await writeSkill(fixture.sourceSkillsDir, "inspect-memory", "new instructions");
    const sharedRoot = join(fixture.agentWorkspacePath, ".agents", "skills");
    await writeSkill(sharedRoot, "inspect-memory", "old instructions");
    await writeSkill(sharedRoot, "user-skill", "user instructions");

    const result = await syncPackagedSkills(fixture);

    assert.deepEqual(result.installed, ["inspect-memory"]);
    assert.equal(await readFile(join(sharedRoot, "inspect-memory", "SKILL.md"), "utf8"), "new instructions");
    assert.equal(await readFile(join(sharedRoot, "inspect-memory", "references", "guide.txt"), "utf8"), "new instructions");
    assert.equal(await readFile(join(sharedRoot, "user-skill", "SKILL.md"), "utf8"), "user instructions");
});

test("syncPackagedSkills creates and reuses the Claude Skill link", async (t) => {
    const fixture = await createFixture(t);
    await writeSkill(fixture.sourceSkillsDir, "inspect-memory", "instructions");

    await syncPackagedSkills(fixture);
    await syncPackagedSkills(fixture);

    const sharedRoot = join(fixture.agentWorkspacePath, ".agents", "skills");
    const claudeRoot = join(fixture.agentWorkspacePath, ".claude", "skills");
    assert.equal((await lstat(claudeRoot)).isSymbolicLink(), true);
    assert.equal(await realpath(claudeRoot), await realpath(sharedRoot));
    assert.equal(await readFile(join(claudeRoot, "inspect-memory", "SKILL.md"), "utf8"), "instructions");
});

test("syncPackagedSkills rejects a conflicting Claude Skill path", async (t) => {
    const fixture = await createFixture(t);
    await writeSkill(fixture.sourceSkillsDir, "inspect-memory", "instructions");
    await mkdir(join(fixture.agentWorkspacePath, ".claude", "skills"), { recursive: true });

    await assert.rejects(syncPackagedSkills(fixture), /Claude Skill path is not a directory link/);
});

test("syncPackagedSkills rejects a Claude Skill link to another directory", async (t) => {
    const fixture = await createFixture(t);
    const claudeDirectory = join(fixture.agentWorkspacePath, ".claude");
    const externalDirectory = join(fixture.root, "external-skills");
    await mkdir(claudeDirectory);
    await mkdir(externalDirectory);
    try {
        await symlink(externalDirectory, join(claudeDirectory, "skills"), process.platform === "win32" ? "junction" : "dir");
    } catch (error) {
        if (["EPERM", "EACCES", "ENOSYS"].includes(error.code)) {
            t.skip(`directory link unavailable: ${error.code}`);
            return;
        }
        throw error;
    }

    await assert.rejects(syncPackagedSkills(fixture), /does not target shared Skills/);
});

test("syncPackagedSkills rejects overlapping source and workspace roots", async (t) => {
    const fixture = await createFixture(t);
    const nestedWorkspace = join(fixture.sourceSkillsDir, "workspace");
    await mkdir(nestedWorkspace);

    await assert.rejects(syncPackagedSkills({
        sourceSkillsDir: fixture.sourceSkillsDir,
        agentWorkspacePath: nestedWorkspace,
    }), /must not overlap/);
});
