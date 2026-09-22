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
import { observer } from 'mobx-react';
import React, { useMemo } from 'react';
import type { ChartProps, ChartType } from '../../entity/chart';
import { type ChartDesc, type InsightUnit, isGetChartConfig } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { EventChart } from './EventChart';
import { FilledLineChart } from './FilledLineChart';
import { StackedBarChart } from './StackedBarChart';
import { StackStatusChart } from './StackStatusChart';
import { StatusChart } from './StatusChart';
import { ChartErrorBoundary } from '../error/ChartErrorBoundary';
import { Mask } from './Mask';
import { customDebounce } from '../../utils/customDebounce';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';

const chartMap: { [K in ChartType]: React.FC<ChartProps<K>> } = {
    filledLine: FilledLineChart,
    stackedBar: StackedBarChart,
    keyEvent: EventChart,
    status: StatusChart,
    stackStatus: StackStatusChart,
    // to be extended...
};

export function isShowMask(session: Session, phase: string): boolean {
    return session.id !== 'HomePage' && session.phase !== 'error' && phase !== 'configuring' && phase !== 'download' && phase !== 'error';
}

const ChartErrorDiv = styled.div<{height: number}>`
    display: flex;
    height: ${(props): string => `${props.height}px`};

    .text {
        color: ${(props): string => props.theme.dangerColor};
        margin: auto;
    }
`;

export const Chart = observer(<T extends ChartType>(props: {
    desc: ChartDesc<T>;
    serial: string;
    title: string;
    session: Session;
    metadata: unknown;
    unit: InsightUnit;
    width: number;
    phase: string;
}) => {
    const { t } = useTranslation('timeline');
    const { desc, serial, title, session, metadata, width, phase, unit } = props;
    // Keep in-flight requests shared across resize renders so only the latest queued view is fetched.
    const mapFunc = useMemo(() => customDebounce(desc.mapFunc), [desc.mapFunc]);
    const offlinePlaceholder = useMemo(() => {
        return <ChartErrorBoundary height={desc.height} width={width} phase={phase}>
            <div className="chart-offline" style={{ width, height: desc.height }}/>
        </ChartErrorBoundary>;
    }, [width, desc.height, phase]);
    if (session.phase !== 'download' && desc.offline === true) {
        return <Mask unitPhase={phase} isShowMask={isShowMask(session, phase)}>{offlinePlaceholder}</Mask>;
    }
    const chartConfig = isGetChartConfig(desc.config) ? desc.config(session, metadata) : desc.config;
    const chartProps: ChartProps<T> = {
        session,
        mapFunc,
        margin: 0,
        title,
        width,
        height: desc.height,
        metadata,
        renderTooltip: desc.renderTooltip,
        onHover: desc.onHover,
        onClick: desc.onClick,
        decorator: desc.decorator,
        unit,
        ...chartConfig,
    };
    return <ChartErrorBoundary height={desc.height} width={width} phase={phase}>
        <Mask unitPhase={phase} isShowMask={isShowMask(session, phase)}>
            {desc.error
                ? <ChartErrorDiv height={desc.height}>
                    <div className="text">
                        {t('RenderError')}
                    </div>
                </ChartErrorDiv>
                : React.createElement(chartMap[desc.type], { key: `${serial} chart`, ...chartProps })
            }
        </Mask>
    </ChartErrorBoundary>;
});
