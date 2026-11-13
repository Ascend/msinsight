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
import type { FrameTreePageRequest } from '../frameTree/types';

const params = { deviceId: '0', eventId: 204412, includeStatic: true, pageSize: 1000, currentPage: 1 };
const previousRequest = window.request;
const previousSource = process.env.REACT_APP_FRAME_TREE_SOURCE;
afterEach(() => {
    window.request = previousRequest;
    if (previousSource === undefined) delete process.env.REACT_APP_FRAME_TREE_SOURCE;
    else process.env.REACT_APP_FRAME_TREE_SOURCE = previousSource;
    jest.resetModules();
});
const api = (): { requestFrameTreePage: FrameTreePageRequest } =>
    jest.requireActual('../frameTree/api');

it.each([undefined, 'api', 'mock'])('always requests real frameTree data, ignoring the legacy source setting %s', async source => {
    if (source === undefined) delete process.env.REACT_APP_FRAME_TREE_SOURCE;
    else process.env.REACT_APP_FRAME_TREE_SOURCE = source;
    const page = { total: 1, nodes: [] };
    window.request = jest.fn().mockResolvedValue(page);
    await expect(api().requestFrameTreePage(params)).resolves.toBe(page);
    expect(window.request).toHaveBeenCalledWith({ command: 'Memory/snapshot/frameTree', params });
    expect(window.request).toHaveBeenCalledTimes(1);
});
it.each([undefined, 'mock'])('reports real API failures without substituting data (legacy source %s)', async source => {
    if (source === undefined) delete process.env.REACT_APP_FRAME_TREE_SOURCE;
    else process.env.REACT_APP_FRAME_TREE_SOURCE = source;
    window.request = jest.fn().mockRejectedValue(new Error('server failure'));
    await expect(api().requestFrameTreePage(params)).rejects.toThrow('server failure');
    expect(window.request).toHaveBeenCalledTimes(1);
});
