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
import React, { useEffect, useRef, useState } from 'react';
import { MIChart } from '@insight/lib/components';
import type { ChartsHandle } from '@insight/lib';
import { useTranslation } from 'react-i18next';
import { type EChartsOption } from 'echarts';
import type { MemScopeContext as Session } from '../state/MemScopeContext';
import { observer } from 'mobx-react';
import { type DetailData } from '../api/types';
import { getNewDetailData } from '../loaders/data';
import { safeStr } from '@insight/lib/utils';

interface treemap {
    name: string;
    value: number;
    children?: treemap[];
};
const transformData = (node: DetailData, t: any, depth: number): treemap => {
    const convertedNode: treemap = {
        name: `${t(node.name)}`,
        value: node.size,
    };
    if (depth > 8) { return convertedNode; }
    if (node.subNodes && node.subNodes.length > 0) {
        convertedNode.children = node.subNodes.map(subNode => transformData(subNode, t, depth + 1));
    }
    return convertedNode;
};

const getColorGradientByGroup = (depth: number): string => {
    const baseHues = [210, 0, 30, 120];
    const group = depth % 4;
    const posInGroup = Math.floor(depth / 4);
    const saturation = 80;
    const lightnessStart = 30;
    const lightnessEnd = 70;
    const lightness = lightnessStart + ((lightnessEnd - lightnessStart) * posInGroup);
    return `hsl(${baseHues[group]}, ${saturation}%, ${lightness}%)`;
};

const mapLevelOption = (): any => {
    const levels = [];
    let i = 7;
    while (--i >= 0) {
        levels.push({
            itemStyle: {
                color: getColorGradientByGroup(i),
                borderColor: getColorGradientByGroup(i),
                borderWidth: 5,
                gapWidth: 5,
            },
            emphasis: {
                itemStyle: {
                    borderColor: '#ddd',
                },
            },
        });
    }
    return levels;
};

const getLevelOption = (): any => {
    return [
        {
            itemStyle: {
                color: '#fff',
                borderColor: '#fff',
                borderWidth: 5,
                gapWidth: 5,
            },
            upperLabel: {
                show: false,
            },
        },
        ...mapLevelOption(),
    ];
};

const getSeries = (chartData: treemap[], t: any): any => {
    return [{
        name: t('MemoryDisassembly'),
        type: 'treemap',
        visibleMin: 300,
        label: {
            show: true,
            formatter: (params: any): string => {
                const { name, value } = params.data;
                const isB: boolean = (value / 1024 / 1024).toFixed(0) === '0';
                return isB ? safeStr(`${name}:${value}B`) : safeStr(`${name}: ${(value / 1024 / 1024).toFixed(0)}MB`);
            },
        },
        upperLabel: {
            show: true,
            height: 30,
        },
        levels: getLevelOption(),
        data: chartData,
    }];
};
const MemorySliceChart: any = observer(({ session }: { session: Session }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const chartRef = useRef<ChartsHandle>(null);
    const [loading, setLoading] = useState(true);
    const [chartOptions, setChartOptions] = useState<EChartsOption>({});
    const [chartData, setChartData] = useState<treemap[]>([]);
    useEffect(() => {
        setChartOptions({
            xAxis: {
                show: false,
            },
            yAxis: {
                show: false,
            },
            legend: {
                show: false,
            },
            tooltip: {
                formatter: (params: any): string => {
                    const { name, value } = params.data;
                    const isB: boolean = (value / 1024 / 1024).toFixed(0) === '0';
                    return isB ? safeStr(`${name}:${value}B`) : safeStr(`${name}: ${(value / 1024 / 1024).toFixed(0)}MB`);
                },
            },
            series: getSeries(chartData, t) as echarts.SeriesOption,
        });
        setLoading(false);
    }, [chartData]);
    useEffect(() => {
        getNewDetailData(session);
    }, [session.memoryStamp, t]);
    useEffect(() => {
        setChartData([transformData(session.memoryData, t, 0)]);
    }, [session.memoryData, t]);
    return <>
        {chartData.length === 1 && chartData[0].value === 0
            ? <div style={{ position: 'relative', top: 40, paddingBottom: 30 }}>
                {(t('noDisassemblyData', { returnObjects: true }) as string[]).map((item, index) => <div key={index}>{item}</div>)}
            </div>
            : <MIChart
                ref={chartRef}
                height="350px"
                loading={loading}
                options={chartOptions}
            />
        }
    </>;
});
export default MemorySliceChart;
