import React from 'react';
import type { Connection } from '@/entities/numa/types';
import type { ConnectionMetricLabelLayout, Point } from '../model/types';

interface Props {
    connection: Connection;
    source: Point;
    target: Point;
    labels: ConnectionMetricLabelLayout[];
    showMetrics: boolean;
    crossing: boolean;
    selected: boolean;
    onSelect: () => void;
}

const DIRECTION_ARROWS = new Set(['←', '→', '↑', '↓']);

interface MetricTextParts {
    arrow?: string;
    arrowBefore?: boolean;
    value: string;
}

function metricTextParts(text: string): MetricTextParts {
    const parts = text.split(' ');
    const first = parts[0];
    if (DIRECTION_ARROWS.has(first)) {
        return { arrow: first, arrowBefore: true, value: parts.slice(1).join(' ') };
    }
    const last = parts[parts.length - 1];
    if (DIRECTION_ARROWS.has(last)) {
        return { arrow: last, arrowBefore: false, value: parts.slice(0, -1).join(' ') };
    }
    return { value: text };
}

const MetricLabelContent: React.FC<MetricTextParts> = ({ arrow, arrowBefore, value }) => {
    if (arrow === undefined) return <>{value}</>;
    if (arrowBefore === true) {
        return (
            <>
                <tspan className="connection-metric-arrow">{arrow}</tspan>
                <tspan>{` ${value}`}</tspan>
            </>
        );
    }
    return (
        <>
            <tspan>{`${value} `}</tspan>
            <tspan className="connection-metric-arrow">{arrow}</tspan>
        </>
    );
};

export const ConnectionLine: React.FC<Props> = ({
    connection,
    source,
    target,
    labels,
    showMetrics,
    crossing,
    selected,
    onSelect,
}) => (
    <g
        className={`connection ${connection.type} ${selected ? 'selected' : ''}`}
        onClick={(event) => {
            event.stopPropagation();
            onSelect();
        }}
        data-testid={`connection-${connection.id}`}
        data-crossing={crossing}
    >
        <title>{connection.label}</title>
        <line className="connection-hit" x1={source.x} y1={source.y} x2={target.x} y2={target.y} />
        <line
            className="connection-line"
            x1={source.x}
            y1={source.y}
            x2={target.x}
            y2={target.y}
            markerStart="url(#arrow-bidirectional)"
            markerEnd="url(#arrow-bidirectional)"
        />
        {showMetrics && labels.map((label) => {
            const textParts = metricTextParts(label.text);
            return (
                <text
                    key={label.key}
                    className="connection-metric-label"
                    x={label.position.x}
                    y={label.position.y}
                    textAnchor={label.textAnchor}
                    dominantBaseline="central"
                    transform={`rotate(${label.rotation} ${label.position.x} ${label.position.y})`}
                    data-progress={label.progress}
                >
                    <MetricLabelContent {...textParts} />
                </text>
            );
        })}
    </g>
);
