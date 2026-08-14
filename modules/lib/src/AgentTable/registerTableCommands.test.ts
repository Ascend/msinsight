/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { observeTableCommands } from './registerTableCommands';
import type { TableObservation } from './types';

describe('observeTableCommands', () => {
    test('exposes fully qualified command names instead of internal capabilities', () => {
        const tables = {
            observe: (): TableObservation[] => [{
                protocolVersion: 1,
                targetId: 'table-1',
                tableKey: 'memscope.system.blocks',
                title: 'Block View',
                revision: 3,
                availability: { visible: true, ready: true, busy: false },
                state: {
                    query: { filters: [], sort: null, page: 1, pageSize: 10 },
                    total: 1,
                    rowCount: 1,
                    selectedRowIds: [],
                    expandedRowIds: [],
                },
                columns: [],
                capabilities: ['table.setSort', 'table.getDisplayedData'],
                dataAccess: { maxRowsPerRequest: 10, availableRows: 1 },
            }],
        };

        const observation = observeTableCommands('MemScope', tables)[0];

        expect(observation.commands).toEqual([
            'MemScope.table.setSort',
            'MemScope.table.getDisplayedData',
        ]);
        expect(observation).not.toHaveProperty('capabilities');
    });
});
