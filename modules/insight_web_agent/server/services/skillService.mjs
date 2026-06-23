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
import { readdir, readFile } from "node:fs/promises";
import { join } from "node:path";

const SKILL_FILE = "SKILL.md";

export const createSkillService = ({ rootDir }) => {
    const skillsDir = join(rootDir, "skills");
    let cache = [];

    const refresh = async () => {
        try {
            const entries = await readdir(skillsDir, { withFileTypes: true });
            const skills = await Promise.all(entries
                .filter((entry) => entry.isDirectory())
                .map((entry) => loadSkill(skillsDir, entry.name)));
            cache = skills.filter(Boolean).sort((a, b) => a.name.localeCompare(b.name));
        } catch (error) {
            if (error?.code !== "ENOENT") console.warn(`Failed to scan skills: ${error.message}`);
            cache = [];
        }
        return cache;
    };

    return {
        async list() {
            return refresh();
        },

        async extractFromPrompt(text) {
            const promptText = String(text ?? "").trim();
            const match = promptText.match(/^\/(\S+)(?:\s+([\s\S]*))?$/);
            if (!match) return { text: promptText, skills: [] };
            const skills = await refresh();
            const skill = skills.find((item) => item.name === match[1]);
            if (!skill) return { text: promptText, skills: [] };
            return { text: String(match[2] ?? "").trim(), skills: [skill] };
        },
    };
};

const loadSkill = async (skillsDir, dirName) => {
    const filePath = join(skillsDir, dirName, SKILL_FILE);
    try {
        const content = await readFile(filePath, "utf8");
        const metadata = parseFrontmatter(content);
        const name = String(metadata.name ?? dirName).trim();
        if (!name) return undefined;
        return {
            name,
            description: String(metadata.description ?? "").trim(),
            path: filePath,
            content,
        };
    } catch (error) {
        console.warn(`Failed to load skill ${dirName}: ${error.message}`);
        return undefined;
    }
};

const parseFrontmatter = (content) => {
    if (!content.startsWith("---")) return {};
    const end = content.indexOf("\n---", 3);
    if (end === -1) return {};
    return Object.fromEntries(content.slice(3, end)
        .split(/\r?\n/)
        .map((line) => line.match(/^([^:#]+):\s*(.*)$/))
        .filter(Boolean)
        .map((match) => [match[1].trim(), match[2].trim().replace(/^['"]|['"]$/g, "")]));
};
