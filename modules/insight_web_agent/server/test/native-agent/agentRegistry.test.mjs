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
import { createAgentRegistry } from "../../native-agent/agents/agentRegistry.mjs";

const agentMarkdown = ({ name, description = "Agent", mode = "all", body = "Instructions", permission = '"*": ask' } = {}) => `---\n${name ? `name: ${name}\n` : ""}description: ${description}\nmode: ${mode}\npermission:\n  bash:\n    ${permission}\n---\n\n${body}\n`;

const createFixture = async () => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-agent-registry-"));
    const bundled = join(root, "bundled");
    const development = join(root, "development");
    await Promise.all([mkdir(bundled), mkdir(development)]);
    return { root, bundled, development };
};

test("registry requires a valid bundled general even when development supplies one", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    await writeFile(join(fixture.development, "general.md"), agentMarkdown({ description: "Development general" }), "utf8");
    const registry = createAgentRegistry({ bundledDir: fixture.bundled, developmentDirs: [fixture.development] });

    await assert.rejects(registry.initialize(), /required bundled Primary Agent is unavailable/);
});

test("development agents override bundled definitions and preserve diagnostics", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    await writeFile(join(fixture.bundled, "general.md"), agentMarkdown({ description: "Bundled general" }), "utf8");
    await writeFile(join(fixture.development, "general.md"), agentMarkdown({ description: "Development general", body: "Development instructions" }), "utf8");
    const registry = createAgentRegistry({ bundledDir: fixture.bundled, developmentDirs: [fixture.development] });

    await registry.initialize();

    assert.equal(registry.getPrimary("general").description, "Development general");
    assert.equal(registry.getPrimary("general").source.kind, "development");
    assert.equal(registry.diagnostics().some(({ code }) => code === "AGENT_OVERRIDDEN"), true);
});

test("development override cannot make required general a subagent", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    await writeFile(join(fixture.bundled, "general.md"), agentMarkdown(), "utf8");
    await writeFile(join(fixture.development, "general.md"), agentMarkdown({ mode: "subagent" }), "utf8");
    const registry = createAgentRegistry({ bundledDir: fixture.bundled, developmentDirs: [fixture.development] });

    await assert.rejects(registry.initialize(), /effective Primary Agent is unavailable/);
});
