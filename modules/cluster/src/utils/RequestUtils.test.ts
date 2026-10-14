import { getSlowRankList } from './RequestUtils';
import type { GetSlowRankListParams } from './interface';

jest.mock('../store', () => ({ store: { sessionStore: { activeSession: { selectedClusterPath: '/cluster' } } } }));
jest.mock('@insight/lib/utils', () => ({
    createCancelableApi: (fn: unknown) => fn,
    createSmartDebounceRequestFunc: (fn: unknown) => fn,
}));

const params = { iterationId: '1' } as GetSlowRankListParams;
const wire = (rankId: unknown = '3', fastRankId: unknown = '0', hasAdvice = true) => ({
    hasAdvice, fastRankId, fastTotalElapseTime: 100,
    data: [{ rankId, totalElapseTime: 200, totalDiffTime: 100, opList: [] }],
    extra: 'preserved',
});

beforeEach(() => {
    window.requestDataDebounced = Object.assign(jest.fn(), {
        cancel: jest.fn(),
        flush: jest.fn(),
        getPendingCount: jest.fn(() => 0),
        getPendingKeys: jest.fn(() => [] as string[]),
    });
});

const respond = (result: unknown) => {
    jest.mocked(window.requestDataDebounced).mockResolvedValueOnce(result);
    return getSlowRankList(params);
};

test('preserves string IDs and the response, rows, operators and other fields', async () => {
    const result = wire();
    const row = result.data[0];
    const operators = row.opList;
    const normalized = await respond(result);
    expect(normalized).toBe(result);
    expect(normalized.data[0]).toBe(row);
    expect(normalized.data[0].opList).toBe(operators);
    expect(normalized).toEqual({ ...wire('3', '0'), extra: 'preserved' });
    expect(window.requestDataDebounced).toHaveBeenCalledWith('communication/duration/slow-rank/list', {
        ...params, clusterPath: '/cluster',
    });
});

test.each(['0', '01', 'worker-A', '9007199254740992', 0, 3, Number.MAX_SAFE_INTEGER])('preserves opaque strings and converts safe numeric wire ID %s to string', async value => {
    expect(await respond(wire(value, value))).toMatchObject({ fastRankId: String(value), data: [{ rankId: String(value) }] });
});

test.each(['', -1, 1.5, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1, null, undefined, true])('rejects invalid rank ID %s without partial mutation', async value => {
    const result = wire();
    result.data[0].rankId = value;
    await expect(respond(result)).rejects.toThrow('Invalid rankId');
    expect(result.fastRankId).toBe('0');
    expect(result.data[0].rankId).toBe(value);
});

test.each(['', -1, 1.5, Number.MAX_SAFE_INTEGER + 1, null, undefined])('rejects invalid fastest ID %s when rows exist', async value => {
    const result = wire();
    result.fastRankId = value;
    await expect(respond(result)).rejects.toThrow('Invalid rankId');
    expect(result.data[0].rankId).toBe('3');
});

test.each([false, true])('allows the backend empty fastest ID with no rows (hasAdvice=%s)', async hasAdvice => {
    const result = { ...wire('3', '', hasAdvice), data: [] };
    expect(await respond(result)).toBe(result);
    expect(result.fastRankId).toBe('');
});

test('propagates transport failures to the caller', async () => {
    jest.mocked(window.requestDataDebounced).mockRejectedValueOnce(new Error('transport failed'));
    await expect(getSlowRankList(params)).rejects.toThrow('transport failed');
});
