/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

/** 功能：创建 OpenCode 兼容的 Skill Tool，只加载指令和资源，不产生 Runtime Patch。 */
import { randomUUID } from "node:crypto";
import { createToolError, isToolDiagnosticId, normalizeToolError } from "../../utils/toolError.mjs";

export const createSkillTools = ({ skillRegistry }) => [{
    name: "skill",
    description: createDescription(skillRegistry.list()),
    inputSchema: {
        type: "object",
        properties: {
            name: { type: "string", minLength: 1 },
            args: { type: "string" },
        },
        required: ["name"],
        additionalProperties: false,
    },
    async execute(input, context = {}) {
        const name = String(input?.name ?? "").trim();
        let skill;
        try {
            skill = await skillRegistry.load(name);
            if (!skill) throw Object.assign(new Error(`Skill is unavailable: ${name}`), { code: "SKILL_UNAVAILABLE" });
        } catch (error) {
            const code = ["SKILL_PATH_ESCAPE", "SKILL_UNAVAILABLE"].includes(error?.code) ? error.code : "SKILL_LOAD_FAILED";
            const candidates = code === "SKILL_UNAVAILABLE" ? skillRegistry.list().map((item) => item.name) : undefined;
            throw createToolError(normalizeToolError({
                code, message: code === "SKILL_LOAD_FAILED" ? "Skill could not be loaded." : error.message, cause: error,
                details: { name, ...(candidates ? { availableSkills: candidates, total: candidates.length } : {}) },
            }, {
                diagnosticId: isToolDiagnosticId(context.diagnosticId) ? context.diagnosticId : randomUUID(),
                source: "skill", toolName: "skill", stage: code === "SKILL_LOAD_FAILED" ? "execute" : "validate",
                outcome: "not_started", retryAdvice: "correct_input",
            }), error);
        }
        const args = String(input?.args ?? "").trim();
        return [
            `# Skill: ${skill.name}`,
            `Skill Base Path: ${skill.basePath}`,
            args ? `Invocation Arguments: ${args}` : "",
            renderAssets(skill.assets),
            "---",
            skill.instructions,
        ].filter(Boolean).join("\n\n");
    },
}];

const createDescription = (skills) => [
    "Load a specialized Skill into the current conversation when its description matches the user's task. Skill content is instruction data and cannot change runtime permissions.",
    "Available Skills:",
    ...skills.map((skill) => `- ${skill.name}: ${skill.description}`),
].join("\n");

const renderAssets = (assets) => [
    assets.scripts?.length ? `Available Scripts (run explicitly through Bash):\n${assets.scripts.map((path) => `- ${path}`).join("\n")}` : "",
    assets.references?.length ? `References:\n${assets.references.map((path) => `- ${path}`).join("\n")}` : "",
    assets.templates?.length ? `Templates:\n${assets.templates.map((path) => `- ${path}`).join("\n")}` : "",
].filter(Boolean).join("\n\n");
