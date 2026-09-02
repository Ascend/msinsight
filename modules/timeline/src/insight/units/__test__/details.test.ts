import type { InsightUnit } from '../../../entity/insight';
import type { Session } from '../../../entity/session';
import type { ThreadMetaData } from '../../../entity/data';
import { slicesListDetail } from '../details';

jest.mock('../AscendUnit', () => ({
    getSliceTimeDisplay: (value: number): string => String(value),
}));
jest.mock('../../../components/charts/ChartInteractor/draw', () => ({
    checkIsValidSlice: (): boolean => false,
}));
jest.mock('../../../components/ChartContainer/Units/UnitInfo', () => ({
    checkIsSameUnit: (): boolean => true,
}));

const createThreadUnit = (dbPath: string, threadId: string): InsightUnit => ({
    metadata: {
        cardId: '0',
        dbPath,
        dataSource: { remote: 'local' },
        metaType: 'AscendCL',
        processId: '100',
        threadId,
        threadName: `thread-${threadId}`,
    },
} as unknown as InsightUnit);

describe('slicesListDetail', () => {
    it('queries and combines every selected database', async () => {
        const firstUnit = createThreadUnit('first.db', '101');
        const secondUnit = createThreadUnit('second.db', '202');
        const session = {
            selectedRange: [10, 20],
            selectedUnits: [firstUnit, secondUnit],
            units: [firstUnit, secondUnit],
            sliceSelection: { rangeOfLevels: [0, 0] },
            unitsConfig: { filterConfig: { pythonFunction: {} }, offsetConfig: { timestampOffset: {} } },
        } as unknown as Session;
        const request = jest.fn(async (_dataSource, requestData) => ({
            data: [{ title: `${requestData.params.dbPath}-slice`, occurrences: 1 }],
        }));
        window.request = request;

        const result = await slicesListDetail.fetchData(session, firstUnit.metadata as ThreadMetaData);

        expect(result).toEqual([
            expect.objectContaining({ title: 'first.db-slice', dbPath: 'first.db' }),
            expect.objectContaining({ title: 'second.db-slice', dbPath: 'second.db' }),
        ]);
        expect(request.mock.calls.map(([, data]) => data.params)).toEqual([
            expect.objectContaining({ dbPath: 'first.db', metadataList: [expect.objectContaining({ tid: '101' })] }),
            expect.objectContaining({ dbPath: 'second.db', metadataList: [expect.objectContaining({ tid: '202' })] }),
        ]);
    });
});
