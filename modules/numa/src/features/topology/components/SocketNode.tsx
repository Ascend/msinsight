import React from 'react';
import type { SocketLayout } from '../model/types';

interface Props {
    layout: SocketLayout;
    selected: boolean;
    onSelect: () => void;
}

export const SocketNode: React.FC<Props> = ({ layout, selected, onSelect }) => (
    <g
        className={`socket ${selected ? 'selected' : ''}`}
        onClick={(event) => {
            event.stopPropagation();
            onSelect();
        }}
        data-testid={`socket-${layout.socket.id}`}
    >
        <rect x={layout.x} y={layout.y} width={layout.width} height={layout.height} rx="20" />
    </g>
);
