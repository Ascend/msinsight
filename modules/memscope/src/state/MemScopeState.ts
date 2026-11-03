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
import type { ThreShold } from '../common/shared/api/types';
import { makeAutoObservable } from 'mobx';

interface TypeOption {
    label: string | number;
    value: string | number;
}

export class MemScopeState {
    lazyUsedThreshold: ThreShold = { perT: null, valueT: null };
    delayedFreeThreshold: ThreShold = { perT: null, valueT: null };
    longIdleThreshold: ThreShold = { perT: null, valueT: null };
    onlyInefficient: boolean = false;
    allowTrim: boolean = true;
    threadIds: number[] = [];
    memoryData: any = { size: 0, name: '', subNodes: [] };// 内存拆解图数据源
    funcData: any = { traces: [], maxTimestamp: 0, minTimestamp: 0 };// 调用栈图数据源
    memoryStamp: number = 0;
    threadId: number | string = '';
    threadOps: TypeOption[] = [];
    searchFunc: string[] = [];
    funcOptions: any = [];
    threadFlag: boolean = false; // 判断是否由双击事件触发的thread的变化
    maxDepth: number = 0; // 火焰图最大深度
    loadingFunc: boolean = false;

    constructor() {
        makeAutoObservable(this);
    }
}
