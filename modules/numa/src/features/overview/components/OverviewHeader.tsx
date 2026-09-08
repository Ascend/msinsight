import React from 'react';
import type { TFunction } from 'i18next';

interface OverviewHeaderProps {
    socketCount: number;
    numaCount: number;
    rangeLabel: string | null;
    t: TFunction<'numa'>;
}

export const OverviewHeader: React.FC<OverviewHeaderProps> = ({ socketCount, numaCount, rangeLabel, t }) => (
    <header className="hero">
        <div>
            <h1>{t('title')}</h1>
            <div className="eyebrow">
                {t('topologySummary', {
                    socketCount: socketCount || '--',
                    numaCount: numaCount || '--',
                })}
            </div>
        </div>
        {rangeLabel !== null && (
            <div className="range-chip">
                <span>{t('metricRange')}</span>
                <strong>{rangeLabel}</strong>
            </div>
        )}
    </header>
);
