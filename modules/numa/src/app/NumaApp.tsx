import React from 'react';
import { NumaOverviewPage } from '@/features/overview/NumaOverviewPage';
import type { NumaStore } from '@/features/overview/model/NumaStore';

interface NumaAppProps {
    store: NumaStore;
}

export const NumaApp: React.FC<NumaAppProps> = ({ store }) => (
    <NumaOverviewPage store={store} />
);
