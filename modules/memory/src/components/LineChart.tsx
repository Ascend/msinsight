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
import * as React from 'react';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import type { Graph } from '../entity/memory';
import { binarySearch, useResizeEventDependency } from '../utils/memoryUtils';
import * as echarts from 'echarts';
import { convertTime, useChartCharacter } from './Common';
import styled from '@emotion/styled';
import { chartColors, getDefaultChartOptions, getLegendStyle, safeStr } from '@insight/lib/utils';
import { type Theme, useTheme } from '@emotion/react';
import type { RangeFlagList } from '../entity/memorySession';

// 最大不分页的折线图图例数量，超过该数量图例分页展示
const MAX_PLAIN_LEGENDS_COUNT = 9;
const SHOW_ALL_SYMBOL_THRESHOLD = 1000;
/**
 * ECharts toolbox.dataZoom 内置 back 图标的默认 SVG path（见 echarts DataZoomFeature.getDefaultOption）。
 * 自定义 toolbox 工具（my*）无法引用内置图标名，只能复用该 path。
 */
const ECHARTS_DATA_ZOOM_BACK_ICON = 'M22,1.4L9.9,13.5l12.3,12.3 M10.3,13.5H54.9v44.6 H10.3v-26';
const ChartDesc = styled.div`
    color: ${(props): string => props.theme.textColor};
    margin-bottom: 24px;
`;

interface IProps {
    graph: Graph;
    hAxisTitle: string;
    vAxisTitle: string;
    onSelectionChanged?: (start: number, end: number) => void;
    /** 服务端抽样场景下，右键/工具栏「回退上次状态」逐级回退；不传则沿用客户端 dataZoom back 语义。还原(restore)始终回到全量 */
    onZoomBack?: () => void;
    record?: any;
    isDark: boolean;
    isStatic: boolean;
    rangeFlagData?: RangeFlagList[];
}

const _getLegendData = (data: string[]): string[] => {
    const tempData = [...data];
    // 去除表示横轴的column列
    tempData.shift();
    if (tempData.length < 2) {
        return tempData;
    }
    for (let i = 1; i < tempData.length; i++) {
        // 遍历到基线卡即可根据上一个图例是否包含比对信息而决定是否需要换行
        if ((tempData[i].endsWith('Baseline') || tempData[i].startsWith('基线'))) {
            if ((tempData[i - 1].endsWith('Comparison') || tempData[i - 1].startsWith('比对'))) {
                tempData.splice(i, 0, '');
            }
            return tempData;
        }
    }
    return tempData;
};

const _getZoomBackTitle = (locale: string): string => (
    locale === 'zh' ? '回退上次状态' : 'Back'
);

const _getOriginOption = (props: IProps, theme: Theme, locale: string): echarts.EChartsOption => {
    const { isStatic, isDark, hAxisTitle, vAxisTitle, onZoomBack } = props;
    const legendDatas = _getLegendData(props.graph.columns);
    // 服务端抽样后图表会重建，ECharts 内置缩放历史为空，原生 back 不会有效显示/工作；
    // 因此隐藏原生 back，改用自定义 myZoomBack 走业务历史栈。
    const toolboxFeature: Record<string, unknown> = {
        dataZoom: {
            ...(onZoomBack ? { icon: { back: 'none' } } : {}),
            yAxisIndex: 'none',
            emphasis: { iconStyle: { textPosition: 'top' } },
        },
    };
    if (onZoomBack) {
        toolboxFeature.myZoomBack = {
            show: true,
            title: _getZoomBackTitle(locale),
            icon: ECHARTS_DATA_ZOOM_BACK_ICON,
            onclick: (): void => {
                onZoomBack();
            },
            emphasis: { iconStyle: { textPosition: 'top' } },
        };
    }
    toolboxFeature.restore = {
        emphasis: { iconStyle: { textPosition: 'top' } },
    };
    return {
        textStyle: getDefaultChartOptions().textStyle,
        title: { text: '' },
        tooltip: {
            trigger: 'axis',
            formatter: (params: any): string => {
                let res = `${isStatic ? safeStr(params?.[0]?.name) : convertTime(params?.[0]?.name)} <br/>`;
                for (const item of params) {
                    if (!isNaN(Number(item?.value?.[item?.encode?.y?.[0]]))) {
                        res += `<span style="background: ${item.color};
                        height: 10px;
                        width: 10px;
                        border-radius: 50%;
                        display: inline-block;
                        margin-right: 10px;"></span>
                        ${safeStr(item.seriesName)}: ${safeStr(item?.value?.[item?.encode?.y?.[0]])}<br/>`;
                    }
                }
                return res;
            },
            ...getDefaultChartOptions(isDark).tooltip,
        },
        legend: {
            itemGap: 20,
            data: legendDatas,
            type: (legendDatas.length > MAX_PLAIN_LEGENDS_COUNT && !legendDatas.includes('')) ? 'scroll' : 'plain',
            ...getLegendStyle(theme),
        },
        grid: { left: '100', right: '100', bottom: 40 },
        xAxis: { type: 'category', boundaryGap: false, name: hAxisTitle },
        yAxis: { type: 'value', name: vAxisTitle, scale: true },
        toolbox: {
            feature: toolboxFeature as echarts.ToolboxComponentOption['feature'],
            top: 20,
            right: 10,
        },
        backgroundColor: 'transparent',
    };
};

const findClosestIndex = (data: Graph['rows'], target: number): number => {
    if (data.length === 0) {
        return -1; // 数组为空时返回 -1
    }

    let left = 0;
    let right = data.length - 1;

    while (left < right - 1) {
        const mid = Math.floor((left + right) / 2);
        if (Number(data[mid][0]) === target) {
            return mid;
        } else if (Number(data[mid][0]) < target) {
            left = mid;
        } else {
            right = mid;
        }
    }

    // 检查 left 和 right 位置的值，哪个更接近 target
    if (Math.abs(Number(data[left][0]) - target) <= Math.abs(Number(data[right][0]) - target)) {
        return left;
    } else {
        return right;
    }
};

const _getMarkAreaOptions = (rangeFlagData: RangeFlagList[], data: Graph['rows'], chartWidth: number): echarts.EChartsOption => {
    return {
        name: 'Mark Area',
        type: 'line',
        data: [],
        markArea: {
            data: rangeFlagData.map(item => {
                const startIndex = findClosestIndex(data, item.timeStamp / 1000000);
                let endIndex = findClosestIndex(data, item.anotherTimeStamp / 1000000);
                if (startIndex === endIndex && startIndex !== 0) {
                    endIndex++;
                }
                const titleWidth = chartWidth / data.length * (endIndex - startIndex);

                return [
                    {
                        xAxis: startIndex,
                        label: {
                            show: titleWidth > 30,
                            position: 'top',
                            formatter: item.description,
                            overflow: 'truncate',
                            width: titleWidth,
                        },
                        itemStyle: {
                            color: item.color,
                            opacity: 0.3,
                        },
                    },
                    {
                        xAxis: endIndex,
                    },
                ];
            }),
        },
    };
};

const _handleOption = (option: echarts.EChartsOption, graph: Graph, chartWidth: number, rangeFlagData?: RangeFlagList[]): echarts.EChartsOption => {
    const lineSeries: echarts.SeriesOption = {
        type: 'line',
        connectNulls: true,
        showAllSymbol: graph.rows.length < SHOW_ALL_SYMBOL_THRESHOLD ? true : 'auto',
        emphasis: {
            label: {
                show: true,
            },
            itemStyle: {
                borderWidth: 5,
                shadowBlur: 5,
                shadowColor: '#ffffff',
            },
        },
        select: {
            itemStyle: {
                borderWidth: 5,
                shadowBlur: 5,
            },
        },
        animation: false,
    };
    const series = Array(graph.columns.length - 1).fill(lineSeries);
    if (rangeFlagData !== undefined && rangeFlagData.length > 0) {
        series.push(_getMarkAreaOptions(rangeFlagData, graph.rows, chartWidth));
    }
    const newOption = {
        ...option,
        color: chartColors,
        animation: false,
        dataset:
        {
            source: [graph.columns, ...graph.rows],
        },
        series,
    };
    return newOption;
};

const _showGraph = (myChart: echarts.ECharts, selectedPoints: React.MutableRefObject<number[]>,
    props: IProps, theme: Theme, chartWidth: number, locale: string): void => {
    const { graph, onSelectionChanged, onZoomBack, rangeFlagData } = props;

    let option = _getOriginOption(props, theme, locale);
    option = _handleOption(option, graph, chartWidth, rangeFlagData);

    // 自定义回退按钮点击后保持框选缩放模式（与右键回退一致）
    const zoomBackFeature = (option.toolbox as { feature?: Record<string, { onclick?: () => void }> } | undefined)
        ?.feature?.myZoomBack;
    if (zoomBackFeature && onZoomBack) {
        zoomBackFeature.onclick = (): void => {
            onZoomBack();
            myChart.dispatchAction({
                type: 'takeGlobalCursor',
                key: 'dataZoomSelect',
                dataZoomSelectActive: true,
            });
        };
    }

    // 数据量大时，切换主题时setOption会阻塞整体界面主题切换，使用 requestAnimationFrame 优化
    requestAnimationFrame(() => {
        myChart.setOption(option, { notMerge: true, lazyUpdate: true });
        myChart.dispatchAction({
            type: 'takeGlobalCursor',
            key: 'dataZoomSelect',
            dataZoomSelectActive: true,
        });
    });

    myChart.on('dataZoom', (param: any) => {
        onSelectionChanged?.(param?.batch?.[0]?.startValue, param?.batch?.[0]?.endValue);
    });

    myChart.on('restore', () => {
        // 还原：清空缩放，回到全量（与「回退上次状态」逐级回退区分）
        onSelectionChanged?.(0, -1);
    });

    myChart.on('click', (param) => {
        myChart.dispatchAction({
            type: 'unselect',
            seriesId: param.seriesId,
            dataIndex: selectedPoints.current,
        });
        myChart.dispatchAction({
            type: 'select',
            seriesId: param.seriesId,
            dataIndex: param.dataIndex,
        });

        selectedPoints.current = [param.dataIndex];
    });

    myChart.getZr().on('contextmenu', () => {
        if (onZoomBack) {
            // 服务端抽样后图表会重建，不能依赖 ECharts 缩放历史，由业务栈逐级回退
            onZoomBack();
        } else {
            myChart.dispatchAction({
                type: 'restore',
            });
        }
        myChart.dispatchAction({
            type: 'takeGlobalCursor',
            key: 'dataZoomSelect',
            dataZoomSelectActive: true,
        });
    });
};

const _handleEvents = (chartObj: echarts.ECharts | undefined, props: IProps,
    selectedPoints: React.MutableRefObject<number[]>, graph: Graph, t: TFunction): void => {
    const { record } = props;
    const compareFun = (key: number, mid: Array<number | string>): number => key - parseFloat(mid[0] as string);
    if (chartObj) {
        if (record !== undefined) {
            const startId = binarySearch(graph.rows, Number(record?.allocationTime ?? record?.nodeIndexStart), compareFun);
            const endId = binarySearch(graph.rows, Number(record?.releaseTime ?? record?.nodeIndexEnd), compareFun);
            const selection = [];
            if (startId >= 0) {
                selection.push(startId);
            }
            if (endId >= 0) {
                selection.push(endId);
            }
            chartObj.dispatchAction({
                type: 'downplay',
                seriesName: t('Operators Allocated'),
                dataIndex: selectedPoints.current,
            });
            chartObj.dispatchAction({
                type: 'highlight',
                seriesName: t('Operators Allocated'),
                dataIndex: selection,
            });
            selectedPoints.current = selection;
        } else {
            chartObj.dispatchAction({
                type: 'downplay',
                seriesName: t('Operators Allocated'),
                dataIndex: selectedPoints.current,
            });
            selectedPoints.current = [];
        }
    }
};

const useTitle = (title: string): string => {
    const { t } = useTranslation('memory', { keyPrefix: 'searchCriteria' });
    const regexItem = [
        'Peak Memory Usage',
        'Operator Activated',
        'Operator Allocated',
        'Operator Reserved',
        'PTA Allocated',
        'PTA Reserved',
        'PTA Activated',
        'GE Allocated',
        'GE Reserved',
        'GE Activated',
        'APP Reserved',
    ];
    const regex = new RegExp(regexItem.join('|'), 'g');
    const translatedMessage = title.replace(regex, match => t(match));
    return translatedMessage;
};
export const LineChart: React.FC<IProps> = (props) => {
    const { graph, record, isDark, rangeFlagData } = props;
    const graphRef = React.useRef<HTMLDivElement>(null);
    const [resizeEventDependency] = useResizeEventDependency();
    const [chartObj, setChartObj] = React.useState<echarts.ECharts | undefined>();
    const selectedPoints = React.useRef<number[]>([]);
    const chartCharacter = useChartCharacter();
    const title = useTitle(graph.title ?? '');
    const { t, i18n } = useTranslation('memory');
    const locale = i18n.language?.slice(0, 2);
    const theme = useTheme();

    React.useLayoutEffect(() => {
        const element = graphRef.current;
        if (!element) {
            return () => {};
        }
        element.oncontextmenu = (): boolean => { return false; };
        const chartWidth = graphRef.current.clientWidth - 200;
        const myChart = echarts.init(element, isDark ? 'dark' : 'customed', { locale });
        _showGraph(myChart, selectedPoints, props, theme, chartWidth, locale);

        setChartObj(myChart);
        return () => {
            myChart.dispose();
        };
    }, [graph, isDark, i18n, rangeFlagData]);

    React.useEffect(() => {
        if (!graphRef.current) {
            return;
        }
        echarts.getInstanceByDom(graphRef.current)?.resize();
    }, [resizeEventDependency]);

    React.useEffect(() => {
        _handleEvents(chartObj, props, selectedPoints, graph, t);
    }, [graph, record, chartObj]);

    return (
        <div>

            {graph.title !== undefined && graph.title?.length !== 0
                ? <ChartDesc>{title}{chartCharacter}</ChartDesc>
                : null
            }
            <div ref={graphRef} style={{ width: 'calc(100vw - 80px)', height: '400px' }}></div>
        </div>
    );
};
