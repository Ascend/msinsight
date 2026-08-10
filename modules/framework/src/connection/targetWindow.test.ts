/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import { getModuleFrames, getTargetWindow } from './targetWindow';

describe('framework frame targeting', () => {
    beforeEach(() => {
        document.body.innerHTML = '';
    });

    it('excludes the Web Agent iframe while retaining business module frames', () => {
        const timeline = document.createElement('iframe');
        timeline.id = 'Timeline';
        const webAgent = document.createElement('iframe');
        webAgent.id = 'AcpSession';
        const memory = document.createElement('iframe');
        memory.id = 'Memory';
        document.body.append(timeline, webAgent, memory);

        expect(getModuleFrames()).toEqual([timeline, memory]);
        expect(getTargetWindow()).toEqual([timeline.contentWindow, memory.contentWindow]);
    });
});
