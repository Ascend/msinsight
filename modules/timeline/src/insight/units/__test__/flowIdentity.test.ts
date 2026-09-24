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
import {
    getCardFlowSourceDbPaths,
    getCardFlowQueryDbPaths,
    getCardFlowQuerySources,
    getCardSourceDbPaths,
    getFlowPointIdentity,
    getLaneProcessIdentity,
    getLaneProcessIdentityCandidates,
    getLaneSourceThreadIdentity,
    getLaneSourceThreadIdentityCandidates,
    getLaneThreadIdentity,
    getLaneThreadIdentityCandidates,
} from '../../../entity/data';

describe('flow identity', () => {
    const point = {
        rankId: 'rank0',
        dbPath: 'thread-1.db',
        pid: '10',
        tid: '20',
        depth: 3,
        timestamp: 100,
    };

    it('separates equal flow points from different databases', () => {
        expect(getFlowPointIdentity(point)).not.toBe(getFlowPointIdentity({ ...point, dbPath: 'thread-2.db' }));
    });

    it('keeps the legacy flow key when database path is missing', () => {
        expect(getFlowPointIdentity({ ...point, dbPath: undefined })).toBe('10_20_3_100');
    });

    it('separates process and thread lane keys by database path', () => {
        expect(getLaneProcessIdentity('rank0', '10', 'thread-1.db'))
            .not.toBe(getLaneProcessIdentity('rank0', '10', 'thread-2.db'));
        expect(getLaneThreadIdentity('rank0', '10', '20', 'thread-1.db'))
            .not.toBe(getLaneThreadIdentity('rank0', '10', '20', 'thread-2.db'));
    });

    it('keeps legacy lane keys when database path is missing', () => {
        expect(getLaneProcessIdentity('rank0', '10')).toBe('rank0-10');
        expect(getLaneThreadIdentity('rank0', '10', '20')).toBe('rank0-10-20');
    });

    it('falls back to legacy lane keys when source-aware metadata is unavailable', () => {
        expect(getLaneProcessIdentityCandidates('rank0', '10', 'trace.db')).toEqual([
            getLaneProcessIdentity('rank0', '10', 'trace.db'),
            getLaneProcessIdentity('rank0', '10'),
        ]);
        expect(getLaneThreadIdentityCandidates('rank0', '10', '20', 'trace.db')).toEqual([
            getLaneThreadIdentity('rank0', '10', '20', 'trace.db'),
            getLaneThreadIdentity('rank0', '10', '20'),
        ]);
        expect(getLaneSourceThreadIdentityCandidates('rank0', '20', 'trace.db')).toEqual([
            getLaneSourceThreadIdentity('rank0', '20', 'trace.db'),
            getLaneSourceThreadIdentity('rank0', '20'),
        ]);
    });

    it('identifies a thread within one physical database without requiring its process hierarchy', () => {
        expect(getLaneSourceThreadIdentity('rank0', '20', 'thread-1.db'))
            .not.toBe(getLaneSourceThreadIdentity('rank0', '20', 'thread-2.db'));
    });

    it('collects every unique database source below a merged card', () => {
        const card = {
            metadata: { cardId: 'rank0', dbPath: 'thread-1.db' },
            children: [
                { metadata: { cardId: 'rank0', dbPath: 'thread-1.db' } },
                { metadata: { cardId: 'rank0', dbPath: 'thread-2.db' } },
                { metadata: { cardId: 'rank1', dbPath: 'other-card.db' } },
            ],
        };

        expect(getCardSourceDbPaths(card, 'rank0')).toEqual(['thread-1.db', 'thread-2.db']);
    });

    it('excludes derived overlap databases from flow sources', () => {
        const card = {
            metadata: { cardId: 'rank0', dbPath: 'thread-1.db' },
            children: [
                { metadata: { cardId: 'rank0', dbPath: 'thread-1.db', metaType: 'Ascend Hardware' } },
                { metadata: { cardId: 'rank0', dbPath: 'thread-2.db', metaType: 'Python' } },
                {
                    metadata: {
                        cardId: 'rank0',
                        dbPath: 'file:msinsight_overlap_rank0?mode=memory&cache=shared',
                        metaType: 'OVERLAP_ANALYSIS',
                    },
                },
            ],
        };

        expect(getCardFlowSourceDbPaths(card, 'rank0')).toEqual(['thread-1.db', 'thread-2.db']);
    });

    it('uses card-level source for single-source flow queries', () => {
        expect(getCardFlowQueryDbPaths('', ['mindstudio_insight_data.db'])).toEqual(['']);
        expect(getCardFlowQueryDbPaths('profile.db', ['profile.db'])).toEqual(['profile.db']);
    });

    it('uses rank-based queries for TEXT projects even when an internal database exists', () => {
        expect(getCardFlowQueryDbPaths('mindstudio_insight_data.db', ['mindstudio_insight_data.db'], true)).toEqual(['']);
    });

    it('keeps the displayed database source when TEXT flow queries use the rank source', () => {
        expect(getCardFlowQuerySources('mindstudio_insight_data.db', ['mindstudio_insight_data.db'], true)).toEqual([{
            queryDbPath: '',
            sourceDbPath: 'mindstudio_insight_data.db',
        }]);
    });

    it('uses the queried database as the displayed source for DB flows', () => {
        expect(getCardFlowQuerySources('profile.db', ['thread-1.db', 'thread-2.db'])).toEqual([
            { queryDbPath: 'thread-1.db', sourceDbPath: 'thread-1.db' },
            { queryDbPath: 'thread-2.db', sourceDbPath: 'thread-2.db' },
        ]);
    });

    it('queries each physical source for multi-source cards', () => {
        expect(getCardFlowQueryDbPaths('profile.db', ['thread-1.db', 'thread-2.db']))
            .toEqual(['thread-1.db', 'thread-2.db']);
    });
});
