/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

export const MAX_NAME_KEYWORDS = 20;
export const MAX_NAME_CHARACTERS = 200;

export const normalizeNameKeywords = (values: string[]): string[] => {
    const keywords = new Map<string, string>();
    values.flatMap(value => value.split(/[,，\r\n]/)).forEach(value => {
        const keyword = value.trim();
        if (keyword && !keywords.has(keyword.toLowerCase())) {
            keywords.set(keyword.toLowerCase(), keyword);
        }
    });
    return [...keywords.values()];
};

// Count Unicode characters across distinct keywords, excluding separators and surrounding whitespace.
export const nameKeywordsWithinLimit = (values: string[]): boolean => {
    const keywords = normalizeNameKeywords(values);
    return keywords.length <= MAX_NAME_KEYWORDS &&
        keywords.reduce((total, keyword) => total + Array.from(keyword).length, 0) <= MAX_NAME_CHARACTERS;
};
