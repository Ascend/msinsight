import React from 'react';
import { MEMORY_HEIGHT, MEMORY_WIDTH } from '../model/constants';

interface Props {
    id: string;
    x: number;
    y: number;
    selected: boolean;
    onSelect?: () => void;
}

export const MemoryNode: React.FC<Props> = ({ id, x, y, selected, onSelect }) => (
    <g
        className={`memory-node ${selected ? 'selected' : ''}`}
        onClick={(event) => {
            event.stopPropagation();
            onSelect?.();
        }}
        data-testid={id}
    >
        <rect x={x} y={y - MEMORY_HEIGHT / 2} width={MEMORY_WIDTH} height={MEMORY_HEIGHT} rx="10" />
        <text x={x + MEMORY_WIDTH / 2} y={y + 5} textAnchor="middle">DRAM</text>
    </g>
);
