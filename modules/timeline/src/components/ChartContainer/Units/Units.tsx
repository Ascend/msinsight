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
import styled from '@emotion/styled';
import cls from 'classnames';
import { computed, runInAction } from 'mobx';
import { observer } from 'mobx-react';
import * as React from 'react';
import { useEffect, useRef, useState } from 'react';
// hooks
import { useWatchResize } from '../../../utils/useWatchDomResize';
// support utils/types
import { preOrderFlatten, type TreeNode } from '../../../entity/common';
import type { InsightUnit } from '../../../entity/insight';
import type { Session } from '../../../entity/session';
import { getAutoKey } from '../../../utils/dataAutoKey';
import { traceSingle } from '../../../utils/traceLogger';
import { Chart } from '../../charts';
import { isPinned, isSonPinned } from '../unitPin';
import { useDeselectUnits, useSelectUnit, useSelectUnits } from './hooks/useSelectUnit';
import type { KeyedInsightUnit } from './types';
import { UnitInfo } from './UnitInfo';
import { ChartErrorBoundary } from '../../error/ChartErrorBoundary';
import eventBus, { EventType, useEventBus } from '../../../utils/eventBus';
import { Mask } from '../../charts/Mask';
import type { OrderOptions } from './hooks';
import { useJumpTarget } from './hooks';
import { CardUnit } from '../../../insight/units/AscendUnit';
import { getRootUnit } from '../../../utils';
import { MetaDataBase, ThreadMetaData } from '../../../entity/data';
import { checkIsSliceMode, PAGE_PADDING } from '../../charts/ChartInteractor/draw';
import { MouseButton } from '../../charts/ChartInteractor/actions';

const Lane = styled.div<{ laneHeight: number; className: string; top: number; zIndex: number; isDragging: boolean }>`
    position: ${(props): string => props.isDragging ? 'fixed' : 'relative'};
    display: flex;
    box-sizing: border-box;
    flex-direction: row;
    height: ${(props): number => props.laneHeight}px;
    border-bottom: solid 1px ${(props): string => props.theme.borderColor};
    top: ${(props): number => props.top}px;
    z-index: ${(props): number => props.zIndex};
    transform: ${(props): string => props.isDragging ? 'scale(1.01)' : 'scale(1)'};
    transition: transform .2s, box-shadow .2s;
    .unit-info {
        background-color: ${(props): string => getUnitBackgroundColor(props)};
        div svg g use {
            fill: ${(props): string => props.theme.fontColor};
        }
    }
    .chart-selected {
        box-shadow: 0 0 0 2px ${(props): string => props.theme.selectedChartBorderColor} inset;
    }
`;

const VIRTUAL_SCROLL_THRESHOLD = 30;
const MOUSE_STOPPED_THRESHOLD = 50;
const UNIT_SELECTED = 'unit-selected';
const UNIT_VISIBLE = 'unit-visible';
const UNIT_EMPTY = 'empty';
export const UNIT_WRAPPER_SCROLLER_ID = 'unitWrapperScroller';
export const PINNED_UNIT_WRAPPER_SCROLLER_ID = 'pinnedUnitWrapperScroller';
const UP = 'up';
const DOWN = 'down';
let scrollDirection = '';
let moveDirectionHasChange = false;

const getUnitBackgroundColor = (props: any): string => {
    if (props.className.includes(UNIT_SELECTED)) {
        return props.theme.selectedChartBackgroundColor;
    }

    if (props.children.length > 0 && props.children[0].props.className.includes(UNIT_EMPTY)) {
        return props.theme.bgColorLight;
    }

    return props.theme.contentBackgroundColor;
};

const Splitter = styled.div`
    width: 100%;
    height: 1px;
    background-color: ${(props): string => props.theme.borderColor};
`;

const Join = (props: { joiner: React.FC; children: JSX.Element[] }): JSX.Element => (
    <>
        {props.children.slice(1).reduce((prev, cur, index) => [...prev, <props.joiner key={`joiner/${index}`}/>, cur], [props.children[0]])}
    </>
);

const ChartView = observer(({ unit, session, width, height }: {unit: KeyedInsightUnit; session: Session; width: number; height: number }): JSX.Element => {
    if (Array.isArray(unit.chart)) {
        return <Join joiner={Splitter}>
            {unit.chart.map((desc, index) =>
                <Chart desc={desc} key={`${getAutoKey(unit)}/${index}`} serial={`${getAutoKey(unit)}/${index}`} unit={unit}
                    title={unit.name} session={session} metadata={unit.metadata} width={width} phase={unit.phase} />,
            )}
        </Join>;
    }
    if (unit.chart !== undefined) {
        return <Chart
            unit={unit} desc={unit.chart} key={getAutoKey(unit)} serial={getAutoKey(unit)}
            title={unit.name} session={session} metadata={unit.metadata} width={width} phase={unit.phase} />;
    }
    if (unit instanceof CardUnit && (unit.phase === 'analyzing' || unit.phase === 'download')) {
        return <ChartErrorBoundary height={height} width={width} phase={unit.phase}>
            <div className="chart-empty" style={{ width, height }}/>
        </ChartErrorBoundary>;
    } else {
        return <ChartErrorBoundary height={height} width={width} phase={unit.phase}>
            <Mask unitPhase={unit.phase} isShowMask={session.id !== 'HomePage' && session.phase !== 'error' && unit.phase !== 'configuring' && unit.phase !== 'download' && unit.phase !== 'error'}>
                <div className="chart-empty" style={{ width, height }}/>
            </Mask>
        </ChartErrorBoundary>;
    }
});

interface UnitProps {
    unit: KeyedInsightUnit;
    session: Session;
    isVisible: boolean;
    hasPinButton: boolean;
    laneInfoWidth: number;
    hasExpandIcon: boolean;
    isPinned: boolean;
    isSonPinned: boolean;
    isPinnedArea?: boolean;
    nextUnitTop?: number;
    isSelecting?: boolean;
    inRangeUnitKeys?: Set<string>;
    forwardedRef?: React.ForwardedRef<HTMLDivElement>;
    enableDrag?: boolean;
    startYRef?: React.MutableRefObject<number>;
    getDraggingUnitIndexByKey?: (key: string) => void;
    setNextUnitTop?: React.Dispatch<React.SetStateAction<number>>;
    onMouseMove?: (e: MouseEvent, isMoveDown: boolean) => void;
}

const isSelectable = (unit: KeyedInsightUnit): boolean => {
    return !['Card', 'Root'].includes(unit?.name);
};

export const UnitObserver = observer((
    { unit, session, isVisible, isSelecting, inRangeUnitKeys, forwardedRef, startYRef, ...props }: UnitProps,
): JSX.Element => {
    const unitKey = getAutoKey(unit);
    const isSelected = session.selectedUnitKeys.includes(unitKey);
    const [chartWidth, ref] = useWatchResize<HTMLDivElement>('width');
    const height = unit.height() + 1; // to support modifying height from outside during runtime, don't useMemo
    const placeholder = React.useMemo(() => {
        return <div className="chart-invisible" style={{ width: chartWidth, height }}/>;
    }, [chartWidth, height]);
    // to support auto redraw when unit height is modified, don't useMemo
    const chart = <ChartView unit={unit} session={session} width={chartWidth} height={height} />;
    const clickedUnit = session.selectedUnits[0];
    const selectUnit = useSelectUnit(session);
    const selectUnits = useSelectUnits(session);
    const deSelectUnits = useDeselectUnits(session);

    const unitMetaData = unit?.metadata as ThreadMetaData;
    const clickedUnitMetaData = clickedUnit?.metadata as ThreadMetaData;
    const isSameCard = unitMetaData.cardId === clickedUnitMetaData?.cardId || unitMetaData.cardId?.endsWith('.db');

    // 拖拽泳道相关
    const [top, setTop] = useState<number>(0);
    const [zIndex, setZIndex] = useState<number>(0);
    const [isCurrentDragging, setIsCurrentDragging] = useState<boolean>(false);
    // 鼠标点击位置离拖拽泳道上边框的距离
    const elementTop = useRef<number>(0);
    const previousY = useRef<number>(0);

    const onMouseDown = (e: React.MouseEvent): void => {
        e.preventDefault();
        document.addEventListener('mousemove', onMouseMove);
        document.addEventListener('mouseup', onMouseUp, { once: true });
        const ele = e.currentTarget.getBoundingClientRect();
        elementTop.current = e.clientY - ele.top;
        previousY.current = e.clientY;
        props.getDraggingUnitIndexByKey?.(unitKey);
        props.setNextUnitTop?.(ele.height);
        setZIndex(999);
        setIsCurrentDragging(true);
        setTop(ele.top);
    };

    const onMouseMove = (e: MouseEvent): void => {
        e.preventDefault();
        const topDistance = e.clientY - elementTop.current;
        // 拖拽过程中鼠标位置超出置顶区域时垂直方向触发滚动事件
        const scrollElement = document.getElementById(PINNED_UNIT_WRAPPER_SCROLLER_ID);
        if (scrollElement) {
            const rect = scrollElement.getBoundingClientRect();
            if (e.clientY > rect.bottom) {
                scrollElement.scrollBy(0, 10);
            }
            if (e.clientY < rect.top) {
                scrollElement.scrollBy(0, -10);
            }
        }
        if (e.clientY !== previousY.current) {
            props.onMouseMove?.(e, e.clientY > previousY.current);
        }
        previousY.current = e.clientY;
        setTop(topDistance);
        runInAction(() => {
            session.isDragging = true;
        });
    };

    const onMouseUp = (e: MouseEvent): void => {
        e.preventDefault();
        e.stopPropagation();
        document.removeEventListener('mousemove', onMouseMove);
        previousY.current = 0;
        props.setNextUnitTop?.(0);
        setTop(0);
        setZIndex(0);
        setIsCurrentDragging(false);
        runInAction(() => {
            session.isDragging = false;
        });
    };

    useEffect(() => {
        if (inRangeUnitKeys === undefined || [undefined, false].includes(isSelecting)) { return; }
        if (isSelectable(unit) && isSelectable(clickedUnit) && isSameCard) {
            if (inRangeUnitKeys?.has(unitKey)) {
                selectUnits(unit);
            } else {
                deSelectUnits(unit);
            }
        }
    }, [inRangeUnitKeys?.size]);

    return <Lane className={cls('unit', {
        [UNIT_SELECTED]: isSelected,
        [UNIT_VISIBLE]: isVisible,
    })} laneHeight={height} ref={forwardedRef} top={props.nextUnitTop ?? top} zIndex={zIndex} isDragging={isCurrentDragging}>
        <UnitInfo
            className={unit.name === 'Empty' ? 'empty' : ''}
            height={height}
            session={session}
            unit={unit}
            isSelected={isSelected}
            onMouseDown={onMouseDown}
            {...props}
        />
        <div className={`${isSelected ? 'chart-selected' : 'chart'} ${getClassNameByMetadata(unit)}`} ref={ref}
            onMouseDown={(e): void => {
                if (session.selectedRangeIsLock) {
                    return;
                }
                if (e.button !== MouseButton.LEFT && session.selectedRange !== undefined) {
                    return;
                }
                if (session.sliceSelection.active) {
                    session.resetOfSliceSelection();
                    if (unit.name === 'Thread') {
                        session.sliceSelection.targetUnit = unit as InsightUnit;
                    }
                }
                selectUnit(unit);
                traceSingle('selectLane', [unit.name]);
            }}
            style={{ flexGrow: 1, minWidth: 0 }}>
            { isVisible ? chart : placeholder }
        </div>
    </Lane>;
});

export const Unit = React.forwardRef((props: UnitProps, ref: React.ForwardedRef<HTMLDivElement>) => {
    return <UnitObserver {...props} forwardedRef={ref}></UnitObserver>;
});

Unit.displayName = 'Unit';

export const computeVisibleUnitRange = (units: InsightUnit[], viewportHeight: number, scrollTop: number): [ number, number ] => {
    let start = 0;
    let end = 0;
    let yOffset = 0;
    for (let i = 0; i < units.length; i++) {
        if (yOffset + VIRTUAL_SCROLL_THRESHOLD > viewportHeight + scrollTop) {
            break;
        }
        if (yOffset + units[i].height() + VIRTUAL_SCROLL_THRESHOLD < scrollTop) {
            start++;
        }
        yOffset += units[i].height() + 1;
        end++;
    }
    return [start, end + 1];
};

export function getClassNameByMetadata(unit: InsightUnit): string {
    if (unit.name !== 'Thread') {
        return '';
    }
    const metadata = unit.metadata as ThreadMetaData;
    const pid = metadata.processId;
    const tid = Array.isArray(metadata.threadIdList) ? metadata.threadIdList.join('_') : metadata.threadId;
    return (pid && tid ? `${pid}_${tid}` : '').replace(/\s+/g, '_');
}

interface FlattenUnitsProps {
    session: Session;
    height: number;
    laneInfoWidth: number;
    hasPinButton: boolean;
    eventType: string;
};

const orderOptions = {
    preOrderFlatten,
    options: {
        when: (unit: TreeNode<InsightUnit>): boolean => unit.isExpanded,
        bypass: (unit: TreeNode<InsightUnit>): boolean => unit.type === 'transparent',
        exclude: (unit: TreeNode<InsightUnit>): boolean => (unit.pinType === 'move' && isPinned(unit)) || !unit.isDisplay || unit.isMultiDeviceHidden,
    },
};

const updateListener = (element: HTMLDivElement): any => {
    element.addEventListener('wheel', handleWheel, { passive: false });
    return () => {
        element.removeEventListener('wheel', handleWheel);
    };
};

const updateSession = (session: Session, totalHeight: number, expandedCardIdSet: Set<string>): void => {
    session.totalHeight = totalHeight;
    if (session.viewedExpandedCardIdSet.size !== expandedCardIdSet.size || [...expandedCardIdSet].some((id) => !session.viewedExpandedCardIdSet.has(id))) {
        session.viewedExpandedCardIdSet = expandedCardIdSet;
    }
};

interface MetaDataWithCardId extends MetaDataBase {
    cardId: string;
}

const computeOnScreenExpandedCardIdSet = (flattenUnits: InsightUnit[], first: number, last: number): Set<string> => {
    return new Set(
        flattenUnits
            .filter((unit, i) => {
                const id = (unit?.metadata as MetaDataWithCardId)?.cardId;
                const isExpanded = unit.isExpanded;
                return first <= i && i < last && id !== undefined && !id.endsWith('Host') && isExpanded;
            }).map(unit => (unit?.metadata as MetaDataWithCardId)?.cardId),
    );
};

const FlattenUnits = observer(({ session, height, hasPinButton, laneInfoWidth, eventType }: FlattenUnitsProps): JSX.Element => {
    const [scrollTop, setScrollTop] = React.useState(0);
    // 监听滚动事件，计算虚拟滚动的泳道
    useEventBus(eventType, (value) => setScrollTop(value as number));
    const flattenUnitsAll = computed(() => orderOptions.preOrderFlatten(getRootUnit(session.units), 0, orderOptions.options)).get();
    const flattenUnits = computed(() => flattenUnitsAll.filter(unit => unit.isUnitVisible && !unit.isMerged)).get();
    const [first, last] = React.useMemo(() => computeVisibleUnitRange(flattenUnits, height, scrollTop),
        [session.pinnedUnits, flattenUnits, height, scrollTop],
    );
    const headOffset = React.useMemo(
        () => flattenUnits.filter((_, i) => i < first).reduce((prev, cur) => prev + cur.height() + 1, 0),
        [flattenUnits, first],
    );
    const visibleUnitsHeight = React.useMemo(
        () => flattenUnits.filter((_, i) => first <= i && i < last).reduce((prev, cur) => prev + cur.height() + 1, 0),
        [flattenUnits, first, last],
    );
    const tailOffset = React.useMemo(
        () => flattenUnits.filter((_, i) => i >= last).reduce((prev, cur) => prev + cur.height() + 1, 0),
        [flattenUnits, last],
    );
    const totalHeight = React.useMemo(() => headOffset + visibleUnitsHeight + tailOffset, [headOffset, visibleUnitsHeight, tailOffset]);
    const expandedCardIdSet = computeOnScreenExpandedCardIdSet(flattenUnits, first, last);
    const [lastMouseMoveTime, setLastMouseMoveTime] = useState<Date>(new Date());
    const autoScrollIntervals = useRef<NodeJS.Timeout[]>([]);
    const mergedSelected = new Set<string>();

    const [isSelecting, setIsSelecting] = useState(false);
    const [selectedUnitKeys, setSelectedUnitKeys] = useState<Set<string>>(new Set());
    const startPoint = useRef({ x: 0, y: 0 });
    // 记录相对于当前屏幕的绝对值Y，用于滑动判断
    const startAbsPointY = useRef({ y: 0 });
    const unitsRefs = useRef(new Map());
    // 记录鼠标移动后的Y值，用于鼠标移动方向判断
    const [movementChangeY, setMovementChangeY] = useState(0);
    const scrollElement = document.getElementById(UNIT_WRAPPER_SCROLLER_ID);

    runInAction(() => {
        updateSession(session, totalHeight, expandedCardIdSet);
    });
    const ref = useRef<HTMLDivElement>(null);
    useEffect(() => {
        const element = ref.current;
        if (element) {
            return updateListener(element);
        }
        return (): void => {};
    }, []);

    useEffect(() => {
        const mainContainer = document.getElementById('main-container');
        mainContainer?.addEventListener('mouseup', handleMouseUp);
        mainContainer?.addEventListener('mouseleave', handleMouseLeave);
        return () => {
            mainContainer?.removeEventListener('mouseup', handleMouseUp);
            mainContainer?.removeEventListener('mouseleave', handleMouseLeave);
        };
    }, []);

    // 辅助函数
    const isNearTop = (e: React.MouseEvent, scrollElement: HTMLElement): boolean => {
        return e.clientY <= 150 && scrollElement.scrollTop > 0;
    };

    const isNearBottom = (e: React.MouseEvent, scrollElement: HTMLElement): boolean => {
        return e.clientY >= scrollElement.clientHeight - 100 &&
                scrollElement.scrollTop < scrollElement.scrollHeight - window.innerHeight;
    };

    const updateStartPoint = (e: React.MouseEvent): void => {
        startAbsPointY.current.y = e.clientY;
    };

    const handleMouseUp = (): void => {
        mergedSelected.clear();
        setIsSelecting(false);
        for (const [key, value] of unitsRefs.current) {
            if (value === null) {
                unitsRefs.current.delete(key);
            }
        }
        clearAutoScrollIntervals();
        moveDirectionHasChange = false;
    };
    const handleMouseDown = (e: React.MouseEvent): void => {
        // 右键不取消状态
        if (e.button === 2) {
            return;
        }
        scrollDirection = '';
        // 左键单击后记录起始坐标点，清空已选中区域，清除其他判断状态
        setMovementChangeY(e.clientY);
        setSelectedUnitKeys(new Set());
        clearAutoScrollIntervals();
        moveDirectionHasChange = false;
        // 禁止在Unit Info区域触发框选
        if (scrollElement) {
            const currentScrollTop = scrollElement.scrollTop;
            if (e.clientX - PAGE_PADDING > laneInfoWidth) {
                setIsSelecting(true);
                startPoint.current = { x: e.clientX, y: Math.abs(currentScrollTop + e.clientY) };
                startAbsPointY.current = { y: e.clientY };
            }
        }
        // 使得点击可以选中第一个节点而不受session中值为空的影响
        const firstSelected: Set<string> = new Set();
        for (const [key, value] of unitsRefs.current) {
            if (!value) continue;
            const rect = value.getBoundingClientRect();
            if (rect.top < e.clientY && e.clientY < rect.bottom) {
                firstSelected.add(key);
            }
            setSelectedUnitKeys(firstSelected);
        }
    };

    const handleMouseLeave = (): void => {
        mergedSelected.clear();
        setIsSelecting(false);
        // 清除所有定时器
        clearAutoScrollIntervals();
    };

    const handleMouseMove = (e: React.MouseEvent): void => {
        // 获取滚动元素
        if (!scrollElement) return;
        if (!isSelecting) return;
        // 有时候会出现触发了移动，但是Y值相同的情况，这种情况要忽略掉，否则会影响下方else逻辑
        if (movementChangeY === e.clientY) {
            return;
        }
        // 初始化方向
        if (scrollDirection === '') {
            handleDirectionInitialization(e);
            return;
        }
        // 用来判断用户选中一个元素向上之后再向下 反复操作的情况
        if (Math.abs(startPoint.current.y - (scrollElement.scrollTop + e.clientY)) < 10) {
            scrollDirection = '';
            moveDirectionHasChange = false;
            setMovementChangeY(e.clientY);
            startAbsPointY.current.y = e.clientY;
            return;
        }

        // 处理滑动方向变化逻辑
        if ((scrollDirection === DOWN && movementChangeY > e.clientY) || (scrollDirection !== DOWN && movementChangeY < e.clientY)) {
            // 确定新的滑动方向
            const newDirection = scrollDirection === DOWN ? UP : DOWN;
            scrollDirection = newDirection;
            clearAutoScrollIntervals();
            setMovementChangeY(e.clientY);
            moveDirectionHasChange = !moveDirectionHasChange;
            return;
        }

        // 更新当前 Y 位置（统一处理）
        setMovementChangeY(e.clientY);

        // 检查选择范围是否锁定
        if (session.selectedRangeIsLock) return;

        // 处理自动滚动
        handleAutoScrollLogic(e);

        // 更新选中元素（算子模式下不更新）
        if (!checkIsSliceMode(session)) {
            updateSelectedElements(e);
        }
    };

    // 初始化方向
    const handleDirectionInitialization = (e: React.MouseEvent): void => {
        if (movementChangeY < e.clientY) {
            scrollDirection = DOWN;
        } else {
            scrollDirection = UP;
        }
    };

    // 更新选中元素
    const updateSelectedElements = (e: React.MouseEvent): void => {
        if (startAbsPointY.current.y <= e.clientY) {
            updateSelectedUnits(e, DOWN);
        } else {
            updateSelectedUnits(e, UP);
        }
    };

    // 处理自动滚动逻辑
    const handleAutoScrollLogic = (e: React.MouseEvent): void => {
        setLastMouseMoveTime(new Date());
        const currentTime = new Date().getTime();
        const mouseStopped = currentTime - lastMouseMoveTime.getTime() > MOUSE_STOPPED_THRESHOLD;

        if (mouseStopped && scrollElement) {
            if (!moveDirectionHasChange) {
                handleAutoScroll(e, scrollElement);
            } else if (scrollDirection === UP && isNearTop(e, scrollElement)) {
                updateStartPoint(e);
                handleAutoScroll(e, scrollElement);
            } else if (scrollDirection === DOWN && isNearBottom(e, scrollElement)) {
                updateStartPoint(e);
                handleAutoScroll(e, scrollElement);
            }
        }
    };

    // 清除自动滚动定时器
    const clearAutoScrollIntervals = (): void => {
        autoScrollIntervals.current.forEach(clearInterval);
        autoScrollIntervals.current = [];
    };

    // 处理自动滚动逻辑
    const handleAutoScroll = (e: React.MouseEvent, scrollElement: HTMLElement): void => {
        const contentHeight = scrollElement.scrollHeight;
        const currentScrollTop = scrollElement.scrollTop;
        clearAutoScrollIntervals();
        // 存在点击后未选中数据情况，做返回处理
        if (selectedUnitKeys.size === 0) {
            return;
        }
        // 自动向下滚动
        if (scrollDirection === DOWN && isNearBottom(e, scrollElement)) {
            const interval = setInterval(() => {
                scrollElement.scrollBy(0, 20);
                if (!checkIsSliceMode(session)) {
                    updateSelectedUnits(e, DOWN);
                }
                if (currentScrollTop >= contentHeight - window.innerHeight) {
                    clearInterval(interval);
                }
            }, 50);
            autoScrollIntervals.current.push(interval);
        }

        // 自动向上滚动
        if (scrollDirection === UP && isNearTop(e, scrollElement)) {
            const interval = setInterval(() => {
                scrollElement.scrollBy(0, -20);
                if (!checkIsSliceMode(session)) {
                    updateSelectedUnits(e, UP);
                }
                if (scrollElement.scrollTop <= 0) {
                    clearInterval(interval);
                }
            }, 50);
            autoScrollIntervals.current.push(interval);
        }
    };

    // 更新选中元素
    const updateSelectedUnits = (e: React.MouseEvent, moveDirection: string): void => {
        const newSelected: Set<string> = new Set();
        const newDeleted: Set<string> = new Set();
        // 遍历所有单元，判断是否在选区范围内
        for (const [key, value] of unitsRefs.current) {
            if (!value) continue;
            const rect = value.getBoundingClientRect();
            if (moveDirectionHasChange) {
                // 滚动方向改变，删除后续选中
                const condition = scrollDirection === UP
                    ? e.clientY < rect.top
                    : rect.bottom < e.clientY;
                if (condition) {
                    newDeleted.add(key);
                }
            } else {
                // 正常添加选中
                const condition = moveDirection === UP
                    ? (rect.top < startAbsPointY.current.y && e.clientY < rect.bottom)
                    : (rect.top < e.clientY && startAbsPointY.current.y < rect.bottom);
                if (condition) {
                    newSelected.add(key);
                }
            }
        }
        // 获取当前的选中集合
        const currentSelected = new Set(selectedUnitKeys);
        currentSelected.forEach(key => mergedSelected.add(key));
        // 如果是删除模式，则从组中删除，反之为添加模式，不应把自己初始的选项删除，限制条件
        if (moveDirectionHasChange && mergedSelected.size !== 1) {
            newDeleted.forEach(key => {
                if (mergedSelected.has(key)) {
                    mergedSelected.delete(key);
                }
            });
        } else {
            newSelected.forEach((key: string) => {
                if (!mergedSelected.has(key)) {
                    mergedSelected.add(key);
                }
            });
        }
        setSelectedUnitKeys(mergedSelected);
    };

    return <div ref={ref} style={{ display: 'flex', flexDirection: 'column', height: totalHeight }} className="laneView"
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
    >
        <div className={INVISIBLE_UNITS_PLACEHOLDER} style={{ height: headOffset }} />
        {flattenUnits.filter((_, i) => first <= i && i < last).map((unit) => {
            if (!isPinned(unit) || unit.pinType === 'copied') {
                const unitKey = getAutoKey(unit);
                return <Unit
                    ref={(el): void => {
                        unitsRefs.current.set(unitKey, el);
                    }}
                    key={unitKey}
                    laneInfoWidth={laneInfoWidth}
                    unit={unit}
                    session={session}
                    hasPinButton={hasPinButton}
                    hasExpandIcon={true}
                    isVisible={true}
                    isPinned={isPinned(unit)}
                    isSonPinned={isSonPinned(unit)}
                    isSelecting={isSelecting}
                    inRangeUnitKeys={selectedUnitKeys}
                />;
            } else {
                return false;
            }
        },
        )}
        <div className={INVISIBLE_UNITS_PLACEHOLDER} style={{ height: tailOffset }} />
    </div>;
});

const TableScroller = styled.div`
    flex-grow: 1;
    overflow-y: scroll;
    overflow-x: hidden;
    border-top: solid 1px ${(props): string => props.theme.borderColor};
    user-select: none;
`;

interface ScrollerProps {
    id: string;
    children: JSX.Element | null;
    session: Session;
    eventType: string;
    orderOptions: OrderOptions;
    unitsArea: InsightUnit[];
    supportJump: boolean;
    shouldUpdateScrollTop?: boolean;
}

export const Scroller = React.forwardRef(({
    id, session, children, eventType, orderOptions: sorderOptions, unitsArea, supportJump, shouldUpdateScrollTop,
}: ScrollerProps, ref: React.ForwardedRef<HTMLDivElement>): JSX.Element => {
    // 广播滚动事件
    function scroll(e: React.UIEvent<HTMLDivElement>): void {
        const scrollTop = e.currentTarget.scrollTop;
        eventBus.emit(eventType, scrollTop);
        if (shouldUpdateScrollTop !== undefined && shouldUpdateScrollTop) {
            // 修改session.scrollTop
            runInAction(() => {
                session.scrollTop = scrollTop;
            });
        }
    }

    function mouseEnter(): void {
        runInAction(() => {
            session.scrollArea = eventType === EventType.PINNEDUNITWRAPPERSCROLL ? 'pinned' : 'unpinned';
        });
    }

    function mouseLeave(): void {
        runInAction(() => {
            session.scrollArea = '';
        });
    }

    // 切换session滚动到之前记录的地方
    React.useEffect(() => {
        (ref as React.MutableRefObject<HTMLDivElement | null>).current?.scrollTo(0, session.scrollTop);
    }, [session]);

    // 跳转到指定泳道
    useJumpTarget(session, unitsArea, supportJump, sorderOptions, (ref as React.MutableRefObject<HTMLDivElement | null>).current);

    return <TableScroller id={id} className={`laneWrapper ${eventType === EventType.PINNEDUNITWRAPPERSCROLL ? 'pinnedScrollArea' : ''}`}
        onScroll={scroll} ref={ref} onMouseEnter={mouseEnter} onMouseLeave={mouseLeave}>
        {children}
    </TableScroller>;
});
Scroller.displayName = 'insight-scroller';

const handleWheel = (event: WheelEvent): void => {
    if (event.ctrlKey) {
        event.preventDefault();
    }
};

const INVISIBLE_UNITS_PLACEHOLDER = 'invisible-units-placeholder';

const Units = ({ session, height, hasPinButton, laneInfoWidth }:
{ session: Session; height: number; hasPinButton: boolean; laneInfoWidth: number }, ref: React.ForwardedRef<HTMLDivElement>): JSX.Element => {
    return <Scroller id={UNIT_WRAPPER_SCROLLER_ID} session={session} unitsArea={session.units} supportJump={true}
        ref={ref} orderOptions={orderOptions} eventType={EventType.UNITWRAPPERSCROLL} shouldUpdateScrollTop>
        <FlattenUnits
            session={session}
            height={height}
            laneInfoWidth={laneInfoWidth}
            hasPinButton={hasPinButton}
            eventType={EventType.UNITWRAPPERSCROLL} />
    </Scroller>;
};

export const RefUnits = React.forwardRef(Units);
RefUnits.displayName = 'Units';
