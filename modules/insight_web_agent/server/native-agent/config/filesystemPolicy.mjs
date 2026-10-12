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
import { stat } from "node:fs/promises";
import { isAbsolute, join, resolve } from "node:path";
import { canonicalPath } from "../shared/utils.mjs";

/** 功能：创建 native-agent 文件系统访问策略服务。 */
export const createFilesystemPolicyService = ({ env = process.env, cwd = process.cwd() } = {}) => {
    const resourceDir = resolve(env.INSIGHT_WEB_AGENT_RESOURCE_DIR ?? cwd);
    const policy = parseFilesystemPolicy(env.INSIGHT_WEB_AGENT_FILESYSTEM_POLICY, resourceDir);

    /** 功能：使用当前文件系统策略和代理工作目录生成会话可访问根目录。 */
    const createSessionFilesystemRoots = (dynamicProjectRoot) => createFilesystemRoots(policy, dynamicProjectRoot, cwd);

    /** 功能：并行规范化所有文件系统根目录，消除可解析的符号链接。 */
    const canonicalizeFilesystemRoots = (roots) => Promise.all(roots.map(canonicalPath));

    /** 功能：仅接受宿主提供的绝对且存在的项目目录，并消除符号链接。 */
    const canonicalizeProjectRoot = async (projectRoot) => {
        const value = typeof projectRoot === "string" ? projectRoot.trim() : "";
        if (!policy.includeProjectRoot || !value || !isAbsolute(value)) return undefined;
        try {
            const canonicalRoot = await canonicalPath(value);
            return (await stat(canonicalRoot)).isDirectory() ? canonicalRoot : undefined;
        } catch (_error) {
            return undefined;
        }
    };

    /** 功能：根据隐藏上下文中的项目根更新会话文件白名单，返回是否发生变化。 */
    const updateSessionFilesystemRoots = async (session, projectRoot) => {
        const nextProjectRoot = await canonicalizeProjectRoot(projectRoot);
        const nextRoots = createSessionFilesystemRoots(nextProjectRoot);
        if (nextProjectRoot === session.projectRoot && JSON.stringify(nextRoots) === JSON.stringify(session.filesystemRoots)) return false;
        session.projectRoot = nextProjectRoot;
        session.filesystemRoots = nextRoots;
        session.canonicalFilesystemRoots = await canonicalizeFilesystemRoots(nextRoots);
        return true;
    };

    return {
        policy,
        resourceDir,
        createSessionFilesystemRoots,
        canonicalizeFilesystemRoots,
        canonicalizeProjectRoot,
        updateSessionFilesystemRoots,
    };
};

/** 功能：解析文件系统白名单 JSON 并补齐资源目录默认值。 */
const parseFilesystemPolicy = (value, resourceDir) => {
    try {
        const policy = JSON.parse(String(value ?? "{}"));
        return {
            includeDocsRoot: policy.includeDocsRoot === true,
            includeAgentWorkspaceRoot: policy.includeAgentWorkspaceRoot !== false,
            includeProjectRoot: policy.includeProjectRoot !== false,
            docsRoot: policy.docsRoot ? resolve(String(policy.docsRoot)) : join(resourceDir, "docs"),
            skillsRoot: policy.skillsRoot ? resolve(String(policy.skillsRoot)) : join(resourceDir, "skills"),
            extraPaths: Array.isArray(policy.extraPaths) ? policy.extraPaths.map((path) => resolve(String(path))) : [],
        };
    } catch (error) {
        console.warn(`Failed to parse native filesystem policy: ${error.message}`);
        return {
            includeDocsRoot: false,
            includeAgentWorkspaceRoot: true,
            includeProjectRoot: true,
            docsRoot: join(resourceDir, "docs"),
            skillsRoot: join(resourceDir, "skills"),
            extraPaths: [],
        };
    }
};

/** 功能：组合工作区、文档、技能、项目和额外白名单路径，生成当前会话可访问根目录。 */
const createFilesystemRoots = (policy, dynamicProjectRoot, agentWorkspaceRoot) => [...new Set([
    policy.includeAgentWorkspaceRoot ? agentWorkspaceRoot : undefined,
    policy.includeDocsRoot ? policy.docsRoot : undefined,
    policy.skillsRoot,
    policy.includeProjectRoot ? dynamicProjectRoot : undefined,
    ...policy.extraPaths,
].filter(isPresentPath).map(resolveFilesystemPath))];

/** 功能：判断候选文件系统根路径是否存在有效值。 */
const isPresentPath = (path) => Boolean(path);

/** 功能：把文件系统根路径解析为绝对路径。 */
const resolveFilesystemPath = (path) => resolve(path);
