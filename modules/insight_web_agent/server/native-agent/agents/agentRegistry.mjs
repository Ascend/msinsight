/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { readFile, readdir, realpath, stat } from "node:fs/promises";
import { isAbsolute, join, relative, resolve } from "node:path";
import { parseAgentMarkdown } from "./agentParser.mjs";

/** 功能：创建 Primary Agent Registry，按来源优先级发现并覆盖 OpenCode Agent 文件。 */
export const createAgentRegistry = ({ bundledDir, developmentDirs = [] }) => {
    const agents = new Map();
    const registryDiagnostics = [];
    const sources = createSources(bundledDir, developmentDirs);

    const initialize = async () => {
        agents.clear();
        registryDiagnostics.length = 0;
        await scanSource(sources[0], agents, registryDiagnostics);
        const bundledGeneral = agents.get("general");
        if (bundledGeneral?.source.kind !== "bundled" || !isPrimaryAgent(bundledGeneral)) {
            throw new Error(`required bundled Primary Agent is unavailable: general (${bundledDir})`);
        }
        for (const source of sources.slice(1)) await scanSource(source, agents, registryDiagnostics);
        if (!isPrimaryAgent(agents.get("general"))) {
            throw new Error("effective Primary Agent is unavailable: general");
        }
        return { agents: list(), diagnostics: diagnostics() };
    };

    const list = () => [...agents.values()].map(toPublicAgent).sort(compareAgent);
    const get = (id) => agents.get(String(id ?? "").trim());
    const getPrimary = (id) => {
        const agent = get(id);
        return isPrimaryAgent(agent) ? agent : undefined;
    };
    const diagnostics = () => [...registryDiagnostics, ...[...agents.values()].flatMap((agent) => agent.diagnostics)];

    return { initialize, list, get, getPrimary, diagnostics };
};

const createSources = (bundledDir, developmentDirs) => [
    { id: "bundled", kind: "bundled", directory: resolve(bundledDir), precedence: 100 },
    ...developmentDirs.map((directory, index) => ({
        id: `development:${index + 1}`,
        kind: "development",
        directory: resolve(directory),
        precedence: 200 + index,
    })),
];

const scanSource = async (source, agents, diagnostics) => {
    let entries;
    try {
        entries = await readdir(source.directory, { withFileTypes: true });
    } catch (error) {
        if (error.code !== "ENOENT" || source.kind === "bundled") {
            diagnostics.push(sourceDiagnostic(source, "AGENT_SOURCE_UNAVAILABLE", error.message));
        }
        return;
    }
    for (const entry of entries.sort((left, right) => left.name.localeCompare(right.name))) {
        if (!entry.isFile() || !entry.name.endsWith(".md")) continue;
        const filePath = join(source.directory, entry.name);
        await registerFile({ source, filePath, agents, diagnostics });
    }
};

const registerFile = async ({ source, filePath, agents, diagnostics }) => {
    try {
        const canonicalRoot = await realpath(source.directory);
        const canonicalFile = await realpath(filePath);
        if (!isInside(canonicalRoot, canonicalFile) || !(await stat(canonicalFile)).isFile()) throw new Error("agent file resolves outside its source root");
        const agent = parseAgentMarkdown({ filePath: canonicalFile, content: await readFile(canonicalFile, "utf8"), source: { ...source, root: canonicalRoot, path: canonicalFile } });
        if (agent.mode === "subagent") {
            agent.diagnostics.push({
                code: "AGENT_SUBAGENT_UNSUPPORTED",
                message: "Subagent delegation is not supported by the current native runtime.",
                path: canonicalFile,
                sourceId: source.id,
                resourceId: agent.id,
            });
        }
        const previous = agents.get(agent.id);
        if (previous) {
            diagnostics.push({
                code: "AGENT_OVERRIDDEN",
                message: `Primary Agent '${agent.id}' from ${previous.source.id} is overridden by ${source.id}.`,
                path: canonicalFile,
                sourceId: source.id,
                resourceId: agent.id,
            });
        }
        agents.set(agent.id, agent);
    } catch (error) {
        diagnostics.push({
            code: "AGENT_INVALID",
            message: error.message,
            path: filePath,
            sourceId: source.id,
            resourceId: entryId(filePath),
        });
    }
};

const isInside = (root, target) => {
    const relation = relative(root, target);
    return !relation || (!relation.startsWith("..") && !isAbsolute(relation));
};

const isPrimaryAgent = (agent) => agent && (agent.mode === "primary" || agent.mode === "all");

const toPublicAgent = (agent) => ({
    id: agent.id,
    name: agent.name,
    description: agent.description,
    mode: agent.mode,
    source: { id: agent.source.id, kind: agent.source.kind },
    diagnostics: [...agent.diagnostics],
});

const compareAgent = (left, right) => {
    if (left.id === "general") return -1;
    if (right.id === "general") return 1;
    return left.id.localeCompare(right.id);
};

const sourceDiagnostic = (source, code, message) => ({ code, message, path: source.directory, sourceId: source.id });
const entryId = (filePath) => filePath.split(/[\\/]/).pop()?.replace(/\.md$/, "");
