/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import React from 'react';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import { formatTimelineTimestamp } from '@/entities/numa/formatters';
import { NumaDiagram } from '@/features/topology/NumaDiagram';
import { DetailsPanel } from './components/DetailsPanel';
import { DiagramLegend } from './components/DiagramLegend';
import { DiagramToolbar } from './components/DiagramToolbar';
import { OverviewHeader } from './components/OverviewHeader';
import { OverviewState } from './components/OverviewState';
import { SystemMetrics } from './components/SystemMetrics';
import { ZOOM_STEP } from './model/NumaStore';
import type { NumaStore } from './model/NumaStore';
import { hasActiveTimeRange } from './model/timeRange';

interface NumaOverviewPageProps {
    store: NumaStore;
}

export const NumaOverviewPage: React.FC<NumaOverviewPageProps> = observer(({ store }) => {
    const { t } = useTranslation('numa');
    const data = store.data;
    const selected = store.selected;
    const detailMetrics = selected?.metrics ?? data?.totalMetrics ?? [];
    const detailTitle = selected?.title ?? t('systemMetrics');
    const detailDescription = selected?.description ?? t('selectTarget');
    const hasData = data !== null && data.sockets.length > 0;
    const socketConnections = data?.connections.filter((connection) => connection.type === 'socket') ?? [];
    const showProtocolAdapterNotice = socketConnections.length > 0 &&
        socketConnections.every((connection) => connection.metrics.every((metric) => metric.hasValue === false));
    const socketCount = data?.sockets.length ?? 0;
    const numaCount = data?.sockets.reduce((sum, socket) => sum + socket.numas.length, 0) ?? 0;
    const rangeLabel = data === null
        ? null
        : hasActiveTimeRange(store.analysisRange)
            ? `${formatTimelineTimestamp(store.analysisRange.startTime, 0)} - ${formatTimelineTimestamp(store.analysisRange.endTime, 0)}`
            : `${formatTimelineTimestamp(data.range.startTime, data.range.startTime)} - ${formatTimelineTimestamp(data.range.endTime, data.range.startTime)}`;

    const handleDiagramWheel = (event: React.WheelEvent<HTMLDivElement>): void => {
        if (!event.ctrlKey && !event.metaKey) return;
        event.preventDefault();
        store.changeZoom(store.zoom + (event.deltaY < 0 ? ZOOM_STEP : -ZOOM_STEP));
    };

    return (
        <main className="app-shell">
            <OverviewHeader
                socketCount={socketCount}
                numaCount={numaCount}
                rangeLabel={rangeLabel}
                t={t}
            />

            <SystemMetrics
                metrics={data?.totalMetrics ?? null}
                locale={store.locale}
                onSelect={() => store.select(null)}
                t={t}
            />

            <section className="workspace">
                <div className="diagram-panel">
                    <OverviewState loading={store.loading} hasData={hasData} error={store.error} t={t} />
                    {!store.loading && data && hasData && (
                        <>
                            <DiagramToolbar
                                zoom={store.zoom}
                                showConnectionMetrics={store.showConnectionMetrics}
                                onChangeZoom={store.changeZoom}
                                onToggleConnectionMetrics={store.toggleConnectionMetrics}
                                t={t}
                            />
                            {showProtocolAdapterNotice && (
                                <div className="protocol-adapter-notice" role="status">
                                    {t('protocolAdapterUnavailable')}
                                </div>
                            )}
                            <div className="diagram-scroll" onWheel={handleDiagramWheel}>
                                <div className="diagram-stage">
                                    <NumaDiagram
                                        data={data}
                                        selected={selected}
                                        zoom={store.zoom}
                                        showConnectionMetrics={store.showConnectionMetrics}
                                        onSelect={store.select}
                                    />
                                </div>
                            </div>
                        </>
                    )}
                    <DiagramLegend visible={hasData} t={t} />
                </div>

                <DetailsPanel
                    title={detailTitle}
                    description={detailDescription}
                    metrics={detailMetrics}
                    locale={store.locale}
                    t={t}
                />
            </section>
        </main>
    );
});
