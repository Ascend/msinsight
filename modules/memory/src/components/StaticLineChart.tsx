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

import React, { useState, useEffect } from 'react';
import { observer } from 'mobx-react-lite';
import { runInAction } from 'mobx';
import { SearchBox } from '../utils/styleUtils';
import { Label } from './Common';
import { Select, Spin } from '@insight/lib/components';
import { StyledEmpty, customConsole as console } from '@insight/lib/utils';
import { useTranslation } from 'react-i18next';
import { Graph, StaticOperatorCurve } from '../entity/memory';
import { LineChart } from './LineChart';
import { Session } from '../entity/session';
import { MemorySession } from '../entity/memorySession';
import { staticOpMemoryGraphGet } from '../utils/RequestUtils';

const StaticLineChart = observer(({ session, memorySession, isDark }:
{ session: Session; memorySession: MemorySession; isDark: boolean }) => {
    const isCompare: boolean = session.compareRank.isCompare;
    // 静态图曲线数据源
    const [memoryStaticCurveData, setMemoryStaticCurveData] = useState<StaticOperatorCurve | undefined>(undefined);
    // 静态图曲线绘制数据
    const [staticLineChartData, setStaticLineChartData] = useState<Graph | undefined>(undefined);
    const [staticCurveSpin, setStaticCurveSping] = useState<boolean>(false);
    const { t } = useTranslation('memory');

    const onMemoryGraphIdChanged = (value: string): void => {
        runInAction(() => {
            memorySession.memoryGraphId = value;
            memorySession.staticSelectedRange = undefined;
            memorySession.searchEventOperatorName = '';
            memorySession.current = 1;
            memorySession.pageSize = 10;
        });
    };

    const onStaticSelectedRangeChanged = (start: number, end: number): void => {
        runInAction(() => {
            if (start > end || !memoryStaticCurveData) {
                memorySession.staticSelectedRange = undefined;
                return;
            }
            const allDataListSet = new Set(memoryStaticCurveData.lines
                .map(item => {
                    return parseFloat(item[0] as string);
                }).sort((a, b) => a - b));
            if (allDataListSet.size <= 1) {
                memorySession.staticSelectedRange = undefined;
                return;
            }
            const allDataList = Array.from(allDataListSet);
            memorySession.staticSelectedRange = { startTs: allDataList[start], endTs: allDataList[end] };
            memorySession.current = 1;
            memorySession.pageSize = 10;
        });
    };

    useEffect(() => {
        if (memorySession.selectedRankId === '' || memorySession.memoryGraphId === '') {
            setStaticLineChartData(undefined);
            setMemoryStaticCurveData(undefined);
            return;
        }
        setStaticCurveSping(true);
        const selectedCard = memorySession.getSelectedRankValue();
        staticOpMemoryGraphGet({ rankId: selectedCard.rankInfo.rankId, dbPath: selectedCard.dbPath, graphId: memorySession.memoryGraphId, isCompare }).then((resp) => {
            // Reset the select range to null when rankId changes
            runInAction(() => {
                memorySession.staticSelectedRange = undefined;
            });
            setMemoryStaticCurveData(resp);
            setStaticLineChartData({
                columns: resp.legends?.map(legend => t(legend)),
                rows: resp.lines,
            });
        }).catch(err => {
            console.error(err);
        }).finally(() => {
            setStaticCurveSping(false);
        });
    }, [memorySession.selectedRankId, session.isAllMemoryCompletedSwitch, memorySession.memoryGraphId, t, isCompare]);

    return (
        <div className="mb-30">
            <SearchBox>
                <div className="flex items-center">
                    <Label name={t('searchCriteria.GraphId')} />
                    <Select
                        id={'select-graphId'}
                        value={memorySession.memoryGraphId}
                        size="middle"
                        onChange={onMemoryGraphIdChanged}
                        options={memorySession.memoryGraphIdList.map((graphId: string) => {
                            return {
                                value: graphId,
                                label: graphId,
                            };
                        })}
                    />
                </div>
            </SearchBox>
            <Spin spinning={staticCurveSpin}>
                { staticLineChartData
                    ? <LineChart
                        hAxisTitle={t('Node Index')}
                        vAxisTitle={t('Memory Usage (MB)')}
                        graph={staticLineChartData}
                        onSelectionChanged={onStaticSelectedRangeChanged}
                        record={memorySession.selectedStaticRecord}
                        isDark={isDark}
                        isStatic={true}
                    />
                    : <StyledEmpty style={{ marginTop: 160 }} translation={t}/>
                }
            </Spin>
        </div>
    );
});

export default StaticLineChart;
