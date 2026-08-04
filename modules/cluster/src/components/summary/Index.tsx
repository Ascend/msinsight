/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import { ExclamationCircleFilled } from '@ant-design/icons';
import type { Session } from '../../entity/session';
import { useEventBus } from '../../utils/eventBus';
import { Filter } from './Filter';
import { StatisticsTable } from './StatisticsTable';
import BaseInfo from './BaseInfo';
import { CommunicatorContainer } from '../communicatorContainer/CommunicatorContainer';
import { HelpIcon } from '@insight/lib/icon';
import { Tooltip, FormItem, CollapsiblePanel, Layout } from '@insight/lib/components';
import { PerformanceChart } from './PerformanceChart';
import { FlowChart } from './FlowChart';
import styled from '@emotion/styled';
import {
    getParallelismPerformanceDataCancelable,
    queryAllConnections,
    slowRankAdvisor,
} from '../../utils/RequestUtils';
import { runInAction } from 'mobx';
import { Communicator, partitionMode } from '../communicatorContainer/ContainerUtils';
import connector from '../../connection';
import { GetSlowRankAdviseRes, IndicatorsItem, PerformanceDataItem } from '../../utils/interface';
import { isEqual } from 'lodash';
import { ExpertLoadBalancingBox } from './expert-load-balancing';
import { ClusterSelect } from '../ClusterSelect';
import { SlowRankTable } from './SlowRankTable';
import parallelismStore, { defaultGenerateConditions } from '../../store/parallelism';

const FlowChartContainer = styled.div`
    margin-top: 24px;
    padding: 16px 24px;
    border: 1px solid ${(props): string => props.theme.borderColor};

    .title {
        color: ${(props): string => props.theme.textColorPrimary};
        margin-bottom: 20px;
        font-size: 16px;
        font-weight: 500;
    }
`;

export const useHit = (containsPreparing: boolean, showZoomTips: boolean): React.ReactElement => {
    const { t } = useTranslation('summary');
    const hit = t(containsPreparing ? 'Computation/CommunicationDescribeWithPreparing' : 'Computation/CommunicationDescribe',
        { returnObjects: true }) as string[];
    return (<Tooltip
        overlayClassName={'width-auto'}
        title={
            (
                <div style={{ padding: '1rem' }}>
                    {hit?.map((item: string, index: number) => <div key={index}>{item}</div>)}
                    <div style={{ marginTop: '2rem' }}>
                        <ExclamationCircleFilled style={{ marginRight: '10px' }}/>
                        {t('Computation/CommunicationLastDescribe')}
                    </div>
                    {showZoomTips && <div style={{ marginTop: '2rem' }}>{t('Chart Zoom Tooltip')}</div>}
                </div>
            )
        }>
        <HelpIcon style={{ cursor: 'pointer', marginLeft: '3px' }} height={20} width={20}/>
    </Tooltip>);
};

export interface PerformanceChartConditions {
    step: string;
    baselineStep?: string;
    orderBy: string;
    top: string;
    group: string; // 形式如：{rankId,rankId,...}
}

export interface CommunicatorData {
    name: string;
    value: string;
}

const defaultPerformanceChartConditions = {
    step: 'All',
    baselineStep: 'All',
    group: 'All',
    orderBy: 'rankId',
    top: 'All',
};

const defaultSlowRankRes = {
    hasSlowRank: false,
    matchSuccess: false,
    topNElements: [],
};

export const Index = observer(({ session, clusterPath }: { session: Session; clusterPath: string }): JSX.Element => {
    const { generateConditions } = parallelismStore;
    const { t } = useTranslation('summary');
    const [isPipeline, setIsPipeline] = useState(false);
    const tips = useHit(true, !isPipeline);
    const [activeRankId, setActiveRankId] = useState('');
    const [performanceLoading, setPerformanceLoading] = useState(false);
    const [slowRankData, setSlowRankData] = useState<GetSlowRankAdviseRes>(defaultSlowRankRes);
    const [adviceContent, setAdviceContent] = useState<string[]>([]);
    const [performanceChartConditions, setPerformanceChartConditions] = useState<PerformanceChartConditions>(defaultPerformanceChartConditions);
    const isDefaultGenerateConditions: boolean = useMemo(() => {
        return isEqual(generateConditions, defaultGenerateConditions);
    }, [JSON.stringify(generateConditions)]);
    const performancePanelRef = useRef<HTMLDivElement>(null);

    const getPerformanceData = async (): Promise<void> => {
        setPerformanceLoading(true);
        const params = {
            step: performanceChartConditions.step,
            baselineStep: session.isCompare ? performanceChartConditions.baselineStep : null,
            ...generateConditions,
            isCompare: session.isCompare,
        };

        const { invoke } = getParallelismPerformanceDataCancelable;
        const { performance, advice } = await invoke(params).finally(() => {
            setPerformanceLoading(false);
        });
        if (performance.length !== 0) {
            const curName = params.dimension === 'ep-dp-pp-cp-tp' ? 'Communication' : 'Avg Communication';
            session.dynamicsIndicatorList = Object.keys(performance[0].commTimeIndicator.diff).map(item => {
                return {
                    key: item,
                    name: curName,
                    unit: 'μs',
                } as IndicatorsItem;
            });
        }
        const performanceAfterDeal = performance.map(item => {
            return {
                index: item.index,
                ...item.indicators.compare,
                diff: item.indicators.diff,
                commCompare: item.commTimeIndicator.compare,
                commDiff: item.commTimeIndicator.diff,
            };
        });
        setAdviceContent(advice ?? []);
        runInAction(() => {
            if (performanceAfterDeal !== undefined) {
                session.performanceData = performanceAfterDeal;

                const map: Map<number, PerformanceDataItem> = new Map();
                performanceAfterDeal.forEach(item => {
                    map.set(item.index, item);
                });
                session.performanceDataMap = map;

                session.setRankDyeingData();
            }
        });
    };

    // 获取全展开的连线数据
    const getAllConnections = async (): Promise<void> => {
        const { connections } = await queryAllConnections({ ...generateConditions, dimension: 'ep-dp-pp-cp-tp', clusterPath });
        runInAction(() => {
            if (connections === undefined) {
                return;
            }

            const partitionModesMap: Map<string, Communicator[]> = new Map();
            connections.forEach(connection => {
                const { type, list } = connection;
                if (!partitionModesMap.has(type)) {
                    partitionModesMap.set(type, []);
                }

                partitionModesMap.get(type)?.push({
                    value: `(${list.join(', ')})`,
                });
            });

            const partitionModes: partitionMode[] = Array.from(partitionModesMap, ([key, value]) => (
                { mode: key, communicators: value }
            ));
            session.communicatorData = {
                clusterPath,
                partitionModes,
                defaultPPSize: 0,
            };
        });
        connector.send({ event: 'updateCommunicatorData', body: session.communicatorData, to: 'Communication' });
    };

    const getSlowRankData = async (): Promise<void> => {
        const params = {
            ...generateConditions,
        };
        const slowRankRes: GetSlowRankAdviseRes = await slowRankAdvisor(params);
        setSlowRankData(slowRankRes);
    };

    const handleFilterChange = (conditions: PerformanceChartConditions): void => {
        setPerformanceChartConditions(conditions);
    };

    useEventBus('activeCommunicator', (data): void => {
        const isPP = (data as CommunicatorData)?.name.startsWith('pipeline');
        setIsPipeline(isPP);

        const ppGroup = (data as CommunicatorData)?.value;
        if (ppGroup !== undefined) {
            setPerformanceChartConditions((prevState) => ({ ...prevState, group: ppGroup }));
            performancePanelRef.current?.scrollIntoView({ behavior: 'smooth', block: 'center' });
        }
    });

    useEventBus('resetPerformanceConditions', (): void => {
        setPerformanceChartConditions(defaultPerformanceChartConditions);
    });

    useEffect(() => {
        if (isDefaultGenerateConditions) {
            return;
        }
        getPerformanceData();
        getSlowRankData();
    }, [performanceChartConditions.step, performanceChartConditions.baselineStep, JSON.stringify(generateConditions), session.isCompare]);

    useEffect(() => {
        if (isDefaultGenerateConditions) {
            return;
        }
        getAllConnections();
    }, [
        generateConditions.algorithm,
        generateConditions.ppSize,
        generateConditions.tpSize,
        generateConditions.cpSize,
        generateConditions.dpSize,
        generateConditions.epSize,
    ]);

    useEffect(() => {
        parallelismStore.updateGenerateConditions(defaultGenerateConditions);
    }, [clusterPath]);

    return <Layout>
        <div style={{ padding: '0 24px 6px' }}>
            <FormItem label={t('Cluster')}>
                <ClusterSelect width={300} session={session}/>
            </FormItem>
        </div>

        <BaseInfo session={session}/>

        <CollapsiblePanel title={t('Parallel Strategy Analysis')}>
            <CommunicatorContainer
                session={session}
                loading={performanceLoading}
                clusterPath={clusterPath}
            />
            <SlowRankTable
                generateConditions={generateConditions}
                slowRankRes={slowRankData}
            />
            {!isDefaultGenerateConditions && <CollapsiblePanel
                ref={performancePanelRef}
                id="communication-overview-panel"
                secondary
                title={<div className={'flex items-center'}>{t('Computation/CommunicationOverview')}{tips}</div>}
                headerStyle={{ padding: 0 }}
                contentStyle={{ paddingLeft: 0, paddingRight: 0 }}
            >
                <Filter
                    session={session}
                    conditions={performanceChartConditions}
                    onFilterChange={handleFilterChange}
                    isPipeline={isPipeline}
                />
                {
                    isPipeline
                        ? <FlowChartContainer data-testid="pipeline-chart">
                            <div className="title">{t('Pipeline Parallelism Chart')}</div>
                            <FlowChart
                                step={performanceChartConditions.step}
                                stage={performanceChartConditions.group}
                                clusterPath={clusterPath}
                            />
                        </FlowChartContainer>
                        : <>
                            <div data-testid="performance-chart">
                                <PerformanceChart
                                    session={session}
                                    {...performanceChartConditions}
                                    {...generateConditions}
                                    loading={performanceLoading}
                                    setActiveRankId={setActiveRankId}
                                    advices={adviceContent}
                                />
                            </div>
                            {
                                generateConditions.dimension === 'ep-dp-pp-cp-tp' && !session.isCompare &&
                                session.rankDbPathMap.get(activeRankId) !== undefined &&
                                <StatisticsTable
                                    session={session}
                                    step={performanceChartConditions.step}
                                    rankId={activeRankId}
                                    dbPath={session.rankDbPathMap.get(activeRankId) as string}
                                />
                            }
                        </>
                }
            </CollapsiblePanel>
            }
        </CollapsiblePanel>

        <CollapsiblePanel title={<>
            {t('MoE Expert Load Balancing Analysis')}
            <Tooltip
                title={<div style={{ padding: '1rem' }}>
                    {t('MoE Expert Load Balancing Analysis Tooltip')}
                </div>}>
                <HelpIcon style={{ cursor: 'pointer', marginLeft: '3px' }} height={20} width={20}/>
            </Tooltip>
        </>} testId="panel-moe-balancing">
            <ExpertLoadBalancingBox />
        </CollapsiblePanel>
    </Layout>;
});

export default Index;
