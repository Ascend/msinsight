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
import { MemoryConnector } from '../MemoryConnector';

it('does not settle a new application request with an old iframe response', async () => {
    const connector = new MemoryConnector(0x100000);
    const send = jest.spyOn(connector, 'send').mockImplementation(() => {});
    let settled = false;
    const result = connector.fetch({ args: { command: 'synthetic' } }).then(response => { settled = true; return response; });
    expect(send).toHaveBeenCalledWith(expect.objectContaining({ id: 0x100000, event: 'request' }), expect.any(Function));
    window.dispatchEvent(new MessageEvent('message', { data: { event: 'request', id: 0, body: { stale: true } } }));
    await Promise.resolve();
    expect(settled).toBe(false);
    window.dispatchEvent(new MessageEvent('message', { data: { event: 'request', id: 0x100000, body: { current: true } } }));
    await expect(result).resolves.toMatchObject({ body: { current: true } });
    const serverFailure = connector.fetch({ args: { command: 'failure' } });
    window.dispatchEvent(new MessageEvent('message', { data: { event: 'request', id: 0x100001, body: { error: 'server failure' } } }));
    await expect(serverFailure).rejects.toBe('server failure');
    send.mockImplementation((_body, reject) => { reject?.(new Error('transport failure')); });
    await expect(connector.fetch({ args: { command: 'transport' } })).rejects.toThrow('transport failure');
    expect((connector as any).pendingRequests.size).toBe(0);
});
