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
import React, { useEffect, useState, useRef } from 'react';
import * as echarts from 'echarts';
import { safeStr } from '@insight/lib/utils';
import { MIChart } from '@insight/lib/components';
import type { ChartsHandle } from '@insight/lib';
import { type EChartsOption } from 'echarts';
import { Session } from '../entity/session';
import { observer } from 'mobx-react';
import { getFuncNewData } from './dataHandler';
import { chartResize } from '../utils/utils';
import { useTheme, type Theme } from '@emotion/react';
import { Loading, MarkLineStack } from './leaks/tools';
import { runInAction } from 'mobx';
const colorTypes: string[] = ['#8fd3e8', '#d95850', '#eb8146', '#ffb248', '#f2d643', '#ebdba4', '#fcce10', '#b5c334', '#1bca93'];
const transData = (data: any, min: number, max: number): any => {
    return data.map((item: any, index: number) => ({
        name: item.func,
        value: [item.depth, Math.max(item.startTimestamp, min), Math.min(item.endTimestamp, max), item.func],
        itemStyle: {
            color: colorTypes[index % 9],
        },
    }));
};
const getRenderItem = (session: Session, theme: Theme, api: any): any => {
    const level = api.value(0);
    const start = api.coord([api.value(1), level]);
    const end = api.coord([api.value(2), level]);
    const height = ((api.size([0, 1]) || [0, 20]) as number[])[1];
    const width = end[0] - start[0];
    const customRes = {
        type: 'rect',
        transition: ['shape'],
        shape: {
            x: start[0],
            y: start[1],
            width,
            height: height - 2,
        },
        emphasis: { style: { stroke: '#000' } },
        textContent: {
            type: 'text',
            style: {
                text: api.value(3),
                fill: '#000',
                overflow: 'truncate',
                width: width - 4,
                fontSize: 11,
            },
        },
        textConfig: {
            position: 'inside',
            inside: true,
            local: true,
        },
        style: {
            fill: api.visual('color'),
            stroke: '',
            lineWidth: 0,
        },
    };
    if (session.searchFunc.includes(api.value(3))) {
        customRes.style.stroke = theme.mode === 'dark' ? '#fff' : '#000';
        customRes.style.lineWidth = 3;
    }
    return customRes;
};
const getSeries = (session: Session, theme: Theme): any => {
    return [
        {
            type: 'custom',
            data: transData(session.funcData.traces, session.minTime, session.maxTime),
            renderItem: (params: any, api: any): any => getRenderItem(session, theme, api),
            encode: {
                x: [0, 1, 2],
                y: 0,
            },
            clip: true,
        },
    ];
};
const getTooltip = (session: Session): echarts.TooltipComponentOption => {
    return {
        trigger: 'item',
        formatter: function (params: any): string {
            const info = session.funcData.traces[params.dataIndex];
            if (!info) {
                return '';
            }
            return safeStr(info.func);
        },
        textStyle: {
            fontSize: 12,
        },
    };
};
const getOptions = (session: Session, theme: Theme): EChartsOption => {
    return {
        color: ['#5470c6', '#91cc75', '#fac858', '#ee6666', '#73c0de', '#3ba272', '#fc8452', '#9a60b4', '#ea7ccc'],
        gradientColor: ['#f6efa6', '#d88273', '#bf444c'],
        xAxis: {
            axisTick: {
                show: false,
            },
            min: session.minTime,
            max: session.maxTime,
            axisLine: {
                show: false,
            },
            axisLabel: {
                formatter: function (value: number): string {
                    return `${(value / 1000 / 1000).toFixed(3)}`;
                },
            },
            splitLine: {
                show: false,
            },
        },
        yAxis: {
            show: false,
            inverse: true,
            axisPointer: {
                show: false,
                snap: true,
            },
            max: session.maxDepth + 1,
        },
        tooltip: getTooltip(session),
        series: getSeries(session, theme),
        grid: {
            left: 80,
            right: 60,
        },
    };
};

const MemoryFunctionCall = observer(({ session }: { session: Session }): React.ReactElement => {
    const chartRef = useRef<ChartsHandle>(null);
    const markLineRef = useRef<HTMLDivElement>(null);
    const [chartOptions, setChartOptions] = useState<EChartsOption>({});
    const { funcData, deviceId, eventType, threadId, searchFunc, threadFlag } = session;
    const [markLineContainerWidth, setMarkLineContainerWidth] = useState(0);

    const theme: Theme = useTheme();

    const handleResize = (): void => {
        if (markLineRef.current === null) {
            return;
        }
        setMarkLineContainerWidth(markLineRef.current.getBoundingClientRect().width);
        const instance = chartRef.current?.getInstance();
        if (instance) {
            instance.clear();
            setChartOptions(getOptions(session, theme));
        }
    };

    const handleMouseMove = (ev: React.MouseEvent<HTMLDivElement>): void => {
        if (markLineRef.current === null) {
            return;
        }
        const rect = markLineRef.current.getBoundingClientRect();
        const x = ev.clientX - rect.left;
        const y = ev.clientY - rect.top;
        if (x < 0 || x > rect.width || y < 0 || y > rect.height) {
            runInAction(() => {
                session.markLineInfo.stack = { x: -1, y: -1 };
                session.markLineInfo.block = { x: -1, y: -1 };
            });
            return;
        }
        runInAction(() => {
            session.markLineInfo.stack = { x, y };
        });
    };

    useEffect(() => {
        if (deviceId === '' || threadFlag) return;
        getFuncNewData(session);
    }, [deviceId, eventType, threadId]);
    useEffect(() => {
        setChartOptions(getOptions(session, theme));
        chartResize(chartRef?.current?.getInstance());
    }, [deviceId, eventType, threadId, funcData, searchFunc, theme.mode, session.minTime, session.maxTime]);

    useEffect(() => {
        if (markLineRef.current === null) {
            return;
        }
        setMarkLineContainerWidth(markLineRef.current.getBoundingClientRect().width);
    }, [chartOptions, theme.mode]);

    useEffect(() => {
        window.addEventListener('resize', handleResize);

        return () => {
            window.removeEventListener('resize', handleResize);
        };
    }, []);

    return <div style={{ position: 'relative', width: 'calc(100vw - 120px)', height: 500 }}
        onMouseMove={handleMouseMove}>
        <MIChart
            ref={chartRef}
            height="100%"
            loading={false}
            options={chartOptions}
        />
        <Loading style={{ position: 'absolute', top: 0, left: 0, width: '100%', height: '100%' }} loading={session.loadingFunc} />
        <div ref={markLineRef}
            style={{ position: 'absolute', top: 60, left: 80, width: 'calc(100% - 140px)', height: 'calc(100% - 130px)', pointerEvents: 'none' }}
        >
            <MarkLineStack session={session} width={markLineContainerWidth} />
        </div>
    </div>;
});
export default MemoryFunctionCall;
