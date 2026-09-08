import React from 'react';
import type { TFunction } from 'i18next';

interface OverviewStateProps {
    loading: boolean;
    hasData: boolean;
    error: string;
    t: TFunction<'numa'>;
}

export const OverviewState: React.FC<OverviewStateProps> = ({ loading, hasData, error, t }) => (
    <>
        {loading && (
            <div className="state-card"><span className="loader" />{t('loading')}</div>
        )}
        {!loading && !hasData && (
            <div className="state-card empty">
                <strong>NUMA</strong>
                <span>{error || t('noData')}</span>
            </div>
        )}
    </>
);
