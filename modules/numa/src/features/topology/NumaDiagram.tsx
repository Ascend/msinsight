import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { formatMetric, formatMetricValue } from '@/entities/numa/formatters';
import type { Connection, Metric, NumaOverview, SelectedItem, SocketNode as SocketNodeData } from '@/entities/numa/types';
import { ConnectionLine } from './components/ConnectionLine';
import { MemoryNode } from './components/MemoryNode';
import { NumaNode } from './components/NumaNode';
import { SocketNode } from './components/SocketNode';
import {
    DEFAULT_SOCKET_COLUMNS,
    MEMORY_WIDTH,
} from './model/constants';
import { buildConnectionMetricLabels, segmentsCross } from './model/connectionLabels';
import { buildDiagramLayout, connectionEndpoints, normalizeSocketColumns } from './model/layout';
import {
    selectedItemForConnection,
    selectedItemForNuma,
    selectedItemForSocket,
} from './model/selection';
import type { Point } from './model/types';

interface Props {
    data: NumaOverview;
    selected: SelectedItem | null;
    onSelect: (item: SelectedItem | null) => void;
    zoom: number;
    showConnectionMetrics?: boolean;
}

function findMetric(metrics: SocketNodeData['metrics'], key: string): Metric | undefined {
    return metrics.find((metric) => metric.key === key);
}

function hasCalculatedConnectionMetric(connection: Connection): boolean {
    return connection.type !== 'socket' || connection.metrics.some((metric) => metric.hasValue !== false);
}

export const NumaDiagram: React.FC<Props> = ({
    data,
    selected,
    onSelect,
    zoom,
    showConnectionMetrics = true,
}) => {
    const { t } = useTranslation('numa');
    const columns = normalizeSocketColumns(DEFAULT_SOCKET_COLUMNS, data.sockets.length);
    const { layouts, nodeBoxes, width: diagramWidth, height, viewBox } = useMemo(
        () => buildDiagramLayout(data.sockets, columns),
        [columns, data.sockets],
    );
    const connectionsByMemoryId = new Map(data.connections.filter((item) => item.type === 'memory')
        .map((item) => [item.target, item]));
    const connectionLayouts = useMemo(() => data.connections.filter(hasCalculatedConnectionMetric).flatMap((connection) => {
        const endpoints = connectionEndpoints(connection, nodeBoxes);
        return endpoints === null ? [] : [{ connection, source: endpoints[0], target: endpoints[1] }];
    }), [data.connections, nodeBoxes]);
    const crossingConnectionIds = useMemo(() => {
        const socketLayouts = connectionLayouts.filter(({ connection }) => connection.type === 'socket');
        const crossingIds = new Set<string>();
        socketLayouts.forEach((layout, index) => {
            socketLayouts.slice(index + 1).forEach((other) => {
                if (segmentsCross([layout.source, layout.target], [other.source, other.target])) {
                    crossingIds.add(layout.connection.id);
                    crossingIds.add(other.connection.id);
                }
            });
        });
        return crossingIds;
    }, [connectionLayouts]);

    const selectConnection = (connection: Connection): void => {
        onSelect(selectedItemForConnection(connection));
    };

    return (
        <svg
            className="numa-diagram"
            viewBox={viewBox}
            style={{ width: `${diagramWidth * zoom}px`, height: `${height * zoom}px` }}
            role="img"
            aria-label={t('diagram.ariaLabel')}
            onClick={() => onSelect(null)}
        >
            <defs>
                <marker id="arrow-bidirectional" markerWidth="6" markerHeight="6" refX="5.5" refY="3" orient="auto-start-reverse">
                    <path d="M0 0L6 3L0 6Z" className="arrow-marker" />
                </marker>
            </defs>
            <rect width={diagramWidth} height={height} rx="6" className="diagram-bg" />

            {layouts.map((layout) => (
                <SocketNode
                    key={`socket-background-${layout.socket.id}`}
                    layout={layout}
                    selected={selected?.id === `socket-${layout.socket.id}`}
                    onSelect={() => onSelect(selectedItemForSocket(layout.socket, t))}
                />
            ))}

            {connectionLayouts.map(({ connection, source, target }) => {
                const crossing = crossingConnectionIds.has(connection.id);
                return (
                    <ConnectionLine
                        key={connection.id}
                        connection={connection}
                        source={source}
                        target={target}
                        labels={buildConnectionMetricLabels(connection, source, target, crossing)}
                        showMetrics={showConnectionMetrics}
                        crossing={crossing}
                        selected={selected?.id === connection.id}
                        onSelect={() => selectConnection(connection)}
                    />
                );
            })}

            {layouts.map((layout) => {
                const memoryOnLeft = layout.column % 2 === 0;
                const externalImpact = findMetric(layout.socket.metrics, 'externalImpact');
                return (
                    <g key={layout.socket.id}>
                        <text x={layout.x + 24} y={layout.y + 34} className="socket-title">{layout.socket.name}</text>
                        {externalImpact && (
                            <text x={layout.x + layout.width - 24} y={layout.y + 34} className="socket-impact" textAnchor="end">
                                {formatMetricValue(externalImpact.value, externalImpact.unit)}
                            </text>
                        )}

                        {layout.socket.numas.map((numa) => {
                            const point = layout.numaPoints.get(numa.id) as Point;
                            const crossSocketRead = findMetric(numa.metrics, 'crossSocketRead');
                            const memoryId = `memory-${numa.id}`;
                            const memoryConnection = connectionsByMemoryId.get(memoryId);
                            const memoryX = memoryOnLeft
                                ? layout.x - 62 - MEMORY_WIDTH / 2
                                : layout.x + layout.width + 62 - MEMORY_WIDTH / 2;
                            return (
                                <React.Fragment key={numa.id}>
                                    <NumaNode
                                        numa={numa}
                                        point={point}
                                        selected={selected?.id === `numa-${numa.id}`}
                                        crossSocketValue={crossSocketRead
                                            ? formatMetric(crossSocketRead, t('noDataCollected'))
                                            : undefined}
                                        crossSocketLabel={t('diagram.crossSocket')}
                                        onSelect={() => onSelect(selectedItemForNuma(numa, layout.socket, t))}
                                    />
                                    <MemoryNode
                                        id={memoryId}
                                        x={memoryX}
                                        y={point.y}
                                        selected={selected?.id === memoryConnection?.id}
                                        onSelect={memoryConnection ? () => selectConnection(memoryConnection) : undefined}
                                    />
                                </React.Fragment>
                            );
                        })}
                    </g>
                );
            })}
        </svg>
    );
};
