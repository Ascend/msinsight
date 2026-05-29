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
import { Spin, CollapsiblePanel, Select } from '@insight/lib/components';
import { StyledEmpty, customConsole as console } from '@insight/lib/utils';
import { useTranslation } from 'react-i18next';
import { Graph, MemoryCurve } from '../entity/memory';
import { LineChart } from './LineChart';
import { Session } from '../entity/session';
import { MemorySession, GroupBy, type RangeFlagList } from '../entity/memorySession';
import { memoryCurveGet } from '../utils/RequestUtils';
import { getTimelineOffsetByKey } from '../connection/handler';
import { Label } from './Common';
import { FlexDiv } from '../utils/styleUtils';

const DynamicLineChart = observer(({ session, memorySession, isDark }:
{ session: Session; memorySession: MemorySession; isDark: boolean }) => {
    const isCompare: boolean = session.compareRank.isCompare;
    // 内存曲线数据源
    const [memoryCurveData, setMemoryCurveData] = useState<MemoryCurve | undefined>(undefined);
    // 内存曲线绘制数据
    const [lineChartData, setLineChartData] = useState<Graph | undefined>(undefined);
    const [curveSpin, setCurveSpin] = useState<boolean>(false);
    const [rangeFlagData, setRangeFlagData] = useState<RangeFlagList[]>([]);
    const [rankOffsetNs, setRankOffsetNs] = useState<number>(0);
    const { t } = useTranslation('memory');

    const onSelectedRangeChanged = (start: number, end: number): void => {
        runInAction(() => {
            if (start > end || !memoryCurveData) {
                memorySession.selectedRange = undefined;
                return;
            }
            const allDataListSet = new Set(memoryCurveData.lines
                .map(item => {
                    return parseFloat(item[0] as string);
                }).sort((a, b) => a - b));
            if (allDataListSet.size <= 1) {
                memorySession.selectedRange = undefined;
                return;
            }
            const allDataList = Array.from(allDataListSet);

            if (end >= allDataList.length) {
                memorySession.selectedRange = { startTs: allDataList[start], endTs: allDataList[allDataList.length - 1] };
            } else {
                memorySession.selectedRange = { startTs: allDataList[start], endTs: allDataList[end] };
            }
            memorySession.current = 1;
            memorySession.pageSize = 10;
        });
    };

    const onMemoryCurveGet = (): void => {
        setCurveSpin(true);
        const rankValue = memorySession.getSelectedRankValue();
        const start = (memorySession.selectedRange?.startTs ?? '').toString();
        const end = (memorySession.selectedRange?.endTs ?? '').toString();
        memoryCurveGet({ rankId: rankValue.rankInfo.rankId, dbPath: rankValue.dbPath, type: memorySession.groupId, isCompare, start, end }).then((resp) => {
            setMemoryCurveData(resp);
            let columns: string[] = [];
            if (memorySession.groupId === GroupBy.STREAM) {
                columns = resp.legends?.map(legend => {
                    const index = legend.lastIndexOf(' ');
                    if (index > -1) {
                        return t(legend.slice(0, index), { stream: legend.slice(index + 1) });
                    } else {
                        return t(legend);
                    }
                });
            } else {
                columns = resp.legends?.map(legend => t(legend));
            }
            if (columns?.length <= 0 || resp.lines?.length <= 0) {
                return;
            }
            setLineChartData({
                title: resp.title,
                columns,
                rows: resp.lines,
            });
            setRankOffsetNs(resp.rankOffsetNs);
        }).catch(err => {
            console.error(err);
        }).finally(() => {
            setCurveSpin(false);
        });
    };

    useEffect(() => {
        const timer = setTimeout(() => {
            if (memorySession.selectedRankId === '') {
                setLineChartData(undefined);
                setMemoryCurveData(undefined);
                return;
            }
            onMemoryCurveGet();
        });

        return () => clearTimeout(timer);
    }, [memorySession.selectedRankId, memorySession.groupId, t, session.isAllMemoryCompletedSwitch, isCompare, memorySession.selectedRange]);

    useEffect(() => {
        runInAction(() => {
            memorySession.searchEventOperatorName = '';
            memorySession.current = 1;
            memorySession.pageSize = 10;
        });
    }, [isCompare]);

    return (
        <div className="mb-30">
            <CollapsiblePanel title={t('Memory Analysis')}>
                <Spin spinning={curveSpin}>
                    {lineChartData
                        ? <>
                            <div style={{ paddingBottom: 20 }}>
                                <RangeFlagSelect memorySession={memorySession} rankOffsetNs={rankOffsetNs} callback={setRangeFlagData}/>
                            </div>
                            <LineChart
                                hAxisTitle={t('Time (ms)')}
                                vAxisTitle={t('Memory Usage (MB)')}
                                graph={lineChartData}
                                onSelectionChanged={onSelectedRangeChanged}
                                record={memorySession.selectedRecord}
                                isDark={isDark}
                                isStatic={false}
                                rangeFlagData={rangeFlagData}
                            />
                        </>
                        : <StyledEmpty style={{ marginTop: 160 }} />
                    }
                </Spin>
            </CollapsiblePanel>
        </div>
    );
});

const RangeFlagSelect = observer(({ memorySession, rankOffsetNs, callback }: { memorySession: MemorySession; rankOffsetNs: number; callback: (value: RangeFlagList[]) => void }): JSX.Element => {
    const [selectValue, setSelectValue] = useState<string[]>([]);
    const [selectOptions, setSelectOptions] = useState<Array<{ label: string; value: string }>>([]);
    const { t } = useTranslation('memory');

    const handleChange = (value: string[]): void => {
        setSelectValue(value);
    };

    const updateSelectData = (): void => {
        const selectOptionsValueListOld = selectOptions.map(item => item.value);
        const selectOptionsValueListNew = memorySession.rangeFlagList.map(item => item.uid);
        const selectValueNew = [...selectValue];
        for (let i = 0; i < selectValueNew.length;) {
            if (!selectOptionsValueListNew.includes(selectValueNew[i])) {
                selectValueNew.splice(i, 1);
            } else {
                i++;
            }
        }
        memorySession.rangeFlagList.forEach(item => {
            if (!selectOptionsValueListOld.includes(item.uid)) {
                selectValueNew.push(item.uid);
            }
        });
        setSelectOptions(memorySession.rangeFlagList.map(item => ({ value: item.uid, label: item.description })));
        setSelectValue(selectValueNew);
    };

    useEffect(() => {
        updateSelectData();
    }, [memorySession.rangeFlagList]);

    useEffect(() => {
        getTimelineOffsetByKey();
    }, [memorySession.hostCondition.value, memorySession.rankCondition.value]);

    useEffect(() => {
        const selectedRangeFlag: RangeFlagList[] = [];
        memorySession.rangeFlagList.forEach(item => {
            if (selectValue.includes(item.uid)) {
                selectedRangeFlag.push({
                    ...item,
                    timeStamp: item.timeStamp + memorySession.timelineOffset - rankOffsetNs,
                    anotherTimeStamp: item.anotherTimeStamp + memorySession.timelineOffset - rankOffsetNs,
                });
            }
        });
        callback(selectedRangeFlag);
    }, [selectValue.length, memorySession.rangeFlagList, memorySession.timelineOffset, rankOffsetNs]);

    return <FlexDiv>
        <Label name={t('AreaMark')} />
        <Select
            mode="multiple"
            allowClear
            style={{ width: 600 }}
            value={selectValue}
            onChange={handleChange}
            options={selectOptions}
            maxTagTextLength={10}
            maxTagCount="responsive"
            showSearch={true}
            optionFilterProp="label"
        />
    </FlexDiv>;
});

export default DynamicLineChart;
