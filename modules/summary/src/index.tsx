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

import React, { useEffect, useMemo, useState } from 'react';
import { observer } from 'mobx-react';
import { runInAction } from 'mobx';
import type { OverviewSession } from './session';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import { Button, Empty, FormItem, MIChart, Select, Spin, Tooltip } from '@insight/lib/components';
import { HelpIcon } from '@insight/lib/icon';
import i18n from '@insight/lib/i18n';
import { customConsole as console } from '@insight/lib/utils';
import { timeFields, useSummaryData, type SummaryKind, type SummaryResponse } from './api';
import { chartOptions, type RankOrder } from './charts';

const ChartGrid = styled.div<{ $time?: boolean }>`
    display: grid;
    grid-template-columns: ${(props): string => props.$time ? 'minmax(0, 1fr) minmax(0, 2fr)' : 'minmax(0, 3fr) minmax(0, 2fr)'};
    gap: 16px;
    > section { min-width: 0; }
    @media (max-width: 1100px) { grid-template-columns: minmax(0, 1fr); }
`;

const Page = styled.main`
    min-height: 100%;
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    padding: 16px;
    background-color: ${(props): string => props.theme.bgColorDark};
    color: ${(props): string => props.theme.textColorPrimary};
    section { min-width: 0; }
    @media (max-width: 600px) { padding: 8px; }
`;

const Surface = styled.div`
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 12px;
    padding-top: 16px;
    border-radius: 8px;
    background-color: ${(props): string => props.theme.mode === 'dark' ? props.theme.bgColor : props.theme.bgColorLighter};
    @media (max-width: 600px) { gap: 8px; }
`;

const Content = styled.div`
    flex: 1;
    min-width: 0;
    padding: 16px;
    background-color: ${(props): string => props.theme.mode === 'dark' ? props.theme.bgColor : props.theme.bgColorLighter};    border-radius: 8px;
    > div + div { margin-top: 20px; }
    @media (max-width: 600px) { padding: 8px; }
`;

const ChartHeading = styled.div<{ $alignPlot?: boolean }>`
    min-height: 40px; display: flex; flex-wrap: wrap;
    align-items: center; justify-content: space-between; gap: 8px;
    box-sizing: border-box;
    padding-right: ${(props): string => props.$alignPlot ? '32px' : '0'};
    h3 {
        margin: 0; font-size: 16px; font-weight: 600; line-height: 24px;
        display: flex; align-items: center; gap: 8px;
        color: ${(props): string => props.theme.textColorPrimary};
    }
`;

const HelpButton = styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    align-self: center;
    flex-shrink: 0;
    line-height: 0;
    padding: 0;
    width: 20px;
    height: 20px;
    border: 0;
    background: transparent;
    color: ${(props): string => props.theme.textColorSecondary};
    cursor: help;
    svg { display: block; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; outline-offset: 2px; }
`;

const Controls = styled.div`
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    column-gap: 24px;
    row-gap: 12px;
    padding: 0 12px;
    > div { min-width: 0; max-width: 100%; }
    .label { flex-shrink: 0; white-space: nowrap; line-height: 32px; }
    .content { min-width: 0; flex: 1; }
    .ant-select { max-width: 100%; }
    .ant-select-single:not(.ant-select-customize-input) .ant-select-selector { height: 32px; }
    .ant-select-single .ant-select-selection-item,
    .ant-select-single .ant-select-selection-placeholder { line-height: 30px; }
    @media (max-width: 600px) { padding: 0 8px; column-gap: 16px; }
`;

const Status = styled.div`
    min-height: 380px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    gap: 12px;
`;

function hasRows(data: SummaryResponse): boolean {
    return 'time' in data || ('ranks' in data ? data.ranks.length > 0 : data.transportTypes.length > 0);
}

interface ChartPanelProps {
    kind: SummaryKind;
    title: string;
    session: OverviewSession;
    label: (key: string) => string;
    help?: React.ReactNode;
}

const ChartPanel = observer(({ kind, title, session, label, help }: ChartPanelProps): JSX.Element => {
    const [order, setOrder] = useState<RankOrder>(kind === 'communicationTime' ? 'total' : 'rank');
    const { status, data, retry } = useSummaryData(kind, { clusterPath: session.selectedClusterPath, step: session.summaryStep }, session.ready);
    const selected = kind === 'clusterTime' || kind === 'rankTime' ? session.selectedTimeField : undefined;
    const options = useMemo(() => data && chartOptions(kind, data, label, order, selected), [kind, data, label, order, selected]);
    const events = useMemo(() => kind === 'clusterTime'
        ? {
            click: (event?: { componentType?: string; seriesType?: string; dataIndex?: number }): void => {
                if (typeof event?.dataIndex !== 'number' || event.componentType !== 'series' || event.seriesType !== 'pie') return;
                const field = timeFields[event.dataIndex];
                if (field) session.selectTimeField(field);
            },
        }
        : undefined, [kind, session]);
    const sortable = kind === 'rankTime' || kind === 'communicationTime';
    return <section aria-label={label(title)}>
        <ChartHeading $alignPlot={kind === 'communicationTime'}>
            <h3>{label(title)}{help}</h3>
            {sortable && <FormItem label={label('Sort')}>
                <Select
                    aria-label={`${label(title)} ${label('Sort')}`}
                    value={order}
                    onChange={(value: RankOrder): void => setOrder(value)}
                    style={{ width: 200, fontSize: 14 }}
                    options={[
                        { value: 'rank', label: label('Rank order') },
                        { value: 'total', label: label('Total time descending') },
                    ]}
                />
            </FormItem>}
        </ChartHeading>
        <div aria-busy={status === 'loading'}>
            {status === 'loading' && <Status role="status" aria-label={label('Loading data')}><Spin /></Status>}
            {status === 'error' && <Status role="alert">
                <span>{label('Request failed')}</span>
                <Button onClick={retry}>{label('Retry')}</Button>
            </Status>}
            {status === 'idle' && <Status><Empty description={label('Select a parsed cluster')} /></Status>}
            {status === 'success' && data && (hasRows(data)
                ? <MIChart options={options ?? {}} onEvents={events} width="100%" height="380px" />
                : <Status><Empty description={label('No data')} /></Status>)}
        </div>
    </section>;
});

export default observer(function Summary({ session, onClusterChange }: {
    session: OverviewSession; onClusterChange?: (path: string) => void;
}): JSX.Element {
    const locale = session.language;
    const { t: label } = useTranslation('summary', { lng: locale, keyPrefix: 'modern' });
    const descriptions = i18n.getFixedT(locale, 'summary')('Computation/CommunicationDescribeWithPreparing', { returnObjects: true }) as string[];
    const shared = { session, label };
    return <Page key={session.renderId}><Surface>
        <Controls>
            <FormItem label={i18n.getFixedT(locale, 'summary')('Cluster')} style={{ marginBottom: 0 }}>
                <Select aria-label={i18n.getFixedT(locale, 'summary')('Cluster')} style={{ width: 300 }} value={session.selectedClusterPath}
                    onChange={(path: string): void => {
                        if (path === session.selectedClusterPath || !session.clusterList.some(cluster => cluster.path === path)) return;
                        session.selectCluster(path);
                        onClusterChange?.(path);
                    }}
                    options={session.clusterList.map(cluster => ({
                        value: cluster.path,
                        label: cluster.parsed && cluster.durationParsed ? cluster.name : label('Parsing cluster', { name: cluster.name }),
                    }))} />
            </FormItem>
            <FormItem label={label('Step')} style={{ marginBottom: 0 }}>
                <Select
                    aria-label={label('Step')}
                    value={session.summaryStep}
                    onChange={(step: string): void => { runInAction(() => { session.summaryStep = step; }); }}
                    disabled={!session.ready}
                    style={{ width: 160 }}
                    options={[
                        { value: 'All', label: label('All') },
                        ...Array.from(new Set(session.stepList)).filter(value => value !== 'All').map(value => ({ value, label: value })),
                    ]}
                />
            </FormItem>
        </Controls>
        <Content>
            <BaseInfo session={session} />
            <ChartGrid $time>
                <ChartPanel {...shared} kind="clusterTime" title="Cluster time analysis" help={
                    <Tooltip trigger={['hover', 'focus']} placement="bottomLeft"
                        overlayStyle={{ maxWidth: 'calc(100vw - 32px)' }}
                        overlayInnerStyle={{ maxWidth: 900, padding: 16 }}
                        title={<div>{descriptions.map(description => <div key={description}>{description}</div>)}</div>}>
                        <HelpButton type="button" aria-label={label('Time metric definitions')}>
                            <HelpIcon width={20} height={20} aria-hidden="true" />
                        </HelpButton>
                    </Tooltip>
                } />
                <ChartPanel {...shared} kind="rankTime" title="Rank time analysis" />
            </ChartGrid>
            <ChartGrid>
                <ChartPanel {...shared} kind="communicationTime" title="Communication time analysis" />
                <ChartPanel {...shared} kind="clusterBandwidth" title="Cluster bandwidth analysis" />
            </ChartGrid>
        </Content>
    </Surface></Page>;
});

const ClusterCard = styled.section`
    margin: 0 0 16px;
    h3 {
        margin: 0 0 12px; font-size: 16px; font-weight: 600;
        color: ${(props): string => props.theme.textColorPrimary};
    }
    dl {
        display: grid; grid-template-columns: repeat(4, minmax(0, 1fr));
        margin: 0; padding: 18px 0;
        border: 1px solid ${(props): string => props.theme.mode === 'dark' ? props.theme.borderColorLighter : props.theme.borderColor};
        border-radius: 8px;
    }
    dl > div { padding: 0 24px; min-width: 0; }
    dl > div + div { border-left: 1px solid ${(props): string => props.theme.mode === 'dark' ? props.theme.borderColorLighter : props.theme.borderColor}; }
    dt { font-size: 12px; color: ${(props): string => props.theme.textColorSecondary}; margin-bottom: 6px; }
    dd { margin: 0; font-size: 20px; font-weight: 600; overflow-wrap: anywhere; }
    small { font-size: 12px; font-weight: 400; margin-left: 5px; color: ${(props): string => props.theme.textColorSecondary}; }
    @media (max-width: 900px) {
        dl { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 20px 0; }
        dl > div:nth-child(3) { border-left: 0; }
    }
    @media (max-width: 480px) {
        dl { grid-template-columns: minmax(0, 1fr); }
        dl > div + div { border-left: 0; }
    }
`;

function formatDate(date: Date): string {
    if (!Number.isFinite(date.getTime())) return '-';
    const pad = (value: number): string => String(value).padStart(2, '0');
    return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
}

const BaseInfo = observer(({ session }: { session: OverviewSession }): JSX.Element => {
    const { t: label } = useTranslation('summary', { lng: session.language, keyPrefix: 'modern' });
    const [info, setInfo] = useState<Record<string, React.ReactNode>>({});
    useEffect(() => {
        setInfo({});
        if (!session.ready) return;
        const controller = new AbortController();
        const renderId = session.renderId;
        void window.requestData('summary/queryTopData', {
            clusterPath: session.selectedClusterPath, isCompare: false, stepIdList: [],
        }, undefined, { signal: controller.signal }).then(response => {
            if (controller.signal.aborted || session.renderId !== renderId) return;
            const data = response.baseInfo.compare;
            runInAction(() => {
                session.stepList = data.stepList;
            });
            const duration = Number(data.collectDuration);
            setInfo({
                collectStartTime: data.collectStartTime == null ? '-' : formatDate(new Date(data.collectStartTime)),
                collectDuration: data.collectDuration == null || !Number.isFinite(duration) ? '-' : (duration / 60000000).toFixed(2),
                rankCount: data.rankCount,
                stepNum: data.stepNum,
            });
        }).catch(error => { if (!controller.signal.aborted && error?.name !== 'AbortError') console.error(error); });
        return () => controller.abort();
    }, [session.renderId, session.ready, session.selectedClusterPath]);
    const metrics = [
        ['collectStartTime', 'Collection time', ''], ['collectDuration', 'Collection duration', 'minutes'],
        ['rankCount', 'Device count', 'devices'], ['stepNum', 'Iteration count', 'iterations'],
    ];
    return <ClusterCard aria-label={label('Cluster basic information')}>
        <h3>{label('Cluster basic information')}</h3>
        <dl>{metrics.map(([key, title, unit]) => <div key={key}>
            <dt>{label(title)}</dt>
            <dd><span>{info[key] ?? '-'}</span>{unit && <small>{label(unit)}</small>}</dd>
        </div>)}</dl>
    </ClusterCard>;
});
