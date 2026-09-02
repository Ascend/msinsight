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
import { clearParentMap, createCounterParam, createStatusParam, getHardwareSummaryDbPaths, mergeSummaryStatusData, reorderMultiSourceCardLanes, recursiveExpandUnit, updateDataSourceAndParentMetaDataMap } from '../unitFunc';

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
        ThreadingProcessUnit: class extends MockUnit {
            constructor(metadata: Record<string, unknown>) {
                super('Process', metadata);
            }
        },
        ThreadingThreadUnit: class extends MockUnit {
            constructor(metadata: Record<string, unknown>) {
                super('Thread', metadata);
            }
        },
        ThreadingLlcCacheUnit: class extends MockUnit {
            constructor(metadata: Record<string, unknown>) {
                super('LLC Cache', metadata);
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

const createThreadingTree = (): InsightMetaData<'card'> => ({
    type: 'card',
    dataSource,
    metadata: {
        cardId: 'Threading',
        dbPath: 'threading.db',
        dataSource,
    } as never,
    children: [{
        type: 'process',
        dataSource,
        metadata: {
            cardId: 'Threading',
            dbPath: 'threading.db',
            dataSource,
            processId: '1000',
            processName: 'Process_1000',
            metaType: 'THREADING_ANALYSIS',
        },
        children: ['10000', '10001'].map((threadId) => ({
            type: 'thread' as const,
            dataSource,
            metadata: {
                cardId: 'Threading',
                dbPath: 'threading.db',
                dataSource,
                processId: '1000',
                processName: 'Process_1000',
                threadId,
                threadName: `Thread_${threadId}`,
                metaType: 'THREADING_ANALYSIS',
            },
            children: [{
                type: 'counter' as const,
                dataSource,
                metadata: {
                    cardId: 'Threading',
                    dbPath: 'threading.db',
                    dataSource,
                    processId: '1000',
                    processName: 'Process_1000',
                    threadId,
                    threadName: 'LLC Cache',
                    dataType: ['LLC Hits', 'LLC Misses'],
                    metaType: 'THREADING_ANALYSIS',
                    metricGroup: 'llc_cache',
                    bucketWidthNs: 500_000_000,
                },
            }],
        })),
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

    it('creates an LLC Cache lane under every Process and Thread group', () => {
        const cardUnit = createCardUnit();
        const metadataTree = createThreadingTree();

        updateDataSourceAndParentMetaDataMap(metadataTree, dataSource);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);

        expect(cardUnit.children).toHaveLength(1);
        const processUnit = cardUnit.children?.[0];
        expect(processUnit?.name).toBe('Process');
        expect(processUnit?.metadata.processId).toBe('1000');
        expect(processUnit?.children).toHaveLength(2);
        expect(processUnit?.children?.map((thread) => thread.name)).toEqual(['Thread', 'Thread']);
        expect(processUnit?.children?.map((thread) => thread.metadata.threadId)).toEqual(['10000', '10001']);
        processUnit?.children?.forEach((thread) => {
            expect(thread.children).toHaveLength(1);
            expect(thread.children?.[0].name).toBe('LLC Cache');
            expect(thread.children?.[0].metadata.threadId).toBe(thread.metadata.threadId);
            expect(thread.children?.[0].metadata.bucketWidthNs).toBe(500_000_000);
            expect(thread.children?.[0].metadata.metricGroup).toBe('llc_cache');
        });
    });
});

describe('counter request cache identity', () => {
    const baseParams = {
        rankId: '0',
        dbPath: 'threading.db',
        pid: '1000',
        threadName: 'LLC Cache',
        threadId: '10001',
        metricGroup: 'llc_cache',
        metaType: 'THREADING_ANALYSIS',
        startTime: 0,
        endTime: 100,
    };

    it('isolates the cache by real thread id and metric group', () => {
        const key = createCounterParam('unit/counter', baseParams);
        expect(createCounterParam('unit/counter', { ...baseParams, threadId: '10002' })).not.toBe(key);
        expect(createCounterParam('unit/counter', { ...baseParams, metricGroup: 'thread_state' })).not.toBe(key);
    });

    it('preserves the source database path from merged metadata', () => {
        const cardUnit = createCardUnit();
        const metadataTree = createHostTree();
        const sourceProcess = metadataTree.children?.[0];
        if (sourceProcess !== undefined) {
            sourceProcess.metadata.dbPath = 'worker.db';
            sourceProcess.children?.forEach(child => {
                child.metadata.dbPath = 'worker.db';
            });
        }

        updateDataSourceAndParentMetaDataMap(metadataTree, dataSource);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);

        expect(cardUnit.children?.[0].metadata.dbPath).toBe('worker.db');
        expect(cardUnit.children?.[0].children?.[0].metadata.dbPath).toBe('worker.db');
    });

    it('preserves the source label from merged hardware metadata', () => {
        const cardUnit = createCardUnit();
        const metadataTree = createHostTree();
        const sourceThread = metadataTree.children?.[0].children?.[0].children?.[0];
        if (sourceThread !== undefined) {
            (sourceThread.metadata as any).sourceLabel = 'Thread 2';
        }

        updateDataSourceAndParentMetaDataMap(metadataTree, dataSource);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);

        expect((cardUnit.children?.[0].children?.[0].children?.[0].metadata as any).sourceLabel).toBe('Thread 2');
    });

    it('separates thread trace cache keys by physical thread and database', () => {
        const base = {
            cardId: 'rank0',
            dbPath: 'thread-1.db',
            processId: 'Ascend Hardware',
            metaType: 'Ascend Hardware',
            unitType: 'thread',
            threadId: '7',
            threadIdList: [],
            startTime: 0,
            endTime: 100,
        };

        expect(createStatusParam('unit/threadTraces', base)).not.toBe(createStatusParam('unit/threadTraces', {
            ...base,
            dbPath: 'thread-2.db',
        }));
        expect(createStatusParam('unit/threadTraces', base)).not.toBe(createStatusParam('unit/threadTraces', {
            ...base,
            threadId: '8',
        }));
    });

    it('moves top-level Process lanes first for multi-source hardware', () => {
        const process1 = { name: 'Process', metadata: { processName: 'Process 1', metaType: 'PROCESS' } } as unknown as InsightUnit;
        const hardware = {
            name: 'Process',
            metadata: { processName: 'Ascend Hardware', metaType: 'Ascend Hardware' },
            children: [
                { name: 'Thread', metadata: { dbPath: 'thread-1.db' } },
                { name: 'Thread', metadata: { dbPath: 'thread-2.db' } },
            ],
        } as unknown as InsightUnit;
        const overlap = { name: 'Process', metadata: { processName: 'Overlap Analysis', metaType: 'Overlap Analysis' } } as unknown as InsightUnit;
        const process2 = { name: 'Process', metadata: { processName: 'Process 2', metaType: 'PROCESS' } } as unknown as InsightUnit;
        const card = { name: 'Card', metadata: {}, children: [hardware, process1, overlap, process2] } as unknown as InsightUnit;

        reorderMultiSourceCardLanes(card);

        expect(card.children).toEqual([process1, process2, hardware, overlap]);
    });

    it('keeps the original top-level order for single-source hardware', () => {
        const process = { name: 'Process', metadata: { processName: 'Process 1', metaType: 'PROCESS' } } as unknown as InsightUnit;
        const hardware = {
            name: 'Process',
            metadata: { processName: 'Ascend Hardware', metaType: 'Ascend Hardware' },
            children: [{ name: 'Thread', metadata: { dbPath: 'thread-1.db' } }],
        } as unknown as InsightUnit;
        const card = { name: 'Card', metadata: {}, children: [hardware, process] } as unknown as InsightUnit;

        reorderMultiSourceCardLanes(card);

        expect(card.children).toEqual([hardware, process]);
    });

    it('collects stable unique database paths from hardware Stream children', () => {
        const unit = {
            children: [
                { metadata: { dbPath: 'thread-2.db' } },
                { metadata: { dbPath: 'thread-1.db' } },
                { metadata: { dbPath: 'thread-2.db' } },
                { metadata: { dbPath: '' } },
            ],
        } as unknown as InsightUnit;

        expect(getHardwareSummaryDbPaths(unit)).toEqual(['thread-2.db', 'thread-1.db']);
    });

    it('sorts and merges overlapping or adjacent summary intervals', () => {
        expect(mergeSummaryStatusData([
            { startTime: 30, duration: 10, name: '', type: '' },
            { startTime: 0, duration: 10, name: '', type: '' },
            { startTime: 8, duration: 12, name: '', type: '' },
            { startTime: 20, duration: 5, name: '', type: '' },
        ])).toEqual([
            { startTime: 0, duration: 25, name: '', type: '' },
            { startTime: 30, duration: 10, name: '', type: '' },
        ]);
    });
});
