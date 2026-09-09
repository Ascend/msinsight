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

import { createRequest } from '@insight/lib/utils';
import type { NumaOverview } from '@/entities/numa/types';
import { connector } from '@/infrastructure/connection/client';

const request = createRequest(connector);

export interface NumaOverviewParams {
    rankId: string;
    dbPath: string;
    startTime?: number;
    endTime?: number;
}

export const getNumaOverview = async (params: NumaOverviewParams): Promise<NumaOverview> => {
    return request('numa/overview', {
        ...params,
        startTime: params.startTime ?? 0,
        endTime: params.endTime ?? 0,
    });
};
