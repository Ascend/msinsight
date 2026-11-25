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

import { queryFreeAttributionDetails, queryFreeAttributionSummary } from './request';
import type { FreeAttributionParams } from './interface';

jest.mock('../connection', () => ({ __esModule: true, default: {} }));

const params: FreeAttributionParams = {
    rankId: 'rank',
    dbPath: 'source.db',
    startTime: 0,
    endTime: 0,
    rangeMode: 'all',
    current: 1,
    pageSize: 10,
    orderBy: 'totalFreeDuration',
    order: 'desc',
};

it('leaves Free attribution errors to the view without global notifications', async () => {
    const request = jest.fn().mockResolvedValue({});
    window.requestData = request;
    await queryFreeAttributionSummary(params);
    expect(request).toHaveBeenLastCalledWith('systemView/freeAttribution/summary', params, 'timeline', { silent: true });

    const details = { ...params, groupId: 'matched:aten::add', orderBy: 'freeStartTime' };
    await queryFreeAttributionDetails(details);
    expect(request).toHaveBeenLastCalledWith('systemView/freeAttribution/details', details, 'timeline', { silent: true });

    const error = new Error('The selected database was released.');
    request.mockRejectedValue(error);
    await expect(queryFreeAttributionSummary(params)).rejects.toBe(error);
    await expect(queryFreeAttributionDetails(details)).rejects.toBe(error);
});
