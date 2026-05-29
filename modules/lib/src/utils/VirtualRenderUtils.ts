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
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

const getSize = ({ visibleHeight, itemHeight, count }:
{visibleHeight: number;itemHeight: number;count: number}):
{
    visibleCount: number;
    totalHeight: number;
} => {
    if (itemHeight <= 0) {
        return { visibleCount: 0, totalHeight: visibleHeight };
    }
    const visibleCount = Math.ceil(visibleHeight / itemHeight) + 2;
    const totalHeight = itemHeight * count;
    return { visibleCount, totalHeight };
};

const initVirtual = ({ boxElement, targetElement, scrollEvent, totalHeight, resetScroll }:
{
    scrollEvent: (e: Event) => void;
    totalHeight: number;
    boxElement: HTMLElement | null;
    targetElement: HTMLElement | null;
    resetScroll: boolean;
},
) : (() => void) => {
    const isNullElement = boxElement === null || boxElement === undefined || targetElement === null || targetElement === undefined;
    if (isNullElement) {
        return (): void => {};
    }
    // 占位div
    const placeholderWrapper = document.createElement('div');
    placeholderWrapper.style.height = `${totalHeight}px`;
    boxElement.appendChild(placeholderWrapper);
    boxElement.style.position = 'relative';
    if (totalHeight === 0) {
        Object.assign(targetElement.style, { position: 'relative' });
    } else {
        Object.assign(targetElement.style, { position: 'absolute', top: '0', left: '0' });
    }
    // 添加滚动事件
    boxElement.addEventListener('scroll', scrollEvent);
    if (resetScroll) {
        boxElement.scrollTo({ top: 0 });
    }
    return (): void => {
        // 清理占位div
        boxElement.removeChild(placeholderWrapper);
        boxElement.removeEventListener('scroll', scrollEvent);
    };
};

export const useWatchVirtualRender = <T>({ visibleHeight, itemHeight, dataSource, resetScroll = true }: {
    visibleHeight: number;
    itemHeight: number;
    dataSource: readonly T[];
    resetScroll?: boolean;
},
): { data: readonly T[];boxRef: React.MutableRefObject<null>;targetRef: React.MutableRefObject<null>} => {
    const boxRef = useRef(null);
    const targetRef = useRef(null);

    const [range, setRange] = useState<[number, number]>([0, 0]);
    const prevDataSourceLengthRef = useRef(dataSource.length);

    const { visibleCount, totalHeight } = useMemo(() => getSize({ visibleHeight, itemHeight, count: dataSource.length }),
        [visibleHeight, itemHeight, dataSource.length]);

    // 将 scrollEvent 和 setPostion 抽取到一个稳定的函数中，以便在 useEffect 依赖项中使用
    const updateRangeAndPosition = useCallback(() => {
        if(itemHeight <= 0 || boxRef.current === null || targetRef.current === null) {
            return;
        }

        const start = Math.floor((boxRef.current as HTMLElement).scrollTop / itemHeight);
        const end = start + visibleCount;
        setRange([start, end]);
        const offset = start * itemHeight;
        (targetRef.current as HTMLElement).style.top = `${offset}px`;
    }, [visibleCount, totalHeight, targetRef.current, boxRef.current]);

    const scrollEvent = useCallback(() => {
        updateRangeAndPosition();
    }, [updateRangeAndPosition]);

    useEffect(() => {
        const currentLength = dataSource.length;
        const isLengthChanged = currentLength !== prevDataSourceLengthRef.current;

        // 初始化范围
        setRange([0, visibleCount]);

        prevDataSourceLengthRef.current = currentLength;

        // 初始化虚拟滚动逻辑
        const cleanup = initVirtual({
            scrollEvent,
            totalHeight,
            targetElement: targetRef.current,
            boxElement: boxRef.current,
            resetScroll,
        });

        // 如果数据长度发生变化（即追加数据），则主动更新一次范围和位置
        if (isLengthChanged) {
            updateRangeAndPosition();
        }

        return cleanup;
    }, [scrollEvent, totalHeight, dataSource.length, updateRangeAndPosition]); // 添加 updateRangeAndPosition 作为依赖

    const renderData = useMemo(() => dataSource.slice(...range), [dataSource, range]);
    return { data: renderData, boxRef, targetRef };
};
