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

import type { InsightMetaData } from '../../../entity/data';
import type { InsightUnit } from '../../../entity/insight';
import { clearParentMap, recursiveExpandUnit, updateDataSourceAndParentMetaDataMap } from '../unitFunc';

jest.mock('../AscendUnit', () => {
    class MockUnit {
        name: string;
        metadata: Record<string, unknown>;
        children?: MockUnit[];

        constructor(name: string, metadata: Record<string, unknown>) {
            this.name = name;
            this.metadata = metadata;
        }
    }

    return {
        CounterUnit: class extends MockUnit {
            constructor(metadata: Record<string, unknown>) {
                super('Counter', metadata);
            }
        },
        LabelUnit: class extends MockUnit {
            constructor(metadata: Record<string, unknown>) {
                super('Label', metadata);
            }
        },
        ProcessUnit: class extends MockUnit {
            constructor(metadata: Record<string, unknown>) {
                super('Process', metadata);
            }
        },
        ThreadUnit: class extends MockUnit {
            chart = { config: {} };

            constructor(metadata: Record<string, unknown>) {
                super('Thread', metadata);
            }
        },
    };
});

const dataSource = { remote: 'local' } as unknown as DataSource;

const createCardUnit = (): InsightUnit => ({
    name: 'Card',
    metadata: {
        cardId: 'rank0',
        dbPath: 'rank0.db',
        dataSource,
    },
    children: [],
} as Partial<InsightUnit> as InsightUnit);

const createNpuMetricsTree = (): InsightMetaData<'card'> => ({
    type: 'card',
    dataSource,
    metadata: {
        cardId: 'rank0',
        dbPath: 'rank0.db',
        dataSource,
    } as never,
    children: [{
        type: 'label',
        dataSource,
        metadata: {
            cardId: 'rank0',
            dbPath: 'rank0.db',
            dataSource,
            processId: '__npu_metrics__',
            processName: 'NPU Metrics',
            metaType: 'NPU_METRICS',
            label: '',
        },
        children: [{
            type: 'label',
            dataSource,
            metadata: {
                cardId: 'rank0',
                dbPath: 'rank0.db',
                dataSource,
                processId: '14083671101',
                processName: 'HBM',
                metaType: 'TEXT',
                label: 'NPU',
            },
            children: [{
                type: 'counter',
                dataSource,
                metadata: {
                    cardId: 'rank0',
                    dbPath: 'rank0.db',
                    dataSource,
                    processId: '14083671101',
                    processName: 'HBM',
                    threadName: 'HBM 0/Read',
                    threadId: 'HBM 0/Read',
                    metaType: 'TEXT',
                    dataType: ['Read(MB/s)'],
                },
            }],
        }],
    }],
});

const createHostTree = (): InsightMetaData<'card'> => ({
    type: 'card',
    dataSource,
    metadata: {
        cardId: 'rank0',
        dbPath: 'rank0.db',
        dataSource,
    } as never,
    children: [{
        type: 'process',
        dataSource,
        metadata: {
            cardId: 'rank0',
            dbPath: 'rank0.db',
            dataSource,
            processId: 'process-100',
            processName: 'Process 100',
            metaType: 'PROCESS',
        },
        children: [{
            type: 'process',
            dataSource,
            metadata: {
                cardId: 'rank0',
                dbPath: 'rank0.db',
                dataSource,
                processId: 'global-tid-200',
                processName: 'Thread 200',
                threadId: '200',
                metaType: 'CANN_API',
            },
            children: [{
                type: 'thread',
                dataSource,
                metadata: {
                    cardId: 'rank0',
                    dbPath: 'rank0.db',
                    dataSource,
                    processId: 'global-tid-200',
                    processName: '',
                    threadId: 'pytorch-api',
                    threadName: 'PyTorch',
                    metaType: 'PYTORCH_API',
                    groupNameValue: '',
                    rankList: [],
                },
            }, {
                type: 'label',
                dataSource,
                metadata: {
                    cardId: 'rank0',
                    dbPath: 'rank0.db',
                    dataSource,
                    processId: 'global-tid-200',
                    processName: 'CANN',
                    metaType: 'CANN_API',
                    label: '',
                },
                children: [{
                    type: 'thread',
                    dataSource,
                    metadata: {
                        cardId: 'rank0',
                        dbPath: 'rank0.db',
                        dataSource,
                        processId: 'global-tid-200',
                        processName: '',
                        threadId: 'acl',
                        threadName: 'acl',
                        metaType: 'CANN_API',
                        groupNameValue: '',
                        rankList: [],
                    },
                }],
            }, {
                type: 'process',
                dataSource,
                metadata: {
                    cardId: 'rank0',
                    dbPath: 'rank0.db',
                    dataSource,
                    processId: 'global-tid-200',
                    processName: 'MSTX',
                    threadId: '200',
                    metaType: 'MSTX_EVENTS',
                },
                children: [{
                    type: 'thread',
                    dataSource,
                    metadata: {
                        cardId: 'rank0',
                        dbPath: 'rank0.db',
                        dataSource,
                        processId: 'global-tid-200',
                        processName: '',
                        threadId: 'domain-0',
                        threadName: 'domain 0',
                        metaType: 'MSTX_EVENTS',
                        groupNameValue: '',
                        rankList: [],
                    },
                }],
            }],
        }],
    }],
});

describe('timeline unit metadata expansion', () => {
    afterEach(() => {
        clearParentMap();
    });

    it('builds the NPU Metrics hierarchy during metadata expansion', () => {
        const cardUnit = createCardUnit();
        const metadataTree = createNpuMetricsTree();

        updateDataSourceAndParentMetaDataMap(metadataTree, dataSource);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);

        expect(cardUnit.children).toHaveLength(1);
        const npuMetrics = cardUnit.children?.[0];
        expect(npuMetrics?.name).toBe('Label');
        expect(npuMetrics?.children).toHaveLength(1);
        const hbm = npuMetrics?.children?.[0];
        expect(hbm?.name).toBe('Label');
        expect(hbm?.children).toHaveLength(1);
        expect(hbm?.children?.[0].name).toBe('Counter');
    });

    it('keeps host lanes with the same process id separated during metadata expansion', () => {
        const cardUnit = createCardUnit();
        const metadataTree = createHostTree();

        updateDataSourceAndParentMetaDataMap(metadataTree, dataSource);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);

        expect(cardUnit.children).toHaveLength(1);
        const threadUnit = cardUnit.children?.[0].children?.[0];
        expect(threadUnit?.children).toHaveLength(3);

        const pytorch = threadUnit?.children?.find(unit => unit.name === 'Thread' && unit.metadata.threadName === 'PyTorch');
        const cann = threadUnit?.children?.find(unit => unit.name === 'Label' && unit.metadata.processName === 'CANN');
        const mstx = threadUnit?.children?.find(unit => unit.name === 'Process' && unit.metadata.processName === 'MSTX');

        expect(pytorch).toBeDefined();
        expect(pytorch?.children).toBeUndefined();
        expect(cann?.children).toHaveLength(1);
        expect(cann?.children?.[0].metadata.threadName).toBe('acl');
        expect(mstx?.children).toHaveLength(1);
        expect(mstx?.children?.[0].metadata.threadName).toBe('domain 0');
    });
});
