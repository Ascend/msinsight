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
import { isValidSession, type ValidSession } from '../entity/session';
import { dataFunc } from './utils';

export interface Cache {
    getData: <T extends any>(session: ValidSession, params: any) => Promise<Partial<any>>;
}

export interface CacheFactory {
    create: () => Cache;
}

export type Caches = Record<any, Cache | null>;

/**
 * create a new caches object for the session which is latest created without caches
 * @param session new session without caches
 * @returns new caches for session
 */

export const createCaches = (session: ValidSession): Caches => {
    return {};
};

const msAbsoluteTimeInterfaces: any[] = [];

export default class DicCachedEngine {
    fetchData = async <T extends any>(dataConfig: any): Promise<any> => {
        const { session, params } = dataConfig;
        if (!isValidSession(session)) {
            // wedge return emptyData as unknown as DataType<T>;
            return {};
        }
        let sessionCaches = session.caches;
        if (sessionCaches === null) {
            sessionCaches = createCaches(session);
            session.caches = sessionCaches;
        }

        let jsonData: Partial<any> = {};
        for (const key of Object.keys(params)) {
            const requestKey = key as keyof any;
            const currCache = null; // sessionCaches[requestKey];
            if (dataFunc === undefined) {
                continue;
            }
            let data;
            try {
                if (currCache === null) {
                    data = [];
                } else {
                    data = [];
                }
            } catch (e) {
                // wedge ErrorRes
                const err = e as (any | undefined);
            }
            jsonData = { ...jsonData, ...data };
        }
        return jsonData;
    };
}
