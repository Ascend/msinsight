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
import { observer } from 'mobx-react-lite';
import React, { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Tooltip, CollapsiblePanel, Layout, Spin } from '@insight/lib/components';
import type { Session } from '../../entity/session';
import Filter, { AnalysisType, defaultCondition } from './Filter';
import type { ConditionDataType } from './Filter';
import CommunicationTimeTable from './CommunicationTimeTable';
import CommunicationTimeChart, { type DataItem } from './CommunicationTimeChart';
import CommunicationMatrix from './CommunicationMatrix';
import { notNullObj } from '../Common';
import { getSlowRankList, queryCommunication, queryCommunicationOperatorLists } from '../../utils/RequestUtils';
import CommunicationTimeAnalysisChart from './CommunicationTimeAnalysisChart';
import type { AnalysisChartData } from './CommunicationTimeAnalysisChart';
import { HelpIcon } from '@insight/lib/icon';
import AdviceLabel, { type CommunicationAdvice } from './CommunicationDuration/AdviceLabel';
import Operators from './CommunicationDuration/Opertators';
import DiffTimeTable from './DiffTimeTable';
import { ClickOperatorItem, GetSlowRankListResult } from '../../utils/interface';

interface ShowDataType {
    chartData: DataItem[];
    analysisChartData: AnalysisChartData;
    tableData: DataItem[];
    adviceData: CommunicationAdvice[];
}

const getEmptyShowData = (): ShowDataType => ({
    chartData: [],
    analysisChartData: { minTime: 0, maxTime: 0, data: [] },
    tableData: [],
    adviceData: [],
});

const parseDurationData = (res: any, isCompare: boolean): Pick<ShowDataType, 'chartData' | 'tableData' | 'adviceData'> => {
    const { advice = [], items = [] } = res ?? {};
    const data: DataItem[] = [...items];
    // 默认按总时间降序排序
    data.sort((a: DataItem, b: DataItem) => isCompare
        ? b.compareData.diff.elapseTime - a.compareData.diff.elapseTime
        : b.compareData.compare.elapseTime - a.compareData.compare.elapseTime);
    return { tableData: data, chartData: data, adviceData: advice };
};

export interface CardInfo {
    cardId: string;
    dbPath: string;
}
const DEFAULT_CARD_INFO = { cardId: '', dbPath: '' };

const CommunicationAnalysis = observer(({ session, active = true }: { session: Session;active?: boolean }) => {
    const [card, setCard] = useState(DEFAULT_CARD_INFO);
    const [operatorLoading, setOperatorLoading] = useState(false);
    const [durationLoading, setDurationLoading] = useState(false);
    const [filterLoading, setFilterLoading] = useState(false);
    const [showData, setShowData] = useState<ShowDataType>(getEmptyShowData);
    const [conditions, setConditions] = useState<ConditionDataType>(defaultCondition);
    const latestOperatorRequestId = useRef(0);
    const latestDurationRequestId = useRef(0);
    const showOperator = (newCard: CardInfo): void => {
        setCard(newCard);
    };
    const returnHome = (): void => { setCard(DEFAULT_CARD_INFO); };
    const handleFilterChange = (newConditions: ConditionDataType): void => {
        setConditions(previousConditions => ({ ...previousConditions, ...newConditions }));
        const operatorRequestId = ++latestOperatorRequestId.current;
        const durationRequestId = ++latestDurationRequestId.current;
        if (newConditions.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
            setOperatorLoading(false);
            setDurationLoading(false);
            return;
        }
        const notNullKeys: string[] = ['stage', 'operatorName', 'type'];
        if (!notNullObj(newConditions, notNullKeys)) {
            setShowData(getEmptyShowData());
            setOperatorLoading(false);
            setDurationLoading(false);
            return;
        }
        setOperatorLoading(true);
        setDurationLoading(true);
        const requestConditions = { ...newConditions, isCompare: session.isCompare };

        void queryCommunicationOperatorLists(requestConditions)
            .then(data => {
                if (operatorRequestId === latestOperatorRequestId.current) {
                    setShowData(previousData => ({ ...previousData, analysisChartData: data }));
                }
            })
            .catch(() => undefined)
            .finally(() => {
                if (operatorRequestId === latestOperatorRequestId.current) {
                    setOperatorLoading(false);
                }
            });

        void queryCommunication(requestConditions)
            .then(res => {
                if (durationRequestId === latestDurationRequestId.current) {
                    setShowData(previousData => ({ ...previousData, ...parseDurationData(res, session.isCompare) }));
                }
            })
            .catch(() => undefined)
            .finally(() => {
                if (durationRequestId === latestDurationRequestId.current) {
                    setDurationLoading(false);
                }
            });
    };

    const handleOperatorAlignment = (targetOperator?: ClickOperatorItem): void => {
        const requestId = ++latestOperatorRequestId.current;
        setOperatorLoading(true);
        void queryCommunicationOperatorLists({
            ...conditions,
            targetOperatorName: targetOperator?.name ?? '',
            isCompare: session.isCompare,
        })
            .then(data => {
                if (requestId === latestOperatorRequestId.current) {
                    setShowData(previousData => ({ ...previousData, analysisChartData: data }));
                }
            })
            .catch(() => undefined)
            .finally(() => {
                if (requestId === latestOperatorRequestId.current) {
                    setOperatorLoading(false);
                }
            });
    };

    useEffect(() => () => {
        latestOperatorRequestId.current += 1;
        latestDurationRequestId.current += 1;
    }, []);

    return <CommunicationAnalysisCom
        session={session}
        operatorLoading={operatorLoading}
        durationLoading={durationLoading}
        filterLoading={filterLoading}
        onFilterLoadingChange={setFilterLoading}
        onOperatorAlignment={handleOperatorAlignment}
        handleFilterChange={handleFilterChange} showData={showData}
        active={active} showOperator={showOperator} setShowData={setShowData} conditions={conditions}
        rankId={card.cardId} dbPath={card.dbPath} returnHome={returnHome}
    />;
});

const CommunicationAnalysisCom = (props: {[propName: string]: any}): JSX.Element => {
    const {
        session, handleFilterChange, showData, active, showOperator,
        setShowData, conditions, rankId, dbPath, returnHome, operatorLoading, durationLoading, filterLoading, onFilterLoadingChange,
        onOperatorAlignment,
    } = props;
    const { t } = useTranslation('communication');
    const [loadingSlowRank, setLoadingSlowRank] = useState(false);
    const [slowRankData, setSlowRankData] = useState<GetSlowRankListResult | null>(null);
    const slowRankTooltipContent = t('slowRankList.TitleTooltip', { returnObjects: true }) as string[];
    const slowRankTooltipList = slowRankTooltipContent.map((item, index) => <div style={{ padding: '6px 0' }} key={index}>{item}</div>);

    useEffect(() => {
        const fetchData = async (): Promise<void> => {
            setLoadingSlowRank(true);
            const res = await getSlowRankList(conditions).finally(() => {
                setLoadingSlowRank(false);
            });
            setSlowRankData(res);
        };

        if (conditions.type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
            fetchData();
        }
    }, [conditions]);

    return (
        <Spin spinning={filterLoading} delay={200}>
            <Layout>
                {/* 筛选条件 */}
                <div className="ml-24 mr-24">
                    <Filter handleFilterChange={handleFilterChange} session={session} onLoadingChange={onFilterLoadingChange} />
                </div>
                {/* 通信用时分析 */}
                <div className={'communication'} style={{ display: conditions.type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS ? 'block' : 'none' }}>
                    <div>
                        <CollapsiblePanel title={t('sessionTitle.Communication')}>
                            <CommunicationTimeAnalysisChart dataSource={showData.analysisChartData}
                                loading={operatorLoading} onAlignmentChange={onOperatorAlignment} session={session}/>

                            {
                                slowRankData?.hasAdvice && !session.isCompare
                                    ? <CollapsiblePanel
                                        title={t('sessionTitle.Potential Slow Rank List')}
                                        secondary
                                        collapsible
                                        tooltip={slowRankTooltipList}
                                    >
                                        <DiffTimeTable
                                            fastTotalElapseTime={slowRankData.fastTotalElapseTime}
                                            fastRankId={slowRankData.fastRankId}
                                            data={slowRankData.data}
                                            loading={loadingSlowRank}
                                        ></DiffTimeTable>
                                    </CollapsiblePanel>
                                    : null
                            }
                        </CollapsiblePanel>

                        <CommunicationTimeChart dataSource={showData.chartData} session={session} loading={durationLoading}/>
                        <CommunicationTimeTable showOperator={showOperator} dataSource={showData.tableData} session={session}
                            loading={durationLoading}
                            conditions={conditions} updateSort={(data): void => {
                                setShowData({ ...showData, chartData: data });
                            }}/>
                        { showData.adviceData.length > 0 && <AdviceLabel adviceData={showData.adviceData}/> }
                    </div>
                </div>
                {/* 通信矩阵 */}
                <CommunicationMatrix isShow={conditions.type === AnalysisType.COMMUNICATION_MATRIX && active} conditions={conditions} session={session}/>
                {/* 带宽分析 */}
                { rankId !== '' && <Operators iterationId={conditions.iterationId} rankId={rankId} dbPath={dbPath}
                    session={session} returnHome={returnHome}
                    operatorName={conditions.operatorName} stage={conditions.stage} pgName={conditions.pgName} groupIdHash={conditions.groupIdHash}/> }
                <div style={{ position: 'absolute', top: '15px', right: '20px' }}>
                    <Tooltip
                        placement="left"
                        title={
                            (
                                <div style={{ padding: '10px' }}>
                                    {t('Tooltip')}
                                </div>
                            )
                        }>
                        <HelpIcon style={{ cursor: 'pointer', marginLeft: '3px' }} height={20} width={20}/>
                    </Tooltip>
                </div>
            </Layout>
        </Spin>
    );
};

export default CommunicationAnalysis;
