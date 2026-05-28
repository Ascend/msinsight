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

const createHardwareMetricsTree = (): InsightMetaData<'card'> => ({
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
            processId: '__hardware_metrics__',
            processName: 'Hardware Metrics',
            metaType: 'HARDWARE_METRICS',
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

describe('timeline unit metadata expansion', () => {
    afterEach(() => {
        clearParentMap();
    });

    it('deduplicates label and counter hardware metric nodes during repeated expansion', () => {
        const cardUnit = createCardUnit();
        const metadataTree = createHardwareMetricsTree();

        updateDataSourceAndParentMetaDataMap(metadataTree, dataSource);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);
        recursiveExpandUnit(metadataTree.children ?? [], cardUnit);

        expect(cardUnit.children).toHaveLength(1);
        const hardwareMetrics = cardUnit.children?.[0];
        expect(hardwareMetrics?.name).toBe('Label');
        expect(hardwareMetrics?.children).toHaveLength(1);
        const hbm = hardwareMetrics?.children?.[0];
        expect(hbm?.name).toBe('Label');
        expect(hbm?.children).toHaveLength(1);
        expect(hbm?.children?.[0].name).toBe('Counter');
    });
});
