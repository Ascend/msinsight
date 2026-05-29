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

import { cloneDeep, throttle } from 'lodash';
import type { Cache } from '../cache';
import { logger } from '../../utils/Logger';
import type { ValidSession } from '../../entity/session';
import { getRange, dataFunc } from '../utils';

/**
 * this cache won't process any data,
 * just return the data every threshold ms
 */
// ThrottleCache<T extends DataKey>
export class ThrottleCache<T extends any> implements Cache {
    // data: SampleType[T];
    data: any;
    key: T;
    // params: DataParam<DataKey>;
    fetch: (session: ValidSession, params: any) => void;

    constructor(key: T, threshold: number = 500) {
        this.key = key;
        this.data = {};
        // params: DataParam<T>;
        this.fetch = (throttle(async (session: ValidSession, params: any) => {
            try {
                this.data = (await dataFunc(session, getRange, params)) as any[];
            } catch (e) {
                // e: ErrorRes;
                const err = e as (any | undefined);
                if (err) {
                    logger(`throttleCache/${this.key}`, `got error when fetching data: ${err.errorMessage}`, 'warn');
                }
            }
        }, threshold));
    };

    getData = (session: ValidSession, params: any): Promise<any> => {
        this.fetch(session, params);
        return Promise.resolve({ [this.key as string]: cloneDeep(this.data) });
    };
};
