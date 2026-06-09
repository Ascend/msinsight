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

import { cloneDeep } from 'lodash';
import type { CardMetaData } from '../entity/data';

export interface TimelineCard {
    cardName: string;
    cardPath: string;
    cluster: string;
    host: string;
    rankId: string;
    dbPath?: string;
    result: boolean;
    isFtrace?: boolean;
}

export const buildBaselineCardMetadata = (item: TimelineCard, dataSource: any): CardMetaData => {
    const curDataSource = cloneDeep(dataSource);
    curDataSource.dataPath = [item.cardPath];
    return {
        dataSource: curDataSource,
        cardId: item.rankId,
        dbPath: item.dbPath ?? '',
        cluster: item.cluster,
        cardName: item.cardName,
        cardPath: item.cardPath,
        isFtrace: item.isFtrace,
    };
};
