/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { chmod, mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { loadCapabilityCenterConfig } from "../../config/capabilityCenterConfig.mjs";

const writeConfig = (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
const makeExecutable = async (path) => {
    await writeFile(path, "fake", "utf8");
    if (process.platform !== "win32") await chmod(path, 0o700);
};

test("loads generic CLI capability from the first available platform candidate", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "capability-config-"));
    const configPath = join(root, "capability-center.json");
    const executable = join(root, process.platform === "win32" ? "custom.exe" : "custom");
    t.after(() => rm(root, { recursive: true, force: true }));
    await makeExecutable(executable);
    await writeConfig(configPath, {
        schemaVersion: 1,
        capabilities: [{
            type: "cli",
            name: "custom_cli",
            description: "Custom CLI",
            executable: {
                win32: ["./missing.exe", "./custom.exe"],
                default: ["./missing", "./custom"],
            },
        }],
    });

    assert.deepEqual(loadCapabilityCenterConfig({ configPath, resourceDir: root }), [{
        type: "cli",
        name: "custom_cli",
        description: "Custom CLI",
        executable,
    }]);
});

test("skips unavailable CLI capabilities", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "capability-config-skip-"));
    const configPath = join(root, "capability-center.json");
    t.after(() => rm(root, { recursive: true, force: true }));
    await writeConfig(configPath, {
        schemaVersion: 1,
        capabilities: [{ type: "cli", name: "missing", executable: "missing-cli" }],
    });

    assert.deepEqual(loadCapabilityCenterConfig({ configPath, resourceDir: root, env: { PATH: "" } }), []);
});

test("rejects duplicate capability names and unsupported types", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "capability-config-invalid-"));
    const configPath = join(root, "capability-center.json");
    t.after(() => rm(root, { recursive: true, force: true }));
    await writeConfig(configPath, {
        schemaVersion: 1,
        capabilities: [
            { type: "cli", name: "duplicate", executable: "first" },
            { type: "cli", name: "duplicate", executable: "second" },
        ],
    });
    assert.throws(
        () => loadCapabilityCenterConfig({ configPath, resourceDir: root }),
        /configured more than once/,
    );

    await writeConfig(configPath, {
        schemaVersion: 1,
        capabilities: [{ type: "unknown", name: "bad", executable: "bad" }],
    });
    assert.throws(
        () => loadCapabilityCenterConfig({ configPath, resourceDir: root }),
        /unsupported type/,
    );
});

test("rejects fields outside the minimal CLI schema", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "capability-config-fields-"));
    const configPath = join(root, "capability-center.json");
    t.after(() => rm(root, { recursive: true, force: true }));
    await writeConfig(configPath, {
        schemaVersion: 1,
        capabilities: [{
            type: "cli",
            name: "extra_config",
            executable: "custom-cli",
            maxConcurrency: 2,
        }],
    });

    assert.throws(
        () => loadCapabilityCenterConfig({ configPath, resourceDir: root }),
        /unknown field 'maxConcurrency'/,
    );
});
