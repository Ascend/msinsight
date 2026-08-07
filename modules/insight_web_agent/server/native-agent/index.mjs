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

const { join, resolve } = await import("node:path");
const { createAcpProtocolServer } = await import("./acp/protocol.mjs");
const { createAcpSessionNotifier } = await import("./acp/sessionUpdates.mjs");
const { createFilesystemPolicyService } = await import("./config/filesystemPolicy.mjs");
const { createSessionStore } = await import("./data/sessionStore.mjs");
const { createBladeRuntime } = await import("./runtime/bladeRuntime.mjs");
const { createNativeSessionService } = await import("./session/sessionService.mjs");
const { createToolRegistry } = await import("./tools/ToolRegistry.mjs");
const { createMsinsightTools } = await import("./tools/msinsightTools.mjs");

const sessions = new Map();
const running = new Map();
const storeDir = resolve(process.env.MSINSIGHT_NATIVE_STORE_DIR ?? join(process.cwd(), ".msinsight_native_agent"));
const bladeStoragePath = join(storeDir, "blade");

const notifier = createAcpSessionNotifier(process.stdout);
const filesystem = createFilesystemPolicyService();
const toolRegistry = createToolRegistry({ tools: createMsinsightTools() });
const sessionStore = createSessionStore({
    sessions,
    storeDir,
    bladeStoragePath,
    createFilesystemRoots: filesystem.createSessionFilesystemRoots,
    canonicalizeFilesystemRoots: filesystem.canonicalizeFilesystemRoots,
    canonicalizeProjectRoot: filesystem.canonicalizeProjectRoot,
});
const aiRuntime = createBladeRuntime({
    bladeStoragePath,
    filesystemPolicy: filesystem.policy,
    toolRegistry,
    notifier,
});
const sessionService = createNativeSessionService({
    sessions,
    running,
    aiRuntime,
    sessionStore,
    filesystem,
    toolRegistry,
    notifier,
});
const acpProtocol = createAcpProtocolServer({
    writeJson: notifier.writeJson,
    handleRequest: sessionService.handleRequest,
    beforeRequest: sessionStore.load,
});

await acpProtocol.start();
