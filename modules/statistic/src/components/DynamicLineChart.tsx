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
import { Spin, CollapsiblePanel } from '@insight/lib/components';
import { StyledEmpty, customConsole as console } from '@insight/lib/utils';
import { useTranslation } from 'react-i18next';
import { Graph, Curve } from '../entity/curve';
import { LineChart } from './LineChart';
import { CurveSession } from '../entity/curveSession';
import { Session } from '../entity/session';
import { curveGet } from '../utils/RequestUtils';

/**
 * 国际化-中文
 */
const LANGUAGE_ZH = 'zhCN';

const DynamicLineChart = observer(({ session, curveSession, isDark }:
{ session: Session; curveSession: CurveSession; isDark: boolean }) => {
    // 内存曲线数据源
    const [curveData, setCurveData] = useState<Curve | undefined>(undefined);
    // 内存曲线绘制数据
    const [lineChartData, setLineChartData] = useState<Graph | undefined>(undefined);
    // 内存曲线绘制描述
    const [description, setDescription] = useState<string>('');

    const [curveSpin, setCurveSpin] = useState<boolean>(false);
    const { t } = useTranslation('statistic');

    const onSelectedRangeChanged = (start: number, end: number): void => {
        runInAction(() => {
            if (start > end || !curveData) {
                curveSession.selectedRange = undefined;
                return;
            }
            const allDataListSet = new Set(curveData.lines
                .map(item => {
                    return item[0] as string;
                }));
            if (allDataListSet.size <= 1) {
                curveSession.selectedRange = undefined;
                return;
            }
            const allDataList = Array.from(allDataListSet);
            curveSession.selectedRange = { startTs: allDataList[start].toString(), endTs: allDataList[end].toString() };
            curveSession.current = 1;
            curveSession.pageSize = 10;
        });
    };

    const onMemoryCurveGet = (): void => {
        setCurveSpin(true);
        curveGet({ rankId: curveSession.rankIdCondition.value, type: curveSession.groupId, isZh: session.language === LANGUAGE_ZH }).then((resp) => {
            // Reset the select range to null when rankId changes
            runInAction(() => {
                curveSession.selectedRange = undefined;
            });
            setCurveData(resp);
            let columns: string[] = [];
            columns = resp.legends?.map(legend => t(legend));
            setLineChartData({
                columns,
                rows: resp.lines,
            });
            setDescription(resp.description as string);
        }).catch(err => {
            console.error(err);
        }).finally(() => {
            setCurveSpin(false);
        });
    };

    useEffect(() => {
        if (curveSession.rankIdCondition.value === undefined || curveSession.rankIdCondition.value === '') {
            setLineChartData(undefined);
            setCurveData(undefined);
            return;
        }
        onMemoryCurveGet();
    }, [curveSession.rankIdCondition.value, curveSession.groupId, session.language]);

    return (
        <div className="mb-30">
            <div
                style={{ marginLeft: 25, marginBottom: 5 }}>
                <span> {t('Curve Description')}: </span>
                <span> {description} </span>
            </div>
            <CollapsiblePanel title={t('Curve Data')}>
                <Spin spinning={curveSpin} tip="loading...">
                    { lineChartData
                        ? <LineChart
                            hAxisTitle={'x'}
                            vAxisTitle={'y'}
                            graph={lineChartData}
                            onSelectionChanged={onSelectedRangeChanged}
                            record={curveSession.selectedRecord}
                            isDark={isDark}
                        />
                        : <StyledEmpty style={{ marginTop: 160 }} />
                    }
                </Spin>
            </CollapsiblePanel>
        </div>
    );
});

export default DynamicLineChart;
