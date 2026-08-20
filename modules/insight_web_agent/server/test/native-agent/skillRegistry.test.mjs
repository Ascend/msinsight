/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { mkdtemp, mkdir, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { createSkillRegistry } from "../../native-agent/skills/skillRegistry.mjs";
import { createSkillTools } from "../../native-agent/tools/skillTools.mjs";

const skillMarkdown = (instructions, extra = "") => `---\nname: inspect-memory\ndescription: Inspect memory data\n${extra}---\n\n${instructions}\n`;

test("Skill metadata is discovered eagerly while instructions load on Tool invocation", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-skill-registry-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const directory = join(root, "inspect-memory");
    const filePath = join(directory, "SKILL.md");
    await mkdir(directory);
    await writeFile(filePath, skillMarkdown("First instructions", "model: ignored\n"), "utf8");
    const registry = createSkillRegistry({ bundledDir: root });
    await registry.initialize();

    assert.equal(registry.list()[0].instructions, undefined);
    assert.equal(registry.list()[0].path, undefined);
    assert.equal(registry.get("inspect-memory").instructions, undefined);
    assert.equal(registry.diagnostics().some(({ code }) => code === "SKILL_RUNTIME_EFFECT_IGNORED"), true);

    await writeFile(filePath, skillMarkdown("Updated instructions with !`echo disabled`"), "utf8");
    const skillTool = createSkillTools({ skillRegistry: registry })[0];
    const output = await skillTool.execute({ name: "inspect-memory", args: "focus=peak" });

    assert.match(output, /Invocation Arguments: focus=peak/);
    assert.match(output, /Updated instructions with !`echo disabled`/);
    assert.doesNotMatch(output, /First instructions/);
});
