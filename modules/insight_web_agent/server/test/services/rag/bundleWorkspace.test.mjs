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
import { mkdtemp, readFile, readdir, stat } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import {
    installBundleToWorkspace,
    stageBundleForWorkspace,
} from "../../../services/rag/bundleWorkspace.mjs";
import {
    bundleFileId,
    bundleMcpId,
    bundleSkillId,
} from "../../../services/rag/wire/packageBundleContracts.mjs";

const sha256hex = (text) => createHash("sha256").update(text, "utf8").digest("hex");

const SOURCE_ID = "group/repo";
const skillId = bundleSkillId(SOURCE_ID, "skills/demo");
const mcpId = bundleMcpId(SOURCE_ID, "svc", "app/tools/mcp_servers");

const contents = {
    "skills/demo/SKILL.md": "# Demo skill.",
    "skills/demo/notes.md": "# Notes",
    "app/tools/mcp_servers/svc.py": "# svc",
    "tools/run.sh": "#!/bin/sh\necho demo",
};

const skill = {
    skillId,
    sourceId: SOURCE_ID,
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
    sourceId: SOURCE_ID,
    name: "svc",
    rootPath: "app/tools/mcp_servers",
    entryPath: "app/tools/mcp_servers/svc.py",
    runtime: "python",
    transport: "stdio",
    arguments: ["-m", "svc"],
    environment: ["SVC_HOME"],
};
const owners = {
    "skills/demo/SKILL.md": [{ kind: "skill", id: skillId }],
    "skills/demo/notes.md": [{ kind: "skill", id: skillId }],
    "app/tools/mcp_servers/svc.py": [{ kind: "mcp", id: mcpId }],
    "tools/run.sh": [{ kind: "skill-tool", id: skillId }],
};
const files = Object.entries(contents)
    .map(([sourcePath, text]) => ({
        fileId: bundleFileId(SOURCE_ID, sourcePath),
        sourceId: SOURCE_ID,
        sourcePath,
        packagePath: `bundles/${SOURCE_ID}/${sourcePath}`,
        sha256: sha256hex(text),
        sizeBytes: Buffer.byteLength(text),
        gitMode: sourcePath.endsWith(".sh") ? "100755" : "100644",
        mediaType: "text/plain",
        owners: owners[sourcePath],
    }))
    .sort((left, right) => (left.fileId < right.fileId ? -1 : 1));
const skip = {
    id: bundleSkillId(SOURCE_ID, "skills/other"),
    kind: "skill",
    sourceId: SOURCE_ID,
    rootPath: "skills/other",
    reasonCode: "SKILL_FRONTMATTER_INVALID",
    reasonPath: "skills/other/SKILL.md",
};
const payload = Object.fromEntries(
    Object.entries(contents).map(([sourcePath, text]) => [
        `bundles/${SOURCE_ID}/${sourcePath}`,
        Buffer.from(text, "utf8"),
    ]),
);

const stage = () => stageBundleForWorkspace({
    skills: [skill],
    mcps: [mcp],
    files,
    skips: [skip],
    payload,
});

test("staging rebuilds skill directories and mcp trees from records only", async () => {
    const staged = await stage();

    assert.deepEqual([...staged.skills.keys()], ["demo"]);
    assert.deepEqual([...staged.mcps.keys()], ["svc"]);
    assert.equal(staged.skipped.length, 1);
    assert.equal(staged.skipped[0].reasonCode, "SKILL_FRONTMATTER_INVALID");
    assert.deepEqual([...staged.skillFiles.get("demo").keys()].sort(), ["SKILL.md", "notes.md"]);
    assert.deepEqual([...staged.mcpFiles.get("svc").keys()].sort(), ["svc.py"]);
    assert.equal(
        staged.skillFiles.get("demo").get("SKILL.md").sha256,
        sha256hex(contents["skills/demo/SKILL.md"]),
    );
    assert.equal(staged.overridden.length, 0);
});

test("staging rejects digest drift and unsafe package paths", async () => {
    const drifted = {
        ...payload,
        [`bundles/${SOURCE_ID}/skills/demo/notes.md`]: Buffer.from("tampered"),
    };
    await assert.rejects(stageBundleForWorkspace({
        skills: [skill],
        mcps: [mcp],
        files,
        skips: [skip],
        payload: drifted,
    }), /digest|mismatch|match/);

    const evil = files.map((record) => ({ ...record }));
    evil[0] = { ...evil[0], packagePath: "bundles/group/repo/../escape.txt" };
    await assert.rejects(stageBundleForWorkspace({
        skills: [skill],
        mcps: [mcp],
        files: evil,
        skips: [skip],
        payload,
    }), /unsafe|escape|path/);
});

test("staging fails closed on workspace case collisions", async () => {
    const otherSkill = {
        ...skill,
        skillId: bundleSkillId(SOURCE_ID, "skills/DEMO"),
        rootPath: "skills/DEMO",
        entryPath: "skills/DEMO/SKILL.md",
        name: "DEMO",
    };
    const otherText = "# Other";
    const otherFile = {
        fileId: bundleFileId(SOURCE_ID, "skills/DEMO/SKILL.md"),
        sourceId: SOURCE_ID,
        sourcePath: "skills/DEMO/SKILL.md",
        packagePath: `bundles/${SOURCE_ID}/skills/DEMO/SKILL.md`,
        sha256: sha256hex(otherText),
        sizeBytes: Buffer.byteLength(otherText),
        gitMode: "100644",
        mediaType: "text/markdown",
        owners: [{ kind: "skill", id: otherSkill.skillId }],
    };
    const otherPayload = {
        ...payload,
        [`bundles/${SOURCE_ID}/skills/DEMO/SKILL.md`]: Buffer.from(otherText),
    };
    await assert.rejects(stageBundleForWorkspace({
        skills: [skill, otherSkill].sort((a, b) => (a.skillId < b.skillId ? -1 : 1)),
        mcps: [mcp],
        files: [...files, otherFile].sort((a, b) => (a.fileId < b.fileId ? -1 : 1)),
        skips: [skip],
        payload: otherPayload,
    }), /collision|case/i);
});

test("install copies skills and mcps into the workspace without executing payload", async () => {
    const staged = await stage();
    const workspace = await mkdtemp(join(tmpdir(), "msinsight-workspace-"));
    const canary = { ...payload, [`bundles/${SOURCE_ID}/tools/evil.js`]: Buffer.from("throw new Error('executed');") };

    const installed = await installBundleToWorkspace({
        staged,
        payload: canary,
        agentWorkspacePath: workspace,
        platform: process.platform,
    });

    assert.deepEqual(installed.skills, ["demo"]);
    assert.deepEqual(installed.mcps, ["svc"]);
    assert.deepEqual(installed.tools, ["tools/run.sh"]);
    assert.equal(installed.skipped.length, 1);
    assert.equal(
        await readFile(join(workspace, ".agents", "skills", "demo", "SKILL.md"), "utf8"),
        contents["skills/demo/SKILL.md"],
    );
    assert.equal(
        await readFile(join(workspace, ".agents", "mcps", "svc", "svc.py"), "utf8"),
        contents["app/tools/mcp_servers/svc.py"],
    );
    assert.ok(installed.registry.skills[0].workspacePath.endsWith(join(".agents", "skills", "demo")));
    assert.ok(installed.registry.mcps[0].entry.workspacePath.endsWith(join(".agents", "mcps", "svc", "svc.py")));
    assert.deepEqual(installed.registry.mcps[0].environment, ["SVC_HOME"]);
    assert.ok(!installed.registry.mcps[0].environment.some((entry) => entry.includes("=")));
    assert.ok(installed.registry.tools[0].workspacePath.endsWith(join(".agents", "tools", "tools", "run.sh")));
    assert.deepEqual(installed.registry.tools[0].ownerSkillIds, [skillId]);
    const runMode = (await stat(join(workspace, ".agents", "tools", "tools", "run.sh"))).mode & 0o777;
    if (process.platform === "win32") {
        assert.equal(staged.toolFiles.get("tools/run.sh").gitMode, "100755");
    } else {
        assert.equal(runMode, 0o755);
    }
});

test("install rejects workspace targets beyond the Windows path limit", async () => {
    const staged = await stage();
    const workspace = await mkdtemp(join(tmpdir(), "msinsight-workspace-"));
    const deep = "a".repeat(300);
    const longStaged = {
        ...staged,
        skillFiles: new Map([["demo", new Map([["SKILL.md", staged.skillFiles.get("demo").get("SKILL.md")]]).set(deep, staged.skillFiles.get("demo").get("SKILL.md"))]]),
    };
    await assert.rejects(
        installBundleToWorkspace({ staged: longStaged, agentWorkspacePath: workspace, platform: "win32" }),
        /path limit|too long/,
    );
});

test("empty bundle stages and installs as a no-op", async () => {
    const staged = await stageBundleForWorkspace({ skills: [], mcps: [], files: [], skips: [], payload: {} });
    assert.equal(staged.skills.size, 0);
    const workspace = await mkdtemp(join(tmpdir(), "msinsight-workspace-"));
    const installed = await installBundleToWorkspace({ staged, agentWorkspacePath: workspace, platform: process.platform });
    assert.deepEqual(installed.skills, []);
    assert.deepEqual(installed.mcps, []);
    assert.deepEqual(installed.registry.skills, []);
});

test("duplicate skill names resolve deterministically with override evidence", async () => {    const otherSkill = {
        ...skill,
        skillId: bundleSkillId("group/other", "skills/demo"),
        sourceId: "group/other",
        rootPath: "skills/demo",
        entryPath: "skills/demo/SKILL.md",
    };
    const staged = await stageBundleForWorkspace({
        skills: [skill, otherSkill].sort((a, b) => (a.skillId < b.skillId ? -1 : 1)),
        mcps: [mcp],
        files,
        skips: [skip],
        payload,
    });
    assert.deepEqual(staged.overridden, ["demo"]);
});

test("workspace install covers platforms collisions and invalid roots", async () => {
    const staged = await stageBundleForWorkspace({
        skills: [skill],
        mcps: [mcp],
        files,
        skips: [skip],
        payload,
    });
    const workspace = await mkdtemp(join(tmpdir(), "msinsight-workspace-"));
    const linuxed = await installBundleToWorkspace({
        staged,
        agentWorkspacePath: workspace,
        platform: "linux",
    });
    assert.deepEqual(linuxed.skills, ["demo"]);
    const { stat: statFile, writeFile, mkdir } = await import("node:fs/promises");
    const runMode = (await statFile(join(workspace, ".agents", "tools", "tools", "run.sh"))).mode & 0o777;
    if (process.platform !== "win32") assert.equal(runMode, 0o755);

    const workspace2 = await mkdtemp(join(tmpdir(), "msinsight-workspace-"));
    await mkdir(join(workspace2, ".agents", "mcps", "SVC"), { recursive: true });
    await assert.rejects(
        installBundleToWorkspace({ staged, agentWorkspacePath: workspace2, platform: process.platform }),
        /collision|case/,
    );

    const notDir = join(workspace2, "file-not-dir");
    await writeFile(notDir, "x");
    await assert.rejects(
        installBundleToWorkspace({ staged, agentWorkspacePath: notDir, platform: process.platform }),
        /directory|workspace/,
    );
});

test("staging rejects files outside their component root", async () => {    const skillEntry = files.find((record) => record.sourcePath === "skills/demo/SKILL.md");
    const strayText = "stray";
    const stray = {
        ...skillEntry,
        fileId: bundleFileId(SOURCE_ID, "other/stray.md"),
        sourcePath: "other/stray.md",
        packagePath: `bundles/${SOURCE_ID}/other/stray.md`,
        sha256: sha256hex(strayText),
        sizeBytes: Buffer.byteLength(strayText),
    };
    const others = files.filter((record) => record !== skillEntry);
    const ordered = [...others, stray].sort((left, right) => (left.fileId < right.fileId ? -1 : 1));
    await assert.rejects(
        stageBundleForWorkspace({
            skills: [skill],
            mcps: [mcp],
            files: ordered,
            skips: [skip],
            payload: { ...payload, [stray.packagePath]: Buffer.from(strayText) },
        }),
        /outside|root|layout/,
    );
});

test("staging cleans up after a late write failure", async () => {
    const tooLong = `n${"o".repeat(300)}`;
    const longSkill = { ...skill, name: tooLong };
    const staged = await stageBundleForWorkspace({
        skills: [skill, longSkill].sort((a, b) => (a.skillId < b.skillId ? -1 : 1)),
        mcps: [mcp],
        files,
        skips: [skip],
        payload,
    });
    assert.deepEqual(staged.overridden, []);
    const before = new Set(await readdir(tmpdir()));
    await assert.rejects(
        installBundleToWorkspace({
            staged,
            agentWorkspacePath: await mkdtemp(join(tmpdir(), "msinsight-workspace-")),
            platform: process.platform,
        }),
    );
    const after = new Set(await readdir(tmpdir()));
    assert.deepEqual([...after].filter((entry) => entry.startsWith("msinsight-bundle-skills-") && !before.has(entry)), []);
});

test("tool path conflicts across sources keep both owners", async () => {
    const otherId = bundleSkillId("group/other", "skills/demo");
    const otherText = "different-bytes!!";
    const otherRecord = {
        fileId: bundleFileId("group/other", "tools/run.sh"),
        sourceId: "group/other",
        sourcePath: "tools/run.sh",
        packagePath: "bundles/group/other/tools/run.sh",
        sha256: sha256hex(otherText),
        sizeBytes: Buffer.byteLength(otherText),
        gitMode: "100755",
        mediaType: "text/plain",
        owners: [{ kind: "skill-tool", id: otherId }],
    };
    const otherSkill = {
        ...skill,
        skillId: otherId,
        sourceId: "group/other",
        toolPaths: ["tools/run.sh"],
        requiredMcpIds: [],
        externalRequirements: [],
    };
    const staged = await stageBundleForWorkspace({
        skills: [skill, otherSkill].sort((a, b) => (a.sourceId < b.sourceId ? -1 : 1)),
        mcps: [mcp],
        files: [...files, otherRecord].sort((a, b) => (a.fileId < b.fileId ? -1 : 1)),
        skips: [skip],
        payload: {
            ...payload,
            "bundles/group/other/tools/run.sh": Buffer.from(otherText),
        },
    });
    assert.deepEqual(staged.toolConflicts, ["tools/run.sh"]);
    const tool = staged.toolFiles.get("tools/run.sh");
    assert.equal(tool.sha256, sha256hex(otherText));
    assert.deepEqual([...tool.owningSkills].sort(), [otherId, skillId].sort());
});

test("same-source skills sort by root path deterministically", async () => {
    const second = {
        ...skill,
        skillId: bundleSkillId(SOURCE_ID, "skills/aaa"),
        rootPath: "skills/aaa",
        entryPath: "skills/aaa/SKILL.md",
        name: "aaa",
    };
    const staged = await stageBundleForWorkspace({
        skills: [skill, second],
        mcps: [mcp],
        files,
        skips: [skip],
        payload,
    });
    assert.deepEqual([...staged.skills.keys()], ["aaa", "demo"]);
    assert.deepEqual(staged.overridden, []);
});

test("absolute tool paths and out-of-root mcp entries are rejected", async () => {    const absText = "abs";
    const absRecord = {
        fileId: bundleFileId(SOURCE_ID, "tools/abs.sh"),
        sourceId: SOURCE_ID,
        sourcePath: "/absolute.sh",
        packagePath: `bundles/${SOURCE_ID}/tools/abs.sh`,
        sha256: sha256hex(absText),
        sizeBytes: Buffer.byteLength(absText),
        gitMode: "100644",
        mediaType: "text/plain",
        owners: [{ kind: "skill-tool", id: skillId }],
    };
    await assert.rejects(
        stageBundleForWorkspace({
            skills: [skill],
            mcps: [mcp],
            files: [...files, absRecord].sort((a, b) => (a.fileId < b.fileId ? -1 : 1)),
            skips: [skip],
            payload: { ...payload, [absRecord.packagePath]: Buffer.from(absText) },
        }),
        /derive|unsafe|absolute|path/,
    );
    const strayMcp = {
        ...mcp,
        entryPath: "other/entry.py",
    };
    const strayText = "entry";
    const strayRecord = {
        fileId: bundleFileId(SOURCE_ID, "other/entry.py"),
        sourceId: SOURCE_ID,
        sourcePath: "other/entry.py",
        packagePath: `bundles/${SOURCE_ID}/other/entry.py`,
        sha256: sha256hex(strayText),
        sizeBytes: Buffer.byteLength(strayText),
        gitMode: "100644",
        mediaType: "text/plain",
        owners: [{ kind: "mcp", id: mcpId }],
    };
    await assert.rejects(
        stageBundleForWorkspace({
            skills: [skill],
            mcps: [{ ...strayMcp }],
            files: [...files, strayRecord].sort((a, b) => (a.fileId < b.fileId ? -1 : 1)),
            skips: [skip],
            payload: { ...payload, [strayRecord.packagePath]: Buffer.from(strayText) },
        }),
        /outside|root|layout/,
    );
    const dotdotText = "evil";
    const dotdotRecord = {
        fileId: bundleFileId(SOURCE_ID, "sub/../../evil.sh"),
        sourceId: SOURCE_ID,
        sourcePath: "sub/../../evil.sh",
        packagePath: `bundles/${SOURCE_ID}/sub/../../evil.sh`,
        sha256: sha256hex(dotdotText),
        sizeBytes: Buffer.byteLength(dotdotText),
        gitMode: "100644",
        mediaType: "text/plain",
        owners: [{ kind: "skill-tool", id: skillId }],
    };
    await assert.rejects(
        stageBundleForWorkspace({
            skills: [skill],
            mcps: [mcp],
            files: [...files, dotdotRecord].sort((a, b) => (a.fileId < b.fileId ? -1 : 1)),
            skips: [skip],
            payload: { ...payload, [dotdotRecord.packagePath]: Buffer.from(dotdotText) },
        }),
        /unsafe|workspace/,
    );
});
