/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2.
 */
/** Case-insensitive literal tokens, matched as ordered (possibly separated) characters. */
export const matchesSessionTitle = (title: string, query: string): boolean => {
    const characters = [...title.toLowerCase()];
    return query.trim().toLowerCase().split(/\s+/u).filter(Boolean).every((token) => {
        let cursor = 0;
        for (const character of token) {
            const next = characters.indexOf(character, cursor);
            if (next < 0) return false;
            cursor = next + 1;
        }
        return true;
    });
};
