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
export const palette = ['deepBlue', 'coralRed', 'tealGreen', 'aquaBlue', 'raspberryPink', 'vividBlue',
    'vividRed', 'royalPurple', 'skyBlue', 'sunsetOrange', 'amethystPurple', 'limeGreen'] as const;
export function operatorColor(name: string): typeof palette[number] {
    let hash = 0;
    for (let i = 0; i < name.length; i++) { hash += 31 * name.charCodeAt(i); }
    return palette[Math.abs(hash) % palette.length];
}
