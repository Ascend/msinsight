import {
    getCardFlowSourceDbPaths,
    getCardSourceDbPaths,
    getFlowPointIdentity,
    getLaneProcessIdentity,
    getLaneSourceThreadIdentity,
    getLaneThreadIdentity,
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
});
