/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import styled from '@emotion/styled';
import React from 'react';
import { LLC_CACHE_COLORS, type LlcCacheStackedBarData } from './llcCache';

const Panel = styled.div`
    box-sizing: border-box;
    width: 402px;
    padding: 12px 14px 13px;
    color: ${(props): string => props.theme.tooltipFontColor};
    font-size: 12px;
    line-height: 1.25;
`;

const TimeRange = styled.div`
    margin-bottom: 5px;
    font-weight: 700;
    letter-spacing: 0.01em;
`;

const Summary = styled.div`
    margin-bottom: 10px;
    color: ${(props): string => props.theme.textColorSecondary};
`;

const MetricRow = styled.div`
    display: grid;
    grid-template-columns: 10px 72px 82px 124px 48px;
    gap: 7px;
    align-items: center;
    min-height: 22px;
`;

const Swatch = styled.span<{ color: string }>`
    display: block;
    width: 9px;
    height: 9px;
    border-radius: 1px;
    background: ${(props): string => props.color};
`;

const Count = styled.span`
    color: ${(props): string => props.theme.textColorSecondary};
    text-align: right;
    font-variant-numeric: tabular-nums;
`;

const BarTrack = styled.span`
    display: block;
    height: 8px;
    overflow: hidden;
    border-radius: 2px;
    background: ${(props): string => props.theme.borderColorLight};
`;

const BarFill = styled.span<{ color: string; width: number }>`
    display: block;
    width: ${(props): string => `${Math.max(0, Math.min(100, props.width))}%`};
    height: 100%;
    background: ${(props): string => props.color};
`;

const Percentage = styled.span`
    text-align: right;
    color: ${(props): string => props.theme.tooltipFontColor};
    font-variant-numeric: tabular-nums;
`;

const formatTime = (nanoseconds: number): string => `${(nanoseconds / 1_000_000_000).toFixed(2)}s`;

const formatInterval = (nanoseconds: number): string => `${(nanoseconds / 1_000_000_000).toFixed(2)}s`;

const formatCount = (value: number): string => Math.round(value).toLocaleString('en-US');

interface LlcCacheTooltipProps {
    data: LlcCacheStackedBarData;
}

export const LlcCacheTooltip = ({ data }: LlcCacheTooltipProps): JSX.Element => {
    const endTime = data.timestamp + data.bucketWidthNs;
    const metrics = [
        { label: 'LLC Misses', count: data.misses, rate: data.missRate },
        { label: 'LLC Hits', count: data.hits, rate: data.hitRate },
    ];
    return <Panel>
        <TimeRange>{formatTime(data.timestamp)} - {formatTime(endTime)}</TimeRange>
        <Summary>
            Aggregation Interval: {formatInterval(data.bucketWidthNs)} · Total Accesses: {formatCount(data.totalAccesses)}
        </Summary>
        {metrics.map((metric, index) => {
            const color = LLC_CACHE_COLORS[index];
            return <MetricRow key={metric.label}>
                <Swatch color={color}/>
                <span>{metric.label}</span>
                <Count>{formatCount(metric.count)}</Count>
                <BarTrack aria-hidden="true"><BarFill color={color} width={metric.rate}/></BarTrack>
                <Percentage>{metric.rate.toFixed(1)}%</Percentage>
            </MetricRow>;
        })}
    </Panel>;
};
