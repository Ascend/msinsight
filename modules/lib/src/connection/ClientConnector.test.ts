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
import { ClientConnector } from './index';
import { createRequest } from '../utils/Request';
import { errorCenter } from '../utils/ErrorCenter';

it('preserves legacy routing and errors while supporting cancellation without a global notification', async () => {
    const request = createRequest(new ClientConnector({ module: 'cluster' }));
    const post = jest.spyOn(window.parent, 'postMessage').mockImplementation(() => {});
    const notify = jest.spyOn(errorCenter, 'handleError').mockImplementation(() => {});
    const lastMessage = (): { id: number; module: string } => JSON.parse(post.mock.calls[post.mock.calls.length - 1][0] as string);
    const reply = (id: number, body: unknown): void => {
        window.dispatchEvent(new MessageEvent('message', { data: JSON.stringify({ event: 'request', id, body }) }));
    };

    const group = request('communication/matrix/group', { clusterPath: 'c' });
    const groupId = lastMessage().id;
    expect(lastMessage()).toMatchObject({ module: 'communication', args: { command: 'communication/matrix/group', params: { clusterPath: 'c' } } });
    const controller = new AbortController();
    const locate = request('unit/kernelDetail', {}, 'timeline', { signal: controller.signal });
    const locateId = lastMessage().id;
    expect(lastMessage().module).toBe('timeline');
    controller.abort();
    await expect(locate).rejects.toMatchObject({ name: 'AbortError' });
    reply(locateId, { old: true });
    reply(groupId, { groups: [1, 2] });
    await expect(group).resolves.toEqual({ groups: [1, 2] });
    await expect(request('unit/kernelDetail', {}, 'timeline', { signal: controller.signal })).rejects.toMatchObject({ name: 'AbortError' });
    expect(post).toHaveBeenCalledTimes(2);
    expect(notify).not.toHaveBeenCalled();

    for (const silent of [true, false]) {
        const failure = request('communication/advisor', {}, undefined, { silent });
        reply(lastMessage().id, { error: { code: 4001, message: 'request failed' } });
        await expect(failure).rejects.toMatchObject({ name: 'WsError', code: 4001, message: 'request failed' });
        expect(notify).toHaveBeenCalledTimes(silent ? 0 : 1);
    }
});
