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
import { downloadSessionExport } from '../sessionExportDownload';
import type { SessionExportDocument } from '../types';

test('downloads the export document as a JSON file', () => {
    jest.useFakeTimers();
    const click = jest.fn();
    HTMLAnchorElement.prototype.click = click;
    const createObjectURL = jest.fn(() => 'blob:export');
    const revokeObjectURL = jest.fn();
    Object.defineProperty(URL, 'createObjectURL', { configurable: true, value: createObjectURL });
    Object.defineProperty(URL, 'revokeObjectURL', { configurable: true, value: revokeObjectURL });

    const documentToSave: SessionExportDocument = {
        format: 'msinsight.agent-session.v1',
        exportedAt: '2026-09-13T06:41:00.000Z',
        filename: 'insight-session-demo-session-1-2026-09-13-06-41-00.json',
        markdown: '# demo\n',
        agent: { name: 'OpenCode', version: '1.18.30' },
        session: { sessionId: 'session-1', title: 'demo', updatedAt: null },
        messages: [],
    };

    downloadSessionExport(documentToSave);

    expect(createObjectURL).toHaveBeenCalledTimes(1);
    expect(click).toHaveBeenCalledTimes(1);
    expect(document.querySelector('a[download]')).toBeNull();
    jest.advanceTimersByTime(1000);
    expect(revokeObjectURL).toHaveBeenCalledWith('blob:export');
    jest.useRealTimers();
});

test('reveals a server-saved export through the host explorer IPC', () => {
    const postMessage = jest.fn();
    (window as Window & { ipc?: { postMessage: (message: string) => void } }).ipc = { postMessage };

    downloadSessionExport({
        format: 'msinsight.agent-session.v1',
        exportedAt: '2026-09-13T06:41:00.000Z',
        filename: 'insight-session-demo-session-1-2026-09-13-06-41-00.json',
        markdown: '# demo\n',
        savedPath: '/Users/test1/Downloads/insight-session-demo-session-1-2026-09-13-06-41-00.json',
        agent: { name: 'OpenCode', version: '1.18.30' },
        session: { sessionId: 'session-1', title: 'demo', updatedAt: null },
        messages: [],
    });

    expect(postMessage).toHaveBeenCalledWith(
        'openProjectInExplorer|/Users/test1/Downloads/insight-session-demo-session-1-2026-09-13-06-41-00.json',
    );
});
