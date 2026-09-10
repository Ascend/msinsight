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
const DISCOVERED_NAME_SUFFIX = /\s*\(auto\)$/i;
const BUILTIN_AGENT_NAME = 'msinsight-native';

export const uniqueCopiedAgentName = (sourceName: string, taken: Iterable<string>): string => {
    const takenNames = new Set([...taken, BUILTIN_AGENT_NAME]);
    const base = sourceName.replace(DISCOVERED_NAME_SUFFIX, '').trim() || 'Agent';
    if (!takenNames.has(base) && !DISCOVERED_NAME_SUFFIX.test(base)) return base;
    for (let index = 1; index < 1000; index += 1) {
        const candidate = index === 1 ? `${base} Copy` : `${base} Copy ${index}`;
        if (!takenNames.has(candidate)) return candidate;
    }
    return `${base} Copy`;
};
