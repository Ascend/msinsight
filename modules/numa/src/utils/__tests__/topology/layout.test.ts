import { buildDiagramLayout, connectionEndpoints, normalizeSocketColumns } from '@/features/topology/model/layout';
import { numaOverviewFixture } from '@/testUtils/numaOverview.fixture';

describe('topology layout', () => {
    it.each([
        [undefined, 2, 2],
        [Number.NaN, 2, 2],
        [0, 2, 1],
        [1.9, 2, 1],
        [99, 2, 2],
        [2, 0, 1],
        [2, Number.POSITIVE_INFINITY, 1],
    ])('normalizes %s columns for %s sockets', (columns, socketCount, expected) => {
        expect(normalizeSocketColumns(columns, socketCount)).toBe(expected);
    });

    it('builds stable two-column geometry and wraps extra rows', () => {
        const diagram = buildDiagramLayout(numaOverviewFixture.sockets, 2);
        expect(diagram.layouts.map(({ column, x, y }) => ({ column, x, y }))).toEqual([
            { column: 0, x: 150, y: 50 },
            { column: 1, x: 690, y: 50 },
        ]);
        expect(diagram.viewBox).toBe('0 0 1140 400');
        expect(diagram.nodeBoxes.get('numa-0')).toEqual({
            center: { x: 300, y: 140 }, width: 180, height: 60,
        });
        expect(diagram.nodeBoxes.get('memory-2')).toEqual({
            center: { x: 1052, y: 140 }, width: 54, height: 56,
        });

        const wrapped = buildDiagramLayout(numaOverviewFixture.sockets, 1);
        expect(wrapped.layouts[1]).toMatchObject({ column: 0, x: 150, y: 430 });
        expect(wrapped.viewBox).toBe('0 0 1140 780');
    });

    it('places a single NUMA node without invalid spacing', () => {
        const socket = {
            ...numaOverviewFixture.sockets[0],
            numas: [numaOverviewFixture.sockets[0].numas[0]],
        };
        expect(buildDiagramLayout([socket], 1).layouts[0].numaPoints.get(0)).toEqual({ x: 300, y: 140 });
    });

    it('clips connection endpoints to compatible node boundaries', () => {
        const { nodeBoxes } = buildDiagramLayout(numaOverviewFixture.sockets, 2);
        expect(connectionEndpoints(numaOverviewFixture.connections[0], nodeBoxes)).toEqual([
            { x: 450, y: 210 },
            { x: 690, y: 210 },
        ]);
        expect(connectionEndpoints(numaOverviewFixture.connections[1], nodeBoxes)).toEqual([
            { x: 300, y: 170 },
            { x: 300, y: 250 },
        ]);
        expect(connectionEndpoints({
            ...numaOverviewFixture.connections[0],
            source: 'numa-0',
            target: 'numa-1',
        }, nodeBoxes)).toBeNull();
        expect(connectionEndpoints({
            ...numaOverviewFixture.connections[0],
            target: 'socket-missing',
        }, nodeBoxes)).toBeNull();
    });
});
