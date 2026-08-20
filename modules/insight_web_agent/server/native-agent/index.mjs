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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
const AI_SYSTEM_MESSAGE_WARNING = "AI SDK Warning: System messages in the prompt or messages fields";

for (const method of ["log", "info", "warn", "debug"]) {
    console[method] = (...args) => {
        if (method === "warn" && String(args[0] ?? "").startsWith(AI_SYSTEM_MESSAGE_WARNING)) return;
        process.stderr.write(`${args.map(String).join(" ")}\n`);
    };
}

const { existsSync } = await import("node:fs");
const { basename, delimiter, dirname, join, resolve } = await import("node:path");
const { fileURLToPath } = await import("node:url");
const { createAcpHostClient } = await import("./acp/hostClient.mjs");
const { createAcpProtocolServer } = await import("./acp/protocol.mjs");
const { createAgentRegistry } = await import("./agents/agentRegistry.mjs");
const { createAcpSessionNotifier } = await import("./acp/sessionUpdates.mjs");
const { createFilesystemPolicyService } = await import("./config/filesystemPolicy.mjs");
const { createSessionStore } = await import("./data/sessionStore.mjs");
const { createRuntime } = await import("./runtime/aiSdkRuntime.mjs");
const { createSkillRegistry } = await import("./skills/skillRegistry.mjs");
const { createNativeSessionService } = await import("./session/sessionService.mjs");
const { createToolRegistry } = await import("./tools/ToolRegistry.mjs");
const { createBashTools } = await import("./tools/bashTools.mjs");
const { createMsinsightTools } = await import("./tools/msinsightTools.mjs");
const { createSkillTools } = await import("./tools/skillTools.mjs");


const createFilesystemPolicyValue = ({ configured, defaults, developmentRoots }) => {
    let policy = defaults;
    try {
        policy = configured ? { ...defaults, ...JSON.parse(configured) } : defaults;
    } catch (error) {
        console.warn(`Failed to parse native filesystem policy before adding development roots: ${error.message}`);
    }
    return JSON.stringify({
        ...policy,
        extraPaths: [...(Array.isArray(policy.extraPaths) ? policy.extraPaths : []), ...developmentRoots],
    });
};

const sessions = new Map();
const running = new Map();
const entryDirectory = dirname(fileURLToPath(import.meta.url));
const defaultResourceDir = basename(entryDirectory) === "native-agent" && existsSync(join(dirname(entryDirectory), "agents"))
    ? dirname(entryDirectory)
    : resolve(entryDirectory, "..", "..");
const packagedResources = basename(defaultResourceDir) === "dist-server";
const defaultBundledAgentsDir = join(defaultResourceDir, "agents");
const defaultBundledDocsDir = packagedResources
    ? join(defaultResourceDir, "docs")
    : resolve(defaultResourceDir, "..", "..", "docs");
const defaultBundledSkillsDir = packagedResources
    ? join(defaultResourceDir, "skills")
    : resolve(defaultResourceDir, "..", "..", "skills");
const storeDir = resolve(process.env.MSINSIGHT_NATIVE_STORE_DIR ?? join(process.cwd(), ".msinsight_native_agent"));
const aiSdkStoragePath = join(storeDir, "ai-sdk");

const notifier = createAcpSessionNotifier(process.stdout);
const developmentRoots = process.env.MSINSIGHT_NATIVE_DEVELOPMENT === "1"
    ? String(process.env.MSINSIGHT_NATIVE_DEVELOPMENT_ROOTS ?? "").split(delimiter).map((path) => path.trim()).filter(Boolean).map(resolve)
    : [];
const filesystem = createFilesystemPolicyService({
    env: {
        ...process.env,
        INSIGHT_WEB_AGENT_RESOURCE_DIR: process.env.INSIGHT_WEB_AGENT_RESOURCE_DIR ?? defaultResourceDir,
        INSIGHT_WEB_AGENT_FILESYSTEM_POLICY: createFilesystemPolicyValue({
            configured: process.env.INSIGHT_WEB_AGENT_FILESYSTEM_POLICY,
            defaults: { docsRoot: defaultBundledDocsDir, skillsRoot: defaultBundledSkillsDir },
            developmentRoots,
        }),
    },
});
const agentRegistry = createAgentRegistry({
    bundledDir: resolve(process.env.MSINSIGHT_NATIVE_BUNDLED_AGENTS_DIR ?? defaultBundledAgentsDir),
    developmentDirs: developmentRoots.map((root) => join(resolve(root), "agents")),
});
const skillRegistry = createSkillRegistry({
    bundledDir: resolve(process.env.MSINSIGHT_NATIVE_BUNDLED_SKILLS_DIR ?? defaultBundledSkillsDir),
    developmentDirs: developmentRoots.map((root) => join(resolve(root), "skills")),
});
await Promise.all([agentRegistry.initialize(), skillRegistry.initialize()]);
for (const diagnostic of [...agentRegistry.diagnostics(), ...skillRegistry.diagnostics()]) {
    console.warn(JSON.stringify({ component: "native-agent", ...diagnostic }));
}
const hostClient = createAcpHostClient({ writeJson: notifier.writeJson });
const toolRegistry = createToolRegistry({ tools: [
    ...createMsinsightTools(),
    ...createBashTools({ sessions, hostClient }),
    ...createSkillTools({ skillRegistry }),
] });
const sessionStore = createSessionStore({
    sessions,
    storeDir,
    createFilesystemRoots: filesystem.createSessionFilesystemRoots,
    canonicalizeFilesystemRoots: filesystem.canonicalizeFilesystemRoots,
    canonicalizeProjectRoot: filesystem.canonicalizeProjectRoot,
});
const aiRuntime = createRuntime({ aiSdkStoragePath, toolRegistry, notifier });
const sessionService = createNativeSessionService({
    sessions,
    running,
    aiRuntime,
    sessionStore,
    filesystem,
    agentRegistry,
    skillRegistry,
    toolRegistry,
    notifier,
});
const acpProtocol = createAcpProtocolServer({
    writeJson: notifier.writeJson,
    handleRequest: sessionService.handleRequest,
    handleResponse: hostClient.handleResponse,
    beforeRequest: sessionStore.load,
    onClose: () => {
        hostClient.close();
        for (const [sessionId, activePrompt] of running) {
            activePrompt.controller.abort();
            const session = sessions.get(sessionId);
            if (session) aiRuntime.abortSession(session);
        }
    },
});

await acpProtocol.start();
