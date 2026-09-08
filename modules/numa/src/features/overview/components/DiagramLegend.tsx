import React from 'react';
import type { TFunction } from 'i18next';

interface DiagramLegendProps {
    visible: boolean;
    t: TFunction<'numa'>;
}

export const DiagramLegend: React.FC<DiagramLegendProps> = ({ visible, t }) => {
    if (!visible) return null;
    return (
        <div className="legend">
            <span><i className="legend-line" />{t('communicationLink')}</span>
            <span><i className="legend-node numa" />NUMA</span>
            <span><i className="legend-node dram" />DRAM</span>
        </div>
    );
};
