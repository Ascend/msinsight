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
import { useTranslation } from 'react-i18next';
import {
    chart,
    on,
    singleData,
    unit,
    UnitHeight,
} from '../../entity/insight';
import type {
    ChartDesc, InsightUnit, LinkLine, LinkLines,
} from '../../entity/insight';
import type { MapValueOfLinkLines, SelectedDataType, Session } from '../../entity/session';
import { hashToNumber } from '../../utils/colorUtils';
import type {
    AscendSliceDetail,
    CardMetaData,
    CounterMetaData,
    ProcessData,
    ProcessMetaData,
    ThreadMetaData,
    ThreadTrace, HostMetaData, SliceMeta, SliceData, LabelMetaData,
} from '../../entity/data';
import { createCounterParam, createStatusParam } from './unitFunc';
import { SelectedDataBottomPanel } from '../../components/SelectedDataBottomPanel';
import { SelectSimpleTabularDetail } from '../../components/details/SelectSimpleDetail';
import { renderRadiusBorder } from '../../components/details/utils';
import { generateFlowParam, slicesListDetail } from './details';
import { colorPalette, getTimeOffset } from './utils';
import React, { useEffect, useState, useMemo } from 'react';
import { observer } from 'mobx-react-lite';
import _ from 'lodash';
import { runInAction } from 'mobx';
import { offsetConfig } from './config/offsetConfig';
import { isPinned, isSonPinned } from '../../components/ChartContainer/unitPin';
import type { Theme } from '@emotion/react';
import type { ChartHandle, ChartType, Scale, StackStatusConfig, StackStatusData, StatusData } from '../../entity/chart';
import { ResizeTable } from '@insight/lib/resize';
import { getDefaultColumData, getPageData, PageType } from '../../components/detailViews/Common';
import { safeJSONParse } from '@insight/lib/utils';
import { SorterResult } from 'antd/lib/table/interface';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
import { getUnitFlows, queryAllSameOperatorsDuration } from '../../api/request';
import { GetUnitFlowsParams, OpData } from '../../api/interface';
import connector from '../../connection';

const MAX_UNIT_CANVAS_HEIGHT = 50_000; // 画布高度上限
const MAX_UNIT_DEPTH = Math.floor(MAX_UNIT_CANVAS_HEIGHT / UnitHeight.STANDARD); // 泳道深度上限
const FALLBACK_DEPTH = 2; // 当深度超过上限，使用此深度值

const isHiddenTitle = (data: AscendSliceDetail): boolean => {
    return data.title === undefined;
};

const isHiddenStartTime = (data: AscendSliceDetail): boolean => {
    return data.startTime === undefined;
};

const isHiddenRawStartTime = (data: AscendSliceDetail): boolean => {
    return data.rawStartTime === undefined;
};

const isHiddenRawEndTime = (data: AscendSliceDetail): boolean => {
    return data.rawEndTime === undefined;
};

const isHiddenDuration = (data: AscendSliceDetail): boolean => {
    return data.duration === undefined;
};

const isHiddenSelfTime = (data: AscendSliceDetail, session?: Session): boolean => {
    if (session?.isSimulation) {
        return true;
    }
    return data.selfTime === undefined || data.selfTime === 0;
};

const nsToMs = (ns: number): number => {
    return ns / 1000000;
};

const nsToNs = (ns: string | bigint): string => {
    if (typeof BigInt === 'undefined') {
        const nsNumber = Number(ns);
        if (Number.isNaN(nsNumber)) {
            return '-';
        }
        const ms = Math.floor(nsNumber / 1000000);
        const us = Math.floor((nsNumber - (ms * 1000000)) / 1000);
        const nsRemainder = nsNumber - (ms * 1000000) - (us * 1000);
        if (ms === 0 && us === 0) {
            return `${nsRemainder}ns`;
        }
        if (ms === 0) {
            return `${us}us${nsRemainder}ns`;
        }
        return `${ms}ms${us}us${nsRemainder}ns`;
    }
    let nsBig: bigint;
    if (typeof ns === 'bigint') {
        // 已经是纳秒
        nsBig = ns;
    } else {
        const s = ns.trim();

        if (s.includes('.')) {
            let [intPart] = s.split('.');
            // 仅取数字，防杂字符
            intPart = intPart.replace(/[^-\d]/g, '');
            // 舍弃小数位
            nsBig = BigInt(intPart);
        } else {
            // 纯整数：直接按纳秒
            const intNs = s.replace(/[^-\d]/g, '');
            nsBig = BigInt(intNs);
        }
    }

    const MS = BigInt(1_000_000); // 1 ms = 1,000,000 ns
    const US = BigInt(1_000); // 1 us = 1,000 ns

    const ms = nsBig / MS;
    const nsAfterMs = nsBig % MS;

    const us = nsAfterMs / US;
    const nsRemainder = nsAfterMs % US;

    if (ms === BigInt(0) && us === BigInt(0)) {
        return `${nsRemainder}ns`;
    }
    if (ms === BigInt(0)) {
        return `${us}us${nsRemainder}ns`;
    }
    return `${ms}ms${us}us${nsRemainder}ns`;
};

export const getSliceTimeDisplay = (startTime: number | undefined): string => {
    if (startTime === undefined) {
        return '';
    }
    return `${nsToMs(startTime).toFixed(6).toString()}`;
};

export const getDetailTimeDisplay = (startTime: number | undefined): string => {
    if (startTime === undefined) {
        return '';
    }
    return nsToNs(startTime.toString());
};

export const getDisplay = (val: string | undefined): string => {
    return val === undefined ? '' : val;
};

const isHidden = (val: string | undefined): boolean => {
    return val === undefined || val === '';
};

const singleSliceDetail = singleData({
    name: 'SingleSlice',
    renderFields: [
        ['Title', (data): string => data.title === undefined ? '' : `${data.title}`, isHiddenTitle],
        ['Start', (data: AscendSliceDetail): string => getDetailTimeDisplay(data.startTime ?? 0), isHiddenStartTime],
        ['Raw Start', (data: AscendSliceDetail): string => `${data.rawStartTime ?? ''}ns`, isHiddenRawStartTime],
        ['Raw End', (data: AscendSliceDetail): string => `${data.rawEndTime ?? '0'}ns`, isHiddenRawEndTime],
        ['Wall Duration', (data): string => getDetailTimeDisplay(data.duration as number), isHiddenDuration],
        ['Self Time', (data): string => getDetailTimeDisplay(data.selfTime as number), isHiddenSelfTime],
        ['Input Shapes', (data: AscendSliceDetail): string => getDisplay(data.inputShapes), (data: AscendSliceDetail): boolean => isHidden(data.inputShapes)],
        ['Input Data Types', (data: AscendSliceDetail): string => getDisplay(data.inputDataTypes), (data: AscendSliceDetail): boolean => isHidden(data.inputDataTypes)],
        ['Input Formats', (data: AscendSliceDetail): string => getDisplay(data.inputFormats), (data: AscendSliceDetail): boolean => isHidden(data.inputDataTypes)],
        ['Output Shapes', (data: AscendSliceDetail): string => getDisplay(data.outputShapes), (data: AscendSliceDetail): boolean => isHidden(data.inputDataTypes)],
        ['Output Data Types', (data: AscendSliceDetail): string => getDisplay(data.outputDataTypes), (data: AscendSliceDetail): boolean => isHidden(data.inputDataTypes)],
        ['Output Formats', (data: AscendSliceDetail): string => getDisplay(data.outputFormats), (data: AscendSliceDetail): boolean => isHidden(data.inputDataTypes)],
        ['Attr Info', (data: AscendSliceDetail): string => getDisplay(data.attrInfo), (data: AscendSliceDetail): boolean => isHidden(data.attrInfo)],
    ],
    fetchData: async (session: Session, metadata: ThreadMetaData) => {
        const selectedSliceData = session.selectedData as unknown as ThreadTrace;
        const timestampOffset = getTimeOffset(session, metadata);
        // 因为泳道chart数据减去了偏移，所有点选的时候得把偏移加回来
        const params = {
            rankId: metadata.cardId,
            dbPath: metadata.dbPath,
            metaType: metadata.metaType,
            pid: metadata.processId,
            tid: metadata.threadId,
            id: selectedSliceData.id,
            startTime: Math.floor(selectedSliceData.startTime + timestampOffset),
            depth: selectedSliceData.depth,
            timePerPx: session.domain.timePerPx,
        };
        const result = await window.request(metadata.dataSource, { command: 'unit/threadDetail', params });
        const res = result?.data ?? {};
        const data: AscendSliceDetail = {
            pid: metadata?.processId,
            tid: metadata?.threadId,
            startTime: selectedSliceData?.startTime,
            depth: selectedSliceData?.depth,
            ...res,
        };
        if (data.rawStartTime !== undefined && session.selectedData?.id === selectedSliceData.id) {
            runInAction(() => {
                (session.selectedData as SelectedDataType).rawStartTime = data.rawStartTime;
            });
        }
        return data;
    },
});

const EmptyJSXElement = (): JSX.Element | null => {
    return <></>;
};

export interface FlowPoint {
    depth: number;
    duration: number;
    id: string;
    name: string;
    pid: string;
    tid: string;
    timestamp: number;
    rankId: string;
    metaType: string;
}

interface FlowEvent {
    cat: string;
    from: FlowPoint;
    to: FlowPoint;
    id: string;
    title: string;
}

interface CategoryFlows {
    cat: string;
    flows: FlowEvent[];
}

const drawRectBorder = (selectedData: SliceData,
    session: Session, xScale: (num: number) => number, yScale: (num: number) => number, ctx: CanvasRenderingContext2D): void => {
    const duration = selectedData.duration < 0 ? session.endTimeAll as number : selectedData.startTime + selectedData.duration;
    const bottomRight = xScale(duration) - xScale(selectedData.startTime);
    renderRadiusBorder({
        topLeft: xScale(selectedData.startTime),
        topRight: yScale(0),
        bottomRight,
        bottomLeft: yScale(1),
        depth: selectedData.depth,
        ctx,
    });
};

const PYTHON_STACK_THREAD_ID_PREFIX = 'python_stack:';

function isSameThreadId(selectedThreadId: string, currentMeta: ThreadMetaData): boolean {
    if (currentMeta.threadIdList) {
        return currentMeta.threadIdList.includes(selectedThreadId);
    }
    if (selectedThreadId === currentMeta.threadId) {
        return true;
    }
    return currentMeta.threadId.startsWith(PYTHON_STACK_THREAD_ID_PREFIX) &&
        currentMeta.threadId.slice(PYTHON_STACK_THREAD_ID_PREFIX.length) === selectedThreadId;
}

interface DrawBorderArgs {
    item?: SliceData;
    threadMetaData: ThreadMetaData;
    session: Session;
    xScale: (num: number) => number;
    yScale: (num: number) => number;
    ctx: CanvasRenderingContext2D;
};

const drawSingleAlignSlice = ({ item, threadMetaData, session, xScale, yScale, ctx }: DrawBorderArgs): void => {
    const singleSliceData = item;
    if (singleSliceData === undefined) {
        return;
    }
    const singleMeta = item as unknown as SliceMeta;
    const alignCheck = singleMeta.cardId === threadMetaData.cardId &&
        singleMeta.processId === threadMetaData.processId &&
        singleMeta.metaType === threadMetaData.metaType &&
        isSameThreadId(singleMeta.threadId, threadMetaData);
    if (alignCheck) {
        drawRectBorder(singleSliceData, session, xScale, yScale, ctx);
    }
};

const getThreadTracesRequestParams = (session: Session, threadMetaData: ThreadMetaData, timestampOffset: number): Record<string, unknown> => {
    const key = threadMetaData.cardId !== undefined ? `${threadMetaData.cardId}_${threadMetaData.threadName}` : null;
    const isFilterPythonFunction = key !== null ? (session?.unitsConfig.filterConfig.pythonFunction as Record<string, boolean>)?.[key] ?? false : false;
    return {
        cardId: threadMetaData.cardId,
        dbPath: threadMetaData.dbPath,
        processId: threadMetaData.processId,
        threadId: threadMetaData.threadId,
        threadIdList: threadMetaData.threadIdList,
        metaType: threadMetaData.metaType,
        startTime: Math.floor(session.domainRange.domainStart + timestampOffset),
        endTime: Math.ceil(session.domainRange.domainEnd + timestampOffset),
        dataSource: threadMetaData.dataSource,
        timePerPx: session.domain.timePerPx,
        isFilterPythonFunction,
        isHideFlagEvents: session.areFlagEventsHidden,
    };
};

function isSameUnit(selectedMeta?: SelectedDataType, currentMeta?: ThreadMetaData): boolean {
    if (!selectedMeta || !currentMeta) {
        return false;
    }

    return isSameThreadId(selectedMeta.threadId, currentMeta) &&
        selectedMeta.processId === currentMeta.processId &&
        selectedMeta.cardId === currentMeta.cardId &&
        selectedMeta.metaType === currentMeta.metaType;
}

/**
 * 获取连线算子的关联算子
 * @param session
 * @param flow
 * @param referFlow
 */
export function handleLinkLinesMap(session: Session, flow: FlowEvent, referFlow: { rankId: string; dbPath: string }): void {
    const getKey = (point: FlowPoint): string => {
        const { pid, tid, depth, timestamp } = point;
        return `${pid}_${tid}_${depth}_${timestamp}`;
    };
    const setLinkLinesMap = (lineType: 'from' | 'to'): void => {
        const mKey = getKey(flow[lineType]);
        const mVal = (session.mapOfLinkLines.get(mKey) ?? { cat: flow.cat, from: [], to: [], current: flow[lineType] }) as MapValueOfLinkLines;
        const attr = lineType === 'from' ? 'to' : 'from';
        if (!mVal[attr].find(item => getKey(item) === getKey(flow[attr]))) {
            flow[attr] = { ...flow[attr], ...referFlow };
            mVal[attr].push(flow[attr]);
            session.mapOfLinkLines.set(mKey, mVal);
        }
    };
    setLinkLinesMap('from');
    setLinkLinesMap('to');
}

export const ThreadUnit = unit<ThreadMetaData>({
    name: 'Thread',
    pinType: 'copied',
    renderInfo: (session: Session, thread: ThreadMetaData, thisUnit: InsightUnit) => {
        return isPinned(thisUnit) && !isSonPinned(thisUnit) ? `${thread.threadName}_${thread.processName} (${thread.processId})_${thread.cardId}` : `${thread.threadName}`;
    },
    chart: chart({
        type: 'stackStatus',
        height: UnitHeight.COLL,
        mapFunc: async (session: Session, metaData: unknown, thisUnit?: InsightUnit) => {
            if (thisUnit === undefined) { return []; }
            const threadMetaData = metaData as ThreadMetaData;
            // 查询泳道chart参数加上时间偏移
            const timestampOffset = getTimeOffset(session, threadMetaData);
            const requestParams = getThreadTracesRequestParams(session, threadMetaData, timestampOffset);
            try {
                thisUnit.isTraceLoading = true;
                const request = await window.request(requestParams.dataSource as DataSource, { command: 'unit/threadTraces', params: requestParams }, { silent: true }).finally(() => {
                    thisUnit.isTraceLoading = false;
                });

                if (request === undefined) {
                    return [];
                }

                const { data: threadTraceList, maxDepth, currentMaxDepth, havePythonFunction } = request;

                if (thisUnit) {
                    let activeMaxDepth = session.autoAdjustUnitHeight ? currentMaxDepth : maxDepth;
                    activeMaxDepth = activeMaxDepth > MAX_UNIT_DEPTH ? FALLBACK_DEPTH : activeMaxDepth;
                    updateUnitData(thisUnit, activeMaxDepth, havePythonFunction);
                }

                // 画布高度渲染上限 50_000 像素左右，所以这里限制最大深度为 MAX_UNIT_DEPTH，超过该深度一般是因为数据异常，不作渲染
                if (maxDepth > MAX_UNIT_DEPTH) {
                    if (thisUnit) {
                        (thisUnit.chart as ChartDesc<'stackStatus'>).error = true;
                    }
                    return [];
                }
                // 泳道chart返回数据减去时间偏移
                return _.map(threadTraceList, (it) => _.map(it, (data) => {
                    let uintColor;
                    if (session.isSimulation) {
                        uintColor = colorPalette[hashToNumber(data.cname, colorPalette.length)];
                    } else {
                        uintColor = colorPalette[hashToNumber(data.name, colorPalette.length)];
                    }
                    return {
                        startTime: data.startTime - timestampOffset,
                        originalStartTime: data.startTime,
                        duration: data.duration,
                        name: data.name,
                        type: data.name,
                        color: uintColor,
                        depth: data.depth,
                        threadId: data.threadId,
                        cardId: threadMetaData.cardId,
                        dbPath: threadMetaData.dbPath,
                        cname: data.cname,
                        id: data.id,
                    } as StackStatusData;
                }));
            } catch (e) {
                return [];
            }
        },
        decorator: (session: Session, metaData: unknown) => {
            return {
                action: async (handle, xScale, yScale, theme): Promise<void> => {
                    maskedNotSelectData(session, handle, xScale, yScale);
                    // click
                    const ctx = handle.context;
                    const selectedData = session.selectedData as unknown as SliceData;
                    const selectedUnitMetaData = session.selectedData;
                    const threadMetaData = metaData as ThreadMetaData;
                    if (ctx === null) {
                        return;
                    }

                    const check = selectedData !== undefined && isSameUnit(selectedUnitMetaData, threadMetaData);
                    // 来自本泳道点击的数据，给数据描边+画线
                    ctx.strokeStyle = theme.textColorPrimary;
                    if (check) {
                        drawRectBorder(selectedData, session, xScale, yScale, ctx);
                    }
                    const benchMarkData = session.benchMarkData as SliceData | undefined;
                    if (benchMarkData === undefined) {
                        return;
                    }
                    const benchMarkMeta = session.benchMarkData as SliceMeta;
                    const benchCheck = benchMarkMeta.cardId === threadMetaData.cardId &&
                        benchMarkMeta.processId === threadMetaData.processId &&
                        benchMarkMeta.metaType === threadMetaData.metaType &&
                        isSameThreadId(benchMarkMeta.threadId, threadMetaData);
                    if (benchCheck) {
                        drawRectBorder(benchMarkData, session, xScale, yScale, ctx);
                    }
                    if (session.alignSliceData === undefined) {
                        return;
                    }
                    session.alignSliceData.forEach((item: SliceData) => {
                        drawSingleAlignSlice({ item, threadMetaData, session, xScale, yScale, ctx });
                    });
                },
                triggers: [
                    session.selectedData,
                    session.selectedData?.duration,
                    session?.searchData,
                    session.alignRender,
                ],
            };
        },
        onClick: async (data, session, metadata) => {
            if (data === undefined) { return; }
            const linkFlow = generateFlowParam(metadata as ThreadMetaData, data);
            linkFlow.isSimulation = session.isSimulation;
            const timestampOffset = getTimeOffset(session, metadata as ThreadMetaData);
            linkFlow.startTime = timestampOffset + (linkFlow.startTime as number);
            linkFlow.endTime = timestampOffset + (linkFlow.endTime as number);
            const raw = await getUnitFlows(linkFlow as GetUnitFlowsParams);
            const categoryFlowEvents = raw.unitAllFlows as CategoryFlows[] ?? [];
            const newLines: LinkLines = {};
            session.mapOfLinkLines.clear();
            for (const categoryFlowEvent of categoryFlowEvents) {
                const cat = categoryFlowEvent.cat;
                const singleCatLinkLine: LinkLine = [];
                for (const flow of categoryFlowEvent.flows) {
                    handleLinkLinesMap(session, flow, { rankId: linkFlow.rankId as string, dbPath: linkFlow.dbPath as string });
                    const singleLine: Record<string, unknown> = {
                        category: flow.cat,
                        cardId: linkFlow.rankId,
                        from: flow.from,
                        to: flow.to,
                    };
                    singleCatLinkLine.push(singleLine);
                }
                newLines[cat] = singleCatLinkLine;
            }
            runInAction(() => {
                session.drawLineMode = 'single';
                session.linkLines = {};
                session.singleLinkLine = newLines;
                session.renderTrigger = !session.renderTrigger;
            });
        },
        onHover: (data, session: Session): void => {
            runInAction(() => {
                session.sharedState.threadTrace = data;
            });
        },
        renderTooltip: (data) => new Map([
            ['Name', data.name],
            ['Duration', getDetailTimeDisplay(data.duration as number)],
        ]),
        config: {
            rowHeight: UnitHeight.STANDARD,
            isCollapse: true,
        },
    }),
    bottomPanelRender: (newSession: Session, metadata) => {
        return [
            {
                Detail: ({ session, height }): JSX.Element => <SelectedDataBottomPanel
                    session={session} height={height} detail={singleSliceDetail}>{EmptyJSXElement}</SelectedDataBottomPanel>,
            },
            {
                Detail: ({ session, height }): JSX.Element => <SelectSimpleTabularDetail
                    session={session} height={height} detail={slicesListDetail}></SelectSimpleTabularDetail>,
                More: (): JSX.Element => <SameOperatorsList session={newSession} metadata={metadata} updater={useSliceListMoreUpdater} />,
                moreWh: 320,
            },
        ];
    },
    collapseAction: (insightUnit) => {
        const chartDesc = (insightUnit.chart as ChartDesc<ChartType>);
        const config = (insightUnit.chart as ChartDesc<ChartType>).config;
        runInAction(() => {
            (config as any).isCollapse = !((config as any).isCollapse as boolean);
            const collapseHeight = UnitHeight.COLL;
            const expandedHeight = (config as any).maxDepth * (config as any).rowHeight;
            chartDesc.height = ((config as any).isCollapse as boolean) ? collapseHeight : expandedHeight;
        });
    },
});

const recoverHistory = (currentUnit: InsightUnit, threadTraceMaxDepth: number): void => {
    const currentChart = currentUnit.chart as ChartDesc<'stackStatus'>;
    const config = currentChart.config as StackStatusConfig;
    if (currentUnit.onceExpand !== undefined) {
        currentUnit.isExpanded = currentUnit.onceExpand;
        if (currentUnit.collapsible) {
            config.isCollapse = !currentUnit.onceExpand;
        }
        delete currentUnit.onceExpand;
        currentChart.height = config.isCollapse ? UnitHeight.COLL : threadTraceMaxDepth * config.rowHeight;
        config.maxDepth = threadTraceMaxDepth;
    }
};

const updateUnitData = (currentUnit: InsightUnit, threadTraceMaxDepth: number, havePythonFunction: boolean): void => {
    const currentChart = currentUnit.chart as ChartDesc<'stackStatus'>;
    const config = currentChart.config as StackStatusConfig;
    runInAction(() => {
        if (threadTraceMaxDepth) {
            // 根据该接口返回的最大深度重新渲染泳道高度
            if (threadTraceMaxDepth > 1 && !currentUnit.collapsible) {
                currentUnit.collapsible = true;
                currentUnit.isExpanded = true;
            }
            // 恢复历史数据
            recoverHistory(currentUnit, threadTraceMaxDepth);
            if (threadTraceMaxDepth !== config.maxDepth) {
                currentChart.height = config.isCollapse ? UnitHeight.COLL : threadTraceMaxDepth * config.rowHeight;
                config.maxDepth = threadTraceMaxDepth;
            }
            currentUnit.havePythonFunction = havePythonFunction;
        }
    });
};

function maskedNotSelectData<T extends ChartType>(session: Session, handle: ChartHandle<T>, xScale: Scale, yScale: Scale): void {
    if (session.searchData) {
        const name = session.searchData.content;
        const isAble = (item: any): boolean => {
            if (session.searchData?.isMatchCase === undefined || name === '') {
                return false;
            }
            const it = item as {name: string};
            if (session.searchData.isMatchExact && session.searchData.isMatchCase) {
                return it.name !== name;
            }
            if (session.searchData.isMatchExact) {
                return it.name?.toLocaleLowerCase() !== session.searchData.content.toLocaleLowerCase();
            } else if (session.searchData.isMatchCase) {
                return !it.name?.includes(name);
            } else {
                return !it.name?.toLocaleLowerCase().includes(session.searchData.content.toLocaleLowerCase());
            }
        };
        const data = handle.findAll(isAble).map((it: any) => it.map((notSelectedData: any) => ({
            ...notSelectedData,
            color: 'transparentMask' as const,
        })));
        handle.draw(data, xScale, yScale);
    }
}

async function createSummaryChart<T extends ProcessMetaData | LabelMetaData>(
    metaData: T,
    session: Session,
    unitType: string,
    unit?: InsightUnit,
): Promise<StatusData[]> {
    const timestampOffset = getTimeOffset(session, metaData);

    const requestParam = {
        cardId: metaData.cardId,
        dbPath: metaData.dbPath,
        processId: metaData.processId,
        metaType: metaData.metaType,
        unitType,
        startTime: Math.floor(Math.max(0, timestampOffset)),
        endTime: Math.ceil(Math.max(0, (session.endTimeAll ?? 0) + timestampOffset)),
        dataSource: metaData.dataSource,
        timePerPx: session.domain.timePerPx,
    };

    const requestKey = createStatusParam('unit/threadTracesSummary', requestParam);
    try {
        // Counter类型泳道去除无效缩略图请求
        if (unit !== undefined && unit?.children?.[0].name === 'Counter') {
            return [];
        }
        if (unit !== undefined) {
            unit.isSummaryLoading = true;
        }
        const request: any = await session.simpleCache.tryFetchFromCache('unit/threadTracesSummary', requestKey, requestParam);
        if (unit !== undefined) {
            unit.isSummaryLoading = false;
        }
        const resProcess = (result: any): StatusData[] => {
            if (result === undefined) {
                return [];
            }
            const threadTraceList = result.data as ProcessData[];
            const res: StatusData[] = [];
            // 泳道chart返回数据减去时间偏移
            threadTraceList.forEach((data) => {
                res.push({
                    startTime: data.startTime - timestampOffset,
                    duration: data.duration,
                    name: '',
                    type: '',
                });
            });
            return res;
        };

        if (requestParam.processId === 'OVERLAP_ANALYSIS' && request === undefined) {
            return new Promise((resolve) => {
                connector.addListener('updateAnalysisData', async (e) => {
                    if (e?.data?.body?.data?.dbId !== requestParam.dbPath) {
                        return;
                    }
                    const result = await session.simpleCache.tryFetchFromCache('unit/threadTracesSummary', requestKey, { ...requestParam });
                    return resolve(resProcess(result));
                });
            });
        }

        return resProcess(request);
    } catch (e) {
        return [];
    }
}

const ProcessSummaryChart = chart({
    type: 'status',
    mapFunc: async (session: Session, metaData: unknown, unit: InsightUnit | undefined) => {
        return await createSummaryChart(metaData as ProcessMetaData, session, 'process', unit);
    },
    config: {
        rowHeight: UnitHeight.STANDARD,
    },
    height: UnitHeight.UPPER,
});

const LabelSummaryChart = chart({
    type: 'status',
    mapFunc: async (session: Session, metaData: unknown, unit: InsightUnit | undefined) => {
        return await createSummaryChart(metaData as LabelMetaData, session, 'label', unit);
    },
    config: {
        rowHeight: UnitHeight.STANDARD,
    },
    height: UnitHeight.UPPER,
});

export const ProcessUnit = unit<ProcessMetaData>({
    name: 'Process',
    configBar: (session: Session, metadata: ProcessMetaData, onClick?: () => void, isHovered?: boolean, isSelected?: boolean) => {
        // Host侧第三级泳道不显示offset
        if ((metadata as ThreadMetaData).threadId !== '') {
            return null;
        }
        return offsetConfig(session, metadata, onClick, isHovered, isSelected);
    },
    tag: (session: Session, metadata: { label?: string }) => metadata.label === undefined ? '' : `${metadata.label}`,
    pinType: 'copied',
    chart: ProcessSummaryChart,
    renderInfo: (session: Session, metadata: ProcessMetaData, thisUnit) => {
        return isPinned(thisUnit) && !isSonPinned(thisUnit) ? `${metadata.cardId}_${metadata.processName}` : `${metadata.processName}`;
    },
});

export const LabelUnit = unit<LabelMetaData>({
    name: 'Label',
    tag: (session: Session, metadata: { label?: string }) => metadata.label === undefined ? '' : `${metadata.label}`,
    pinType: 'copied',
    chart: LabelSummaryChart,
    renderInfo: (session: Session, metadata: LabelMetaData, thisUnit) => {
        return isPinned(thisUnit) && !isSonPinned(thisUnit) ? `${metadata.cardId}_${metadata.processName} (${metadata.processId})` : `${metadata.processName}`;
    },
});

export const CardUnit = unit<CardMetaData>({
    name: 'Card',
    configBar: offsetConfig,
    pinType: 'copied',
    renderInfo: (session: Session, metadata: { cardName: string; cluster?: string; cardPath: string }) =>
        <span style={{ marginLeft: 6 }}>
            {(session.isMultiCluster && metadata.cluster !== undefined ? `${metadata.cluster} ` : '') + metadata.cardName}
        </span>,
    spreadUnits: on(
        'create',
        async (self): Promise<void> => {
        }),
});

export const ROOT_UNIT = unit<HostMetaData>({
    name: 'Root',
    pinType: 'copied',
    renderInfo: (session: Session, metadata: { host: string }) => metadata.host,
});

export const CounterUnit = unit<CounterMetaData>({
    name: 'Counter',
    pinType: 'move',
    collapsible: false,
    renderInfo: (session: Session, metadata) => `${metadata.threadName}`,
    chart: chart({
        type: 'filledLine',
        height: UnitHeight.SUPER_UPPER,
        mapFunc: async (session: Session, metadata) => {
            const countMetaData = metadata as CounterMetaData;
            const timestampOffset = getTimeOffset(session, countMetaData);
            // 查询泳道chart参数加上时间偏移
            const requestParam = {
                rankId: countMetaData.cardId,
                dbPath: countMetaData.dbPath,
                pid: countMetaData.processId,
                threadName: countMetaData.threadName,
                threadId: countMetaData.threadId,
                metaType: countMetaData.metaType,
                startTime: Math.floor(Math.max(0, timestampOffset)),
                endTime: Math.ceil(Math.max(0, (session.endTimeAll ?? 0) + timestampOffset)),
                dataSource: countMetaData.dataSource,
                timePerPx: session.domain.timePerPx,
            };
            const requestKey = createCounterParam('unit/counter', requestParam);
            const request = await session.simpleCache.tryFetchFromCache('unit/counter', requestKey, requestParam, metadata);
            const res = request?.data as number[][];
            return res.map(([timestamp, ...rest]) => [timestamp - timestampOffset, ...rest]);
        },
        config: (session: Session, metadata) => {
            const palette: Array<keyof Theme['colorPalette']> = [];
            const countMetaData = metadata as CounterMetaData;
            countMetaData.dataType.forEach((item, index): void => {
                const colorIndex = hashToNumber(`${item}${countMetaData.threadName}`, colorPalette.length);
                const color = colorPalette[colorIndex];
                if (color === palette[index - 1]) {
                    // 相邻色值不能相同，否则堆叠图无法区分数据
                    palette.push(colorPalette[(colorIndex + 1) % colorPalette.length]);
                } else {
                    palette.push(color);
                }
            });
            return {
                palette,
                valueRange: countMetaData.maxValue ? [0, countMetaData.maxValue] : undefined,
            };
        },
        renderTooltip: (data, metadata) => {
            const tooltipMap = new Map();
            (metadata as CounterMetaData).dataType.forEach((item, index) => {
                tooltipMap.set(item, `${data[index + 1]}`);
            });
            return tooltipMap;
        },
    }),
});

const useColumns = (): any => {
    const { t } = useTranslation('timeline', { keyPrefix: 'sliceList' });
    return [
        { title: t('Index'), dataIndex: 'index', ellipsis: true, width: 60 },
        { title: t('Start Time'), dataIndex: 'startTime', ...getDefaultColumData('time') },
        {
            title: t('Duration(ms)'),
            dataIndex: 'duration',
            ...getDefaultColumData('duration'),
            render: (text: number): string => {
                return (text / 1e6).toFixed(6);
            },
        },
    ];
};

export type SameOperatorsUpdaterType = (session: Session, metadata: unknown) => ({
    page: PageType;
    setPage: (args: PageType) => void;
    sorter: SorterResult<OpData>;
    setSorter: React.Dispatch<React.SetStateAction<SorterResult<OpData>>>;
    slice: any;
    defaultPage: PageType;
    defaultSorter: SorterResult<OpData>;
});

const useSliceListMoreUpdater: SameOperatorsUpdaterType = (session) => {
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const defaultSorter: SorterResult<OpData> = { field: 'duration', order: 'descend' };
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const slice = useMemo(() => session.selectedMultiSlice === '' ? undefined : safeJSONParse(session.selectedMultiSlice), [session.selectedMultiSlice]);
    return { page, setPage, sorter, setSorter, slice, defaultPage, defaultSorter };
};

export const SameOperatorsList = observer(({ session, metadata, updater }: { session: Session; metadata: unknown; updater: SameOperatorsUpdaterType }) => {
    const { page, setPage, sorter, setSorter, slice, defaultPage, defaultSorter } = updater(session, metadata);
    const [selectedRowKey, setSelectedRowKey] = useState('');
    const [dataSource, setDataSource] = useState<OpData[]>([]);
    const [loading, setLoading] = useState(false);

    const loadData = React.useCallback(_.debounce(async (slice: any, page: PageType, sorter: SorterResult<OpData>): Promise<void> => {
        setLoading(true);
        if (slice === undefined || slice.name === 'Totals') {
            setDataSource([]);
            setPage(defaultPage);
            setLoading(false);
            return;
        }
        const orderBy = sorter.field === 'startTime' ? 'timestamp' : sorter.field;
        const { searchOfSlice, rangeOfLevels } = session.sliceSelection;
        const paramsOfDepth = searchOfSlice ? { startDepth: rangeOfLevels[0].toString(), endDepth: rangeOfLevels[1].toString() } : {};
        const params = { ...slice, ...sorter, ...page, ...paramsOfDepth, orderBy };
        try {
            const res = await queryAllSameOperatorsDuration(params);
            const { currentPage, pageSize, sameOperatorsDetails } = res;
            const data = sameOperatorsDetails as OpData[];
            const timestampoffset = getTimeOffset(session, metadata as ThreadMetaData);
            data.forEach(item => {
                item.startTime = getDetailTimeDisplay(item.timestamp - timestampoffset);
            });
            setDataSource((data).map((item, index) => ({ ...item, index: ((currentPage - 1) * pageSize) + index + 1 })));
            setPage({ total: slice.count, current: currentPage, pageSize });
        } finally {
            setLoading(false);
        }
    }, 100), []);

    useEffect(() => {
        setDataSource([]);
        setPage(defaultPage);
        setSorter(defaultSorter);
    }, [slice]);

    useEffect(() => {
        loadData(slice, page, sorter);
    }, [slice, sorter.field, sorter.order, page.current, page.pageSize]);

    return <div style={{ height: '100%', overflow: 'auto', padding: '5px 5px 15px 5px' }}>
        <ResizeTable<OpData>
            onChange={(pagination, filters, newSorter, extra): void => {
                if (extra.action === 'sort') {
                    setSorter(newSorter as SorterResult<OpData>);
                }
            }}
            pagination={getPageData(page, setPage)}
            dataSource={dataSource}
            columns={useColumns()}
            size="small"
            loading = {loading}
            onRow={(record: OpData): {onClick: () => void} => {
                return {
                    onClick: (): void => {
                        jumpToUnitOperator({
                            ...record,
                            name: slice?.name,
                            cardId: (metadata as ThreadMetaData).cardId,
                            dbPath: (metadata as ThreadMetaData).dbPath,
                            metaType: (metadata as ThreadMetaData).metaType,
                        });
                        setSelectedRowKey(record.id);
                    },
                };
            }}
            rowClassName={(record: OpData): string => {
                return record.id === selectedRowKey ? 'selected-row' : 'click-able';
            }}
        />
    </div>;
});
