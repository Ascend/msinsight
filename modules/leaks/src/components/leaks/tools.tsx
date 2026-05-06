
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

import { observer } from 'mobx-react';
import { Session } from '../../entity/session';
import React, { useEffect, useState } from 'react';
import { type Theme, useTheme } from '@emotion/react';
import { runInAction } from 'mobx';
import styled from '@emotion/styled/macro';
import { formatBytes, formatTime } from '@/utils/utils';
import { Spin } from '@insight/lib';

interface AxisTick {
    position: number;
    value: string;
}

const getTransformScaleX = (transform: RenderOptions['transform']): number => transform.scaleX ?? transform.scale;
const getTransformScaleY = (transform: RenderOptions['transform']): number => transform.scaleY ?? transform.scale;

export const Axis = observer(({ session }: { session: Session }): JSX.Element => {
    const { sizeInfo, renderOptions: { zoom, transform, viewport } } = session.leaksWorkerInfo;
    const [axisY, setAxisY] = useState<AxisTick[]>([]);
    const [axisX, setAxisX] = useState<AxisTick[]>([]);
    const theme: Theme = useTheme();

    const computeAxisY = (): AxisTick[] => {
        const { minSize, maxSize } = sizeInfo;
        const { height } = viewport;
        if (minSize === maxSize) return [];

        const scaleY = getTransformScaleY(transform);
        const visibleRange = (maxSize - minSize) / scaleY;
        const visibleMinSize = minSize - transform.y / scaleY / zoom.y;

        const ticks = [];

        // 生成 6 个 tick（0 到 5/5，即包括顶部）
        for (let i = 0; i <= 5; i++) {
            const position = (i / 5) * height;
            const byteValue = visibleMinSize + (i / 5) * visibleRange;
            ticks.push({
                position,
                value: formatBytes(byteValue),
            });
        }

        return ticks;
    };

    const computeAxisX = (): AxisTick[] => {
        const { minTimestamp, maxTimestamp } = sizeInfo;
        const { width } = viewport;
        if (minTimestamp === maxTimestamp) return [];
        const scaleX = getTransformScaleX(transform);
        const visibleRange = (maxTimestamp - minTimestamp) / scaleX;
        const visibleMinSize = minTimestamp - transform.x / scaleX / zoom.x;
        const visibleMaxSize = visibleMinSize + visibleRange;

        const ticks = [];

        for (let i = 0; i < 5; i++) {
            const position = (i / 5) * width;
            const timeValue = visibleMinSize + (i / 5) * visibleRange;

            ticks.push({
                position,
                value: formatTime(timeValue, session.module),
            });
        }

        ticks.push({
            position: width,
            value: formatTime(visibleMaxSize, session.module),
        });
        return ticks;
    };

    useEffect(() => {
        setAxisY(computeAxisY());
        setAxisX(computeAxisX());
    }, [sizeInfo, zoom, transform, viewport]);

    return <div
        style={{
            position: 'absolute',
            pointerEvents: 'none',
            top: 0,
            left: 0,
            width: viewport.width ?? 0,
            height: viewport.height ?? 0,
        }}
    >
        <div
            className="axisY"
            style={{
                position: 'absolute',
                top: 0,
                left: -100,
                borderRight: `1px solid ${theme.textColorPrimary}`,
                height: '100%',
                width: 100,
            }}
        >
            {axisY.map((item, index) => (
                <div key={`axisY_tick_${index}`}
                    style={{ position: 'absolute', bottom: item.position, display: 'flex', width: '100%', alignItems: 'flex-end' }}>
                    <div style={{ flex: 1, textAlign: 'right', transform: 'translateY(50%)' }}>{item.value}</div>
                    <div style={{ height: 1, width: 4, background: theme.textColorPrimary, marginLeft: 2 }} />
                </div>
            ))}
        </div>
        <div
            className="axisX"
            style={{
                position: 'absolute',
                bottom: -20,
                left: 0,
                borderTop: `1px solid ${theme.textColorPrimary}`,
                width: '100%',
                height: 20,
            }}
        >
            {axisX.map((item, index) => (
                <div key={`axisX_tick_${index}`}
                    style={{ position: 'absolute', left: item.position, height: '100%' }}>
                    <div style={{ height: 4, width: 1, background: theme.textColorPrimary, marginBottom: 2 }} />
                    <div style={{ flex: 1, textAlign: 'right', transform: 'translateX(-50%)' }}>{item.value}</div>
                </div>
            ))}
        </div>
    </div>;
});

export const MarkLineCommon = ({ left, label }: { left: number; label: string }): JSX.Element => {
    const theme: Theme = useTheme();
    return <>
        {
            left > -1
                ? <div className="mark-line" style={{
                    position: 'absolute',
                    pointerEvents: 'none',
                    top: 0,
                    left,
                    height: '100%',
                }}>
                    <div style={{ width: 1, height: '100%', background: theme.textColorPrimary }} />
                    <div style={{ transform: 'translateX(-50%)', background: theme.bgColorCommon, padding: '0 5px', borderRadius: 4 }}>{label}</div>
                </div>
                : <></>
        }
    </>;
};

export const MarkLineBlock = observer(({ session }: { session: Session }): JSX.Element => {
    const { sizeInfo, renderOptions: { zoom, transform, viewport } } = session.leaksWorkerInfo;
    const { block, stack, currentTimestamp } = session.markLineInfo;
    const [currentTime, setCurrentTime] = useState<AxisTick>({ position: -1, value: '' });

    const getCurrentTime = (): void => {
        const { minTimestamp, maxTimestamp } = sizeInfo;
        const { width } = viewport;
        const scaleX = getTransformScaleX(transform);
        const visibleRange = (maxTimestamp - minTimestamp) / scaleX;
        const visibleMinSize = minTimestamp - transform.x / scaleX / zoom.x;
        const ratio = block.x / width;
        const currentTimestamp = visibleMinSize + visibleRange * ratio;

        runInAction(() => {
            session.markLineInfo.currentTimestamp = currentTimestamp;
        });
    };

    useEffect(() => {
        getCurrentTime();
    }, [block, viewport]);

    useEffect(() => {
        const { minTimestamp, maxTimestamp } = sizeInfo;
        const { width } = viewport;
        const scaleX = getTransformScaleX(transform);
        const visibleRange = (maxTimestamp - minTimestamp) / scaleX;
        const visibleMinSize = minTimestamp - transform.x / scaleX / zoom.x;

        const ratio = (currentTimestamp - visibleMinSize) / visibleRange;
        const offset = width * ratio;
        const position = offset > width || offset < 0 || (stack.x < 0 && block.x < 0) ? -1 : offset;
        setCurrentTime({ position, value: formatTime(currentTimestamp, session.module) });
    }, [currentTimestamp]);

    return <MarkLineCommon left={currentTime.position} label={currentTime.value} />;
});

export const MarkLineStack = observer(({ session, width }: { session: Session; width: number }): JSX.Element => {
    const { minTime, maxTime } = session;
    const { block, stack, currentTimestamp } = session.markLineInfo;
    const [currentTime, setCurrentTime] = useState<AxisTick>({ position: -1, value: '' });

    const getCurrentTime = (): void => {
        const visibleRange = maxTime - minTime;
        const ratio = stack.x / width;
        const currentTimestamp = minTime + visibleRange * ratio;

        runInAction(() => {
            session.markLineInfo.currentTimestamp = currentTimestamp;
        });
    };

    useEffect(() => {
        getCurrentTime();
    }, [stack.x, width]);

    useEffect(() => {
        const visibleRange = maxTime - minTime;

        const ratio = (currentTimestamp - minTime) / visibleRange;
        const offset = width * ratio;
        const position = offset > width || offset < 0 || (stack.x < 0 && block.x < 0) ? -1 : offset;
        setCurrentTime({ position, value: formatTime(currentTimestamp, session.module) });
    }, [currentTimestamp]);

    return <MarkLineCommon left={currentTime.position} label={currentTime.value} />;
});

const HoverItemContainer = styled.div`
    position: absolute;
    padding: 10px;
    pointer-events: none;
    border-radius: 5px;
    background: ${(props): string => props.theme.bgColorDark};
`;

const RIGHT_MARGIN = 200;
const BOTTOM_MARGIN = 100;

export const HoverItem = observer(({ session }: { session: Session }): JSX.Element => {
    const { leaksWorkerInfo: { hoverItem, renderOptions: { viewport } }, markLineInfo } = session;
    const left = markLineInfo.block.x + RIGHT_MARGIN > viewport.width ? markLineInfo.block.x - RIGHT_MARGIN : markLineInfo.block.x + 20;
    const top = markLineInfo.block.y + BOTTOM_MARGIN > viewport.height ? markLineInfo.block.y - BOTTOM_MARGIN : markLineInfo.block.y;

    return <>
        {
            markLineInfo.block.x < 0 || markLineInfo.block.y < 0 || hoverItem === null
                ? <></>
                : <HoverItemContainer style={{ left, top }}>
                    <div>Addr: {hoverItem.addr}</div>
                    <div>Size: {formatBytes(hoverItem.size)}</div>
                    <div>Life: {formatTime(hoverItem._endTimestamp - hoverItem._startTimestamp, session.module)}</div>
                </HoverItemContainer>
        }
    </>;
});

export const StateHoverItem = observer(({ session, point }: { session: Session; point: { x: number; y: number } }): JSX.Element => {
    const { stateWorkerInfo: { hoverItem, renderOptions: { viewport } } } = session;
    if (point.x < 0 || point.y < 0 || hoverItem === null) {
        return <></>;
    }
    const left = point.x + RIGHT_MARGIN > viewport.width ? point.x - RIGHT_MARGIN : point.x + 20;
    const top = point.y + BOTTOM_MARGIN > viewport.height ? point.y - BOTTOM_MARGIN : point.y;
    const block = hoverItem.data.blocks.length > 0 ? hoverItem.data.blocks[0] : undefined;
    const size = hoverItem.type === 'block' && block !== undefined ? block.size : hoverItem.data.size;

    return <HoverItemContainer style={{ left, top }}>
        <div>Type: {hoverItem.type}</div>
        <div>Addr: {hoverItem.data.address}</div>
        <div>Size: {formatBytes(size)}</div>
        <div>Stream: {hoverItem.data.stream}</div>
        <div>Event: {hoverItem.data.allocOrMapEventId}</div>
        {block === undefined ? <></> : <div>Offset: {formatBytes(block.offset)}</div>}
    </HoverItemContainer>;
});

const LoadingContainer = styled.div`
    display: flex;
    justify-content: center;
    align-items: center;
    background: ${(props): string => props.theme.bgColor};
`;
export const Loading = ({ size = 'default', style = {}, loading }: { size?: 'small' | 'default' | 'large'; style?: React.CSSProperties; loading: boolean }): JSX.Element => {
    return <>
        {
            loading
                ? <LoadingContainer style={style}>
                    <Spin size={size} />
                </LoadingContainer>
                : <></>
        }
    </>;
};

export const GraphToolbar = styled.div`
    display: flex;
    align-items: center;
    justify-content: flex-end;
    gap: 6px;
    min-height: 30px;
    margin-bottom: 6px;

    button {
        height: 24px;
        min-width: 74px;
        padding: 0 8px;
        color: #222;
        font-size: 12px;
        line-height: 22px;
        white-space: nowrap;
        background: #fff;
        border: 1px solid rgba(0, 0, 0, 0.22);
        border-radius: 4px;
        cursor: pointer;
    }

    button:hover {
        background: #f5f5f5;
        border-color: rgba(0, 0, 0, 0.36);
    }

    button:active {
        background: #e8e8e8;
    }
`;
