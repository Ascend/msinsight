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

const MAX_STRING_LENGTH = 1000;
const MAX_ARRAY_LENGTH = 50;
const MAX_DEPTH = 8;

export const createPageContextService = ({ eventBus } = {}) => {
    let observation = null;
    let updatedAt = null;

    const getObservation = () => ({ observation, updatedAt });

    const updateObservation = (nextObservation) => {
        observation = sanitizePageObservation(nextObservation);
        updatedAt = Date.now();
        eventBus?.broadcast?.({ type: "page_observation_updated", updatedAt });
        return getObservation();
    };

    return { getObservation, updateObservation };
};

export const sanitizePageObservation = (value) => sanitizeValue(value);

const sanitizeValue = (value, depth = 0) => {
    if (depth > MAX_DEPTH) return undefined;
    if (value === null || value === undefined) return value;
    if (typeof value === "string") return value.slice(0, MAX_STRING_LENGTH);
    if (typeof value === "number" || typeof value === "boolean") return value;
    if (Array.isArray(value)) {
        return value.slice(0, MAX_ARRAY_LENGTH)
            .map((item) => sanitizeValue(item, depth + 1))
            .filter((item) => item !== undefined);
    }
    if (typeof value !== "object") return undefined;
    return Object.fromEntries(Object.entries(value)
        .slice(0, MAX_ARRAY_LENGTH)
        .map(([key, item]) => [String(key).slice(0, MAX_STRING_LENGTH), sanitizeValue(item, depth + 1)])
        .filter(([, item]) => item !== undefined));
};
