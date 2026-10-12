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
    async execute(input) {
        const name = String(input?.name ?? "").trim();
        const skill = await skillRegistry.load(name);
        if (!skill) throw new Error(`Skill is unavailable: ${name}`);
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
