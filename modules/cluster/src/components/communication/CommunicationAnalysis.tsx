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
import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Tooltip, CollapsiblePanel, Layout } from '@insight/lib/components';
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
import { GetSlowRankListResult } from '../../utils/interface';
import { clearCommunicationPageState, updateCommunicationPageState, setCommunicationAgentState } from '../../agent/communicationController';

interface showDataType {
    chartData: [];
    analysisChartData: AnalysisChartData;
    tableData: [];
    adviceData: CommunicationAdvice[];
}

const searchData = async (conditions: ConditionDataType & {isCompare: boolean}): Promise<showDataType> => {
    const notNullKeys: string[] = ['stage', 'operatorName', 'type'];
    if (!notNullObj(conditions, notNullKeys)) {
        return { chartData: [], analysisChartData: { minTime: 0, maxTime: 0, data: [] }, tableData: [], adviceData: [] };
    }
    const communicationOperatorData = await queryCommunicationOperatorLists({ ...conditions });
    const res = await queryCommunication(conditions);
    const { advice = [], items: data = [] } = res ?? {};
    // 默认按总时间降序排序
    data.sort((a: DataItem, b: DataItem) => conditions.isCompare
        ? b.compareData.diff.elapseTime - a.compareData.diff.elapseTime
        : b.compareData.compare.elapseTime - a.compareData.compare.elapseTime);
    return { analysisChartData: communicationOperatorData, tableData: data, chartData: data, adviceData: advice };
};

export interface CardInfo {
    cardId: string;
    dbPath: string;
}
const DEFAULT_CARD_INFO = { cardId: '', dbPath: '' };

const CommunicationAnalysis = observer(({ session, active = true }: { session: Session;active?: boolean }) => {
    const [card, setCard] = useState(DEFAULT_CARD_INFO);
    const [loading, setLoading] = useState(false);
    const [showData, setShowData] = useState<showDataType>({
        chartData: [],
        analysisChartData: {} as AnalysisChartData,
        tableData: [],
        adviceData: [],
    });
    const [conditions, setConditions] = useState<ConditionDataType>(defaultCondition);
    const showOperator = (newCard: CardInfo): void => {
        setCard(newCard);
    };
    const returnHome = (): void => { setCard(DEFAULT_CARD_INFO); };
    const handleFilterChange = async(newConditions: ConditionDataType): Promise<void> => {
        setConditions({ ...conditions, ...newConditions });
        if (newConditions.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
            return;
        }
        setLoading(true);
        const res = await searchData({ ...newConditions, isCompare: session.isCompare }).finally(() => {
            setLoading(false);
        });
        setShowData(res);
    };

    useEffect(() => {
        if (!active || !session.clusterCompleted) {
            clearCommunicationPageState();
            return;
        }
        updateCommunicationPageState({
            analysisType: conditions.type,
            scope: {
                iterationId: conditions.iterationId,
                baselineIterationId: conditions.baselineIterationId,
                stage: conditions.stage,
                pgName: conditions.pgName,
                groupIdHash: conditions.groupIdHash,
                baselineGroupIdHash: conditions.baselineGroupIdHash,
                operatorName: conditions.operatorName,
            },
            selection: card.cardId === '' ? null : { kind: 'rank-detail', rankId: card.cardId },
        });
        return clearCommunicationPageState;
    }, [session, session.clusterCompleted, active, conditions, card.cardId]);

    useEffect(() => {
        if (session.durationFileCompleted) {
            search();
        }
        async function search(): Promise<void> {
            if (showData.tableData.length === 0 && conditions.type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
                const res = await searchData({ ...conditions, isCompare: session.isCompare });
                setShowData(res);
            }
        }
    }, [session.durationFileCompleted]);

    return <CommunicationAnalysisCom
        session={session}
        loading={loading}
        handleFilterChange={handleFilterChange} showData={showData}
        active={active} showOperator={showOperator} setShowData={setShowData} conditions={conditions}
        rankId={card.cardId} dbPath={card.dbPath} returnHome={returnHome}
    />;
});

const CommunicationAnalysisCom = (props: {[propName: string]: any}): JSX.Element => {
    const {
        session, handleFilterChange, showData, active, showOperator,
        setShowData, conditions, rankId, dbPath, returnHome, loading,
    } = props;
    const { t } = useTranslation('communication');
    const [loadingSlowRank, setLoadingSlowRank] = useState(false);
    const [slowRankData, setSlowRankData] = useState<GetSlowRankListResult | null>(null);
    useEffect(() => {
        setCommunicationAgentState(active && session.clusterCompleted
            ? { conditions, slowRanks: slowRankData, chart: showData.analysisChartData, unitcount: session.unitcount, isCompare: session.isCompare }
            : undefined);
    }, [active, session, session.clusterCompleted, conditions, slowRankData, showData.analysisChartData, session.unitcount, session.isCompare]);
    useEffect(() => () => setCommunicationAgentState(undefined), []);
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
        <Layout>
            {/* 筛选条件 */}
            <div className="ml-24 mr-24">
                <Filter handleFilterChange={handleFilterChange} session={session} />
            </div>
            {/* 通信用时分析 */}
            <div className={'communication'} style={{ display: conditions.type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS ? 'block' : 'none' }}>
                <div>
                    <CollapsiblePanel title={t('sessionTitle.Communication')}>
                        <CommunicationTimeAnalysisChart dataSource={showData.analysisChartData} loading={loading} session={session}/>

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

                    <CommunicationTimeChart dataSource={showData.chartData} session={session}/>
                    <CommunicationTimeTable showOperator={showOperator} dataSource={showData.tableData} session={session}
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
    );
};

export default CommunicationAnalysis;
