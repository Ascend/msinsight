import { getLaneIdentity } from '../../../entity/data';

describe('lane identity', () => {
    const metadata = {
        cardId: 'rank0',
        dbPath: 'thread-1.db',
        processId: 'Ascend Hardware',
        metaType: 'Ascend Hardware',
        threadId: '7',
    };

    it('separates equal-named streams from different source databases', () => {
        expect(getLaneIdentity(metadata)).not.toBe(getLaneIdentity({
            ...metadata,
            dbPath: 'thread-2.db',
        }));
    });

    it('is stable for the same physical lane', () => {
        expect(getLaneIdentity(metadata)).toBe(getLaneIdentity({ ...metadata }));
    });
});
