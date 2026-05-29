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
import type React from 'react';
import { type RefObject, useEffect, useMemo, useRef, useState } from 'react';
import type { ChartData, ChartType, MapFunc } from '../../entity/chart';
import type { Session } from '../../entity/session';
import { logger } from '../../utils/Logger';
import { runInAction } from 'mobx';
import type { InsightUnit } from '../../entity/insight';
import { useTheme } from '@emotion/react';
import { getTimeOffset } from '../../insight/units/utils';
import type { ThreadTraceRequest } from '../../entity/data';
import { getUnitUniqueId } from '../../utils';

export interface Pos {
    x: number;
    y: number;
};

const THREAD_FETCH_TIMEOUT = 600;

export type DataProcessor<T extends ChartType> = (data: ChartData<T>, width: number, domainStart: number, domainEnd: number) => ChartData<T>;
const CLICK_TOLERANCE = 1;

interface UseDataParams<T extends ChartType> {
    session: Session;
    mapFunc: MapFunc<T>;
    unit: InsightUnit;
    metadata: unknown;
    width: number;
    processor?: DataProcessor<T>;
}

let timer: NodeJS.Timeout | null = null;

/**
 * 自动获取连线数据
 * @param session
 * @param unit
 */
function onAutoFetchLines(session: Session, unit: InsightUnit): void {
    // 不存在未请求过的泳道或泳道类型不是Thread，则不需要自动获取连线数据
    if (session.threadsToFetch.size === 0 || unit.name !== 'Thread') return;
    const unitKey = getUnitUniqueId(unit);
    const _unit = session.threadsToFetch.get(unitKey);
    // session.threadsToFetch中不包含当前unit（当前泳道已经请求过），则不需要自动获取连线数据
    if (!_unit) return;
    // 设置当前unit为已展开过
    _unit.hasExpanded = true;
    session.threadsToFetch.delete(unitKey);
    const currentNum = session.threadsToFetch.size;
    // 所有响应结束，更新连线数据
    if (currentNum === 0) {
        timer && clearTimeout(timer);
        runInAction(() => { session.shouldRefetchLines = !session.shouldRefetchLines; });
        return;
    }
    timer && clearTimeout(timer);
    // 在虚拟滚动时，不在可视窗口的泳道不会请求算子信息。此时需判断请求是否已停止且未请求过的泳道是否大于0，若满足此条件需手动
    timer = setTimeout(() => {
        const nextNum = session.threadsToFetch.size;
        if (nextNum === currentNum && nextNum > 0) {
            runInAction(() => { session.shouldRefetchLines = !session.shouldRefetchLines; });
        }
    }, THREAD_FETCH_TIMEOUT);
}

/**
 * Manages the data that are to be rendered as a state.
 *
 * @param session session
 * @param mapFunc mapFunc
 * @param metadata metadata
 * @param width width
 * @returns the data that this chart is currently rendering
 */
export const useData = <T extends ChartType>({ session, mapFunc, unit, metadata, width, processor }: UseDataParams<T>): ChartData<T> => {
    const { domainStart, domainEnd } = session.domainRange;
    const { endTimeAll } = session;
    const [dataState, setDatasState] = useState<ChartData<T>>([]);
    const requestedWidth = useRef(0);
    const theme = useTheme();
    const timestampOffset = getTimeOffset(session, metadata as ThreadTraceRequest);
    useEffect(() => {
        if (width === 0) {
            setDatasState([]);
            return;
        }
        requestedWidth.current = width;
        mapFunc(session, metadata, unit, theme).then(data => {
            // 展开泳道时须泳道的所有子项的算子查询结束才触发session.shouldRefetchLines变更。（场景：先按类型连线，再展开从未展开过的泳道）
            onAutoFetchLines(session, unit);
            if (requestedWidth.current !== width) {
                // drop the data if width has been changed since when request was made
                return;
            }
            // the data should be sorted by startTime(min -> max).
            setDatasState(processor?.(data, width, domainStart, domainEnd) ?? data);
        }).catch(() => {
            logger('hooks useData', 'mapFunc occurred an exception.');
        }).finally(() => {
            runInAction(() => { unit.phase = 'download'; });
        });
    }, [session.phase, domainStart, domainEnd, endTimeAll, width,
        timestampOffset,
        session.unitsConfig.filterConfig.pythonFunction,
        session.autoAdjustUnitHeight,
        session.areFlagEventsHidden,
        session.alignRender]);
    return dataState;
};

export const useRangeAndDomain = (session: Session, width: number, margin: number): Array<[number, number]> => {
    const { domainStart, domainEnd } = session.domainRange;
    return useMemo<Array<[number, number]>>(
        () => [[margin, width - (2 * margin)], [domainStart, domainEnd]],
        [domainStart, domainEnd, width, margin],
    );
};

/**
 * Calls renderer when deps are changed, but limit the frequency
 *
 * @param renderer the render function
 * @param deps the dependencies that triggers re-render
 */
export const useBatchedRender = (renderer: () => void, deps: React.DependencyList): void => {
    useEffect(() => {
        renderer();
    }, deps);
};

export const useHoverPosX = (ref: React.RefObject<HTMLElement>): number | undefined => {
    return useHoverPos(ref)?.x ?? undefined;
};

export const useHoverPos = (ref: React.RefObject<HTMLElement>): Pos | undefined => {
    const [mousePos, setMousePos] = useState<Pos>();
    const onMouseMove = (e: MouseEvent): void => {
        if ((e.target as HTMLElement).className === 'clickable') {
            setMousePos(undefined);
        } else {
            setMousePos({ x: e.offsetX, y: e.offsetY });
        }
    };
    const onMouseOut = (e: MouseEvent): void => { setMousePos(undefined); };
    useEffect(() => {
        if (ref.current === null) {
            return () => { };
        }
        ref.current.addEventListener('mousemove', onMouseMove);
        ref.current.addEventListener('mouseout', onMouseOut);
        return () => {
            ref.current?.removeEventListener('mousemove', onMouseMove);
            ref.current?.removeEventListener('mouseout', onMouseOut);
        };
    }, []);
    return mousePos;
};

interface UseClickParams<T extends ChartType> {
    canvasContainer: RefObject<HTMLElement>;
    dataState: ChartData<T>;
    rangeAndDomain: Array<[number, number]>;
    session: Session;
    metadata: unknown;
    handleMouseUp: (e: MouseEvent) => void;
    handleMouseMoveUp?: (xArr: number[]) => void;
}

export const useClick = <T extends ChartType>({
    canvasContainer, dataState, rangeAndDomain, session, metadata, handleMouseUp, handleMouseMoveUp,
}: UseClickParams<T>): void => {
    useEffect(() => {
        if (canvasContainer.current === null) {
            return () => { };
        }
        let mousedownX: number | null = null;
        let mouseMoved = false;
        const onMouseDownListener = (e: MouseEvent): void => {
            mousedownX = e.offsetX;
            mouseMoved = false;
        };
        const onMouseMoveListener = (e: MouseEvent): void => {
            if (mousedownX !== null && Math.abs(mousedownX - e.offsetX) > CLICK_TOLERANCE) {
                mouseMoved = true;
            }
        };
        const onMouseUpListener = (e: MouseEvent): void => {
            if (mousedownX !== null && Math.abs(mousedownX - e.offsetX) > CLICK_TOLERANCE) {
                runInAction(() => {
                    session.selectedData = undefined;
                    session.drawLineMode = 'all';
                });
                if (mouseMoved && mousedownX !== null) {
                    handleMouseMoveUp?.([mousedownX, e.offsetX]);
                }
                return;
            }
            if (mousedownX !== null && !mouseMoved) {
                handleMouseUp(e);
            }
        };
        // shouldn't call onClick if mouseup point != mousedown point
        canvasContainer.current.addEventListener('mousedown', onMouseDownListener);
        canvasContainer.current.addEventListener('mouseup', onMouseUpListener);
        canvasContainer.current.addEventListener('mousemove', onMouseMoveListener);
        return () => {
            canvasContainer.current?.removeEventListener('mousedown', onMouseDownListener);
            canvasContainer.current?.removeEventListener('mouseup', onMouseUpListener);
            canvasContainer.current?.removeEventListener('mousemove', onMouseMoveListener);
        };
    }, [dataState, rangeAndDomain, session, metadata]);
};
