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
import { ACP_MESSAGE_CHANNEL } from '@insight/lib/FrontendAgentCommand';
import { REPORT_OPENED } from '@insight/lib/AgentReport';
import { requestHostOpenReport } from '../connection';

jest.mock('@insight/lib/connection', () => ({ ClientConnector: jest.fn() }));

const report = { artifactId: '11111111-1111-1111-1111-111111111111', version: 2, title: 'Report' };
const originalParent = Object.getOwnPropertyDescriptor(window, 'parent');

afterEach(() => {
    if (originalParent) Object.defineProperty(window, 'parent', originalParent);
    document.body.innerHTML = '';
    jest.useRealTimers();
    jest.restoreAllMocks();
});

test('requires acknowledgement from the host and sends only a report reference', async () => {
    const host = document.createElement('iframe');
    document.body.append(host);
    Object.defineProperty(window, 'parent', { configurable: true, value: host.contentWindow });
    const post = jest.spyOn(host.contentWindow as Window, 'postMessage').mockImplementation(() => {});
    const request = requestHostOpenReport({ ...report, html: '<script>unsafe()</script>' } as typeof report);
    const sent = post.mock.calls[0][0];
    expect(sent.body.report).toEqual(report);
    let resolved = false;
    void request.then(() => { resolved = true; });
    const data = { channel: ACP_MESSAGE_CHANNEL, event: REPORT_OPENED, requestId: sent.requestId };
    window.dispatchEvent(new MessageEvent('message', { source: window, data }));
    await Promise.resolve();
    expect(resolved).toBe(false);
    window.dispatchEvent(new MessageEvent('message', { source: host.contentWindow, data }));
    await expect(request).resolves.toBe(true);
});

test('times out when the host does not support report tabs, preserving the caller preview', async () => {
    jest.useFakeTimers();
    const host = document.createElement('iframe');
    document.body.append(host);
    Object.defineProperty(window, 'parent', { configurable: true, value: host.contentWindow });
    jest.spyOn(host.contentWindow as Window, 'postMessage').mockImplementation(() => {});
    const request = requestHostOpenReport(report);
    jest.advanceTimersByTime(3000);
    await expect(request).resolves.toBe(false);
});
