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
import { ServerConnector } from './index';

it('drops a slow reply when the requesting iframe has been replaced by one with the same name', async () => {
    const server = new ServerConnector({ getInterceptorHandlers: () => () => {} });
    let finish!: (response: Record<string, unknown>) => void;
    server.registerAwaitFetch(() => new Promise(resolve => { finish = resolve; }));
    const attach = (): HTMLIFrameElement => {
        const frame = document.createElement('iframe');
        frame.name = 'Communication';
        document.body.appendChild(frame);
        return frame;
    };
    const send = (frame: HTMLIFrameElement): void => {
        window.dispatchEvent(new MessageEvent('message', {
            source: frame.contentWindow,
            data: { event: 'request', from: frame.name, id: 1, args: { command: 'communication/advisor' } },
        }));
    };
    const original = attach();
    send(original);
    original.remove();
    const replacement = attach();
    const post = jest.spyOn(replacement.contentWindow as Window, 'postMessage').mockImplementation(() => {});
    try {
        finish({ body: { stale: true } });
        await Promise.resolve();
        expect(post).not.toHaveBeenCalled();
        send(replacement);
        finish({ body: { current: true } });
        await Promise.resolve();
        expect(post).toHaveBeenCalledTimes(1);
        expect(JSON.parse(post.mock.calls[0][0] as string)).toMatchObject({ id: 1, body: { current: true } });
    } finally { replacement.remove(); }
});
