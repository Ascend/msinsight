/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { lstat, readFile, readdir, realpath, stat } from "node:fs/promises";
import { isAbsolute, join, relative, resolve } from "node:path";
import { parse as parseYaml } from "yaml";

const FRONTMATTER_PATTERN = /^---\r?\n([\s\S]*?)\r?\n---\r?\n?([\s\S]*)$/;
const SKILL_ID_PATTERN = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const SUPPORTED_FIELDS = new Set(["name", "description", "license", "compatibility", "metadata"]);

/** 功能：创建纯指令 Skill Registry，禁止内联命令和运行时副作用。 */
export const createSkillRegistry = ({ bundledDir, developmentDirs = [] }) => {
    const skills = new Map();
    const registryDiagnostics = [];
    const sources = createSources(bundledDir, developmentDirs);

    const initialize = async () => {
        skills.clear();
        registryDiagnostics.length = 0;
        for (const source of sources) await scanSource(source, skills, registryDiagnostics);
        return { skills: list(), diagnostics: diagnostics() };
    };
    const list = () => [...skills.values()].map(toPublicSkill).sort((left, right) => left.name.localeCompare(right.name));
    const get = (name) => skills.get(String(name ?? "").trim());
    const load = async (name) => {
        const skill = get(name);
        if (!skill) return undefined;
        const canonicalPath = await realpath(skill.path);
        if (!isInside(skill.sourceRoot, canonicalPath) || !(await stat(canonicalPath)).isFile()) {
            throw new Error(`Skill resolves outside its source root: ${skill.name}`);
        }
        return { ...skill, instructions: extractInstructions(await readFile(canonicalPath, "utf8")) };
    };
    const diagnostics = () => [...registryDiagnostics, ...[...skills.values()].flatMap((skill) => skill.diagnostics)];
    return { initialize, list, get, load, diagnostics };
};

const createSources = (bundledDir, developmentDirs) => [
    { id: "bundled", kind: "bundled", directory: resolve(bundledDir) },
    ...developmentDirs.map((directory, index) => ({ id: `development:${index + 1}`, kind: "development", directory: resolve(directory) })),
];

const scanSource = async (source, skills, diagnostics) => {
    let entries;
    try {
        entries = await readdir(source.directory, { withFileTypes: true });
    } catch (error) {
        if (error.code !== "ENOENT" || source.kind === "bundled") diagnostics.push(createDiagnostic("SKILL_SOURCE_UNAVAILABLE", error.message, source.directory, source));
        return;
    }
    for (const entry of entries.sort((left, right) => left.name.localeCompare(right.name))) {
        if (!entry.isDirectory()) continue;
        await registerSkill({ source, directory: join(source.directory, entry.name), expectedName: entry.name, skills, diagnostics });
    }
};

const registerSkill = async ({ source, directory, expectedName, skills, diagnostics }) => {
    const filePath = join(directory, "SKILL.md");
    try {
        const canonicalRoot = await realpath(source.directory);
        const canonicalDirectory = await realpath(directory);
        const canonicalFile = await realpath(filePath);
        if (!isInside(canonicalRoot, canonicalDirectory) || !isInside(canonicalDirectory, canonicalFile) || !(await stat(canonicalFile)).isFile()) {
            throw new Error("Skill resolves outside its source root");
        }
        const skill = await parseSkillMetadata({ content: await readFile(canonicalFile, "utf8"), expectedName, path: canonicalFile, source: { ...source, root: canonicalRoot } });
        const previous = skills.get(skill.name);
        if (previous) diagnostics.push(createDiagnostic("SKILL_OVERRIDDEN", `Skill '${skill.name}' from ${previous.source.id} is overridden by ${source.id}.`, canonicalFile, source, skill.name));
        skills.set(skill.name, skill);
    } catch (error) {
        if (error.code !== "ENOENT") diagnostics.push(createDiagnostic("SKILL_INVALID", error.message, filePath, source, expectedName));
    }
};

const parseSkillMetadata = async ({ content, expectedName, path, source }) => {
    const match = parseSkillDocument(content);
    const frontmatter = parseYaml(match[1]);
    if (!frontmatter || typeof frontmatter !== "object" || Array.isArray(frontmatter)) throw new Error("frontmatter must be an object");
    const name = String(frontmatter.name ?? "").trim();
    const description = String(frontmatter.description ?? "").trim();
    if (!SKILL_ID_PATTERN.test(name) || name !== expectedName) throw new Error(`Skill name must match its directory: ${expectedName}`);
    if (!description) throw new Error("description is required");
    const instructions = match[2].trim();
    if (!instructions) throw new Error("Skill instructions are required");
    const diagnostics = Object.keys(frontmatter).filter((field) => !SUPPORTED_FIELDS.has(field)).map((field) => (
        createDiagnostic("SKILL_RUNTIME_EFFECT_IGNORED", `Unsupported Skill frontmatter field is ignored: ${field}`, path, source, name)
    ));
    if (instructions.includes("!`")) diagnostics.push(createDiagnostic("SKILL_INLINE_COMMAND_DISABLED", "Skill inline commands are disabled and remain inert text.", path, source, name));
    return {
        name,
        description,
        license: frontmatter.license,
        compatibility: frontmatter.compatibility,
        metadata: frontmatter.metadata,
        basePath: await realpath(join(path, "..")),
        path,
        sourceRoot: source.root,
        source: { id: source.id, kind: source.kind },
        diagnostics,
        assets: await discoverAssets(join(path, "..")),
    };
};

const parseSkillDocument = (content) => {
    const match = String(content ?? "").replace(/^﻿/, "").match(FRONTMATTER_PATTERN);
    if (!match) throw new Error("missing YAML frontmatter");
    return match;
};

const extractInstructions = (content) => {
    const instructions = parseSkillDocument(content)[2].trim();
    if (!instructions) throw new Error("Skill instructions are required");
    return instructions;
};

const toPublicSkill = ({ path: _path, basePath: _basePath, assets: _assets, sourceRoot: _sourceRoot, ...skill }) => ({
    ...skill,
    source: { ...skill.source },
    diagnostics: [...skill.diagnostics],
});

const discoverAssets = async (basePath) => Object.fromEntries(await Promise.all(["scripts", "references", "templates"].map(async (name) => {
    const root = join(basePath, name);
    try {
        const rootEntry = await lstat(root);
        if (rootEntry.isSymbolicLink() || !rootEntry.isDirectory()) return [name, []];
        return [name, await listAssetFiles(root, root, name)];
    } catch (_error) {
        return [name, []];
    }
})));

const listAssetFiles = async (root, directory, prefix) => {
    const files = [];
    for (const entry of (await readdir(directory, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
        if (entry.isSymbolicLink()) continue;
        const path = join(directory, entry.name);
        if (!isInside(root, path)) continue;
        const relativePath = `${prefix}/${relative(root, path).replaceAll("\\", "/")}`;
        if (entry.isFile()) files.push(relativePath);
        else if (entry.isDirectory()) files.push(...await listAssetFiles(root, path, prefix));
    }
    return files;
};

const isInside = (root, target) => {
    const relation = relative(root, target);
    return !relation || (!relation.startsWith("..") && !isAbsolute(relation));
};
const createDiagnostic = (code, message, path, source, resourceId) => ({ code, message, path, sourceId: source.id, resourceId });
