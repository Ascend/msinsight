/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { build } from "esbuild";
import { cp, mkdir, rm } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const rootDir = dirname(fileURLToPath(new URL("../package.json", import.meta.url)));
const outputDir = join(rootDir, "dist-server");
const outputEntry = join(outputDir, "index.mjs");
const outputNativeAgentEntry = join(outputDir, "native-agent", "index.mjs");
const outputAgentConfig = join(outputDir, "agent-servers.json");
const outputSessionConfig = join(outputDir, "acp-session-conf.json");
const outputNativeConfig = join(outputDir, "msinsight-native.json");

await rm(outputDir, { recursive: true, force: true });
await mkdir(outputDir, { recursive: true });
await mkdir(dirname(outputNativeAgentEntry), { recursive: true });

await build({
    bundle: true,
    entryPoints: [join(rootDir, "server", "index.mjs")],
    external: ["node:*"],
    format: "esm",
    platform: "node",
    outfile: outputEntry,
    sourcemap: false,
    target: "node22.14",
});
await build({
    banner: {
        js: 'import { createRequire as __createRequire } from "node:module"; const require = __createRequire(import.meta.url);',
    },
    bundle: true,
    entryPoints: [join(rootDir, "server", "native-agent", "index.mjs")],
    external: ["node:*"],
    format: "esm",
    platform: "node",
    outfile: outputNativeAgentEntry,
    sourcemap: false,
    target: "node22.14",
});

await cp(
    join(rootDir, "agent-servers.json"),
    outputAgentConfig,
);
await cp(
    join(rootDir, "acp-session-conf.json"),
    outputSessionConfig,
);
await cp(join(rootDir, "msinsight-native.json"), outputNativeConfig);
await cp(join(rootDir, "prompts"), join(outputDir, "prompts"), { recursive: true });
await cp(join(rootDir, "agents"), join(outputDir, "agents"), { recursive: true });
await cp(join(rootDir, "..", "..", "docs"), join(outputDir, "docs"), { recursive: true });
await cp(join(rootDir, "..", "..", "skills"), join(outputDir, "skills"), { recursive: true });

console.log(`Server bundle written to ${outputEntry}`);
console.log(`Native agent bundle written to ${outputNativeAgentEntry}`);
console.log(`Agent config copied to ${outputAgentConfig}`);
console.log(`Session config copied to ${outputSessionConfig}`);
console.log(`Prompts copied to ${join(outputDir, "prompts")}`);
console.log(`Agents copied to ${join(outputDir, "agents")}`);
console.log(`Docs copied to ${join(outputDir, "docs")}`);
console.log(`Skills copied to ${join(outputDir, "skills")}`);
