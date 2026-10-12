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
import { sendAgentPanelNotification } from './agentPanelNotification';

describe('Agent panel notifications', () => {
    beforeEach(() => {
        document.body.innerHTML = '';
    });

    it('sends a notification to the Agent iframe using its origin', () => {
        const frame = document.createElement('iframe');
        frame.id = 'AcpSession';
        frame.src = 'http://localhost:3010/index.html';
        document.body.append(frame);
        const postMessage = jest.spyOn(frame.contentWindow!, 'postMessage').mockImplementation();

        sendAgentPanelNotification('switchLanguage', { lang: 'zhCN' });

        expect(postMessage).toHaveBeenCalledWith(
            { event: 'switchLanguage', body: { lang: 'zhCN' } },
            'http://localhost:3010',
        );
    });

    it('does nothing when the Agent iframe is unavailable', () => {
        expect(() => sendAgentPanelNotification('setTheme', { isDark: true })).not.toThrow();
    });
});
