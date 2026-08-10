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

// Blade SDK statically imports optional providers from its shared session chunk.
const unsupportedProviderFactories = {
    "@ai-sdk/azure": "createAzure",
    "@ai-sdk/google": "createGoogleGenerativeAI",
};
const unsupportedProviderPlugin = {
    name: "unsupported-ai-providers",
    setup(buildContext) {
        buildContext.onResolve(
            { filter: /^@ai-sdk\/(?:azure|google)$/ },
            ({ path }) => ({ path, namespace: "unsupported-ai-provider" }),
        );
        buildContext.onLoad(
            { filter: /.*/, namespace: "unsupported-ai-provider" },
            ({ path }) => ({
                contents: `export function ${unsupportedProviderFactories[path]}() { throw new Error("${path} is not supported by insight-web-agent"); }`,
                loader: "js",
            }),
        );
    },
};

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
    plugins: [unsupportedProviderPlugin],
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
await cp(join(rootDir, "prompts"), join(outputDir, "prompts"), { recursive: true });
await cp(join(rootDir, "..", "..", "docs"), join(outputDir, "docs"), { recursive: true });
await cp(join(rootDir, "..", "..", "skills"), join(outputDir, "skills"), { recursive: true });

console.log(`Server bundle written to ${outputEntry}`);
console.log(`Native agent bundle written to ${outputNativeAgentEntry}`);
console.log(`Agent config copied to ${outputAgentConfig}`);
console.log(`Session config copied to ${outputSessionConfig}`);
console.log(`Prompts copied to ${join(outputDir, "prompts")}`);
console.log(`Docs copied to ${join(outputDir, "docs")}`);
console.log(`Skills copied to ${join(outputDir, "skills")}`);
