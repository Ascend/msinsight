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

import type { Theme } from '@emotion/react';
import { getLinkLineColor } from '../utils';

describe('getLinkLineColor', () => {
    it('uses the same category hash to select a theme color', () => {
        const theme = {
            colorPalette: {
                deepBlue: '#123456',
                tealGreen: '#abcdef',
            },
        } as unknown as Theme;

        expect(getLinkLineColor('async_task_queue', theme)).toBe('#123456');
        expect(getLinkLineColor('fwdbwd', theme)).toBe('#abcdef');
    });
});
