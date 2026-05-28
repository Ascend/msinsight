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
import { workerSetMemoryBlockData, workerTransform } from '@/leaksWorker/blockWorker/worker';
import {
    type GraphParam,
    getBlocksGraphData,
} from '../utils/request';
import { message } from 'antd';
import { runInAction } from 'mobx';

export const getBarNewData = async (session: any, startTimestamp?: number, endTimestamp?: number): Promise<void> => {
    try {
        const param: GraphParam = { startTimestamp, endTimestamp };
        const blockData = await getBlocksGraphData(param);
        const transform = { x: 0, y: 0, scale: 1 };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        workerSetMemoryBlockData({ data: blockData });
    } catch (error: any) {
        message.error(error.message);
    }
};
