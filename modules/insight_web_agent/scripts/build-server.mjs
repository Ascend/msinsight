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
const outputAgentConfig = join(outputDir, "agent-servers.json");

await rm(outputDir, { recursive: true, force: true });
await mkdir(outputDir, { recursive: true });

await build({
    bundle: true,
    entryPoints: [join(rootDir, "server", "index.mjs")],
    external: ["node:*"],
    format: "esm",
    platform: "node",
    outfile: outputEntry,
    sourcemap: false,
    target: "node18",
});

await cp(
    join(rootDir, "agent-servers.json"),
    outputAgentConfig,
);
await cp(join(rootDir, "prompts"), join(outputDir, "prompts"), { recursive: true });
await cp(join(rootDir, "..", "..", "docs"), join(outputDir, "docs"), { recursive: true });

console.log(`Server bundle written to ${outputEntry}`);
console.log(`Agent config copied to ${outputAgentConfig}`);
console.log(`Prompts copied to ${join(outputDir, "prompts")}`);
console.log(`Docs copied to ${join(outputDir, "docs")}`);
