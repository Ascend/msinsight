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
import { useEffect, useRef } from 'react';
import { computed, runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { preOrderPinnedFlatten } from '../../entity/common';
import type { TreeNode } from '../../entity/common';
import type { InsightUnit } from '../../entity/insight';
import styled from '@emotion/styled';
// support utils/types
import type { Session } from '../../entity/session';
import { getAutoKey } from '../../utils/dataAutoKey';
// same level infer
import { Unit, Scroller, computeVisibleUnitRange } from './Units';
import { isAncestorPinned, isPinned, isSonPinned } from './unitPin';
import { EventType, useEventBus } from '../../utils/eventBus';
import { PINNED_UNIT_WRAPPER_SCROLLER_ID } from './Units/Units';

// 拖拽时当鼠标距离相邻泳道边界达到阈值时触发相邻泳道移动
const UNIT_MOVE_THRESHOLD = 10;
const PinnedUnitsContainer = styled.div`
    z-index: 1;
    flex-grow: 1;
`;

const handleWheel = (event: WheelEvent): void => {
    if (event.ctrlKey) {
        event.preventDefault();
    }
};

const INVISIBLE_UNITS_PLACEHOLDER = 'invisible-units-placeholder';

const orderOptions = {
    preOrderFlatten: preOrderPinnedFlatten,
    options: {
        when: (unit: TreeNode<InsightUnit>): boolean => unit.isExpanded,
        bypass: (unit: TreeNode<InsightUnit>): boolean => unit.type === 'transparent',
        exclude: (unit: TreeNode<InsightUnit>): boolean => !unit.isDisplay || unit.isMultiDeviceHidden,
        excludeEx: (unit: TreeNode<InsightUnit>): boolean => isSonPinned(unit),
    },
};

const FlattenUnits = observer(({ session, height, laneInfoWidth, eventType }:
{ session: Session; laneInfoWidth: number; height: number; eventType: string }, ref: React.ForwardedRef<HTMLDivElement>): JSX.Element => {
    const [scrollTop, setScrollTop] = React.useState(0);
    const [nextUnitTop, setNextUnitTop] = React.useState(0);
    useEventBus(eventType, (value) => setScrollTop(value as number));
    const flattenUnits = computed(() => preOrderPinnedFlatten(session.pinnedUnits, 0, orderOptions.options)).get();

    const [first, last] = React.useMemo(
        () => computeVisibleUnitRange(flattenUnits, height, scrollTop),
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

    const wrapRef = useRef<HTMLDivElement>(null);
    const draggingUnitKeyRef = useRef<string>('');
    const isUnitBelowTopmostRef = useRef<boolean>(false);
    const unitBelowKeyRef = useRef<string>('');
    const unitsRefs = useRef(new Map<string, HTMLDivElement | null>());
    const [draggingUnitIndex, setDraggingUnitIndex] = React.useState<number>(flattenUnits.length);
    const getDraggingUnitIndexByKey = (key: string): void => {
        draggingUnitKeyRef.current = key;
        const index = flattenUnits.findIndex(item => key === getAutoKey(item));
        setDraggingUnitIndex(index !== -1 ? index : flattenUnits.length);
    };
    // 通过重设session.pinnedUnits中拖拽泳道位置来触发置顶泳道的重绘达到拖拽中泳道和相邻泳道换位的效果
    const switchUnitInSession = (dragKey: string, nextKey: string): void => {
        runInAction(() => {
            const pinnedUnits = session.pinnedUnits;
            const draggingIndex = pinnedUnits.findIndex(item => dragKey === getAutoKey(item));
            const nextDownIndex = pinnedUnits.findIndex(item => nextKey === getAutoKey(item));
            if (draggingIndex !== -1 && nextDownIndex !== -1) {
                const draggingUnit = pinnedUnits.splice(draggingIndex, 1);
                pinnedUnits.splice(nextDownIndex, 0, draggingUnit[0]);
            }
        });
    };
    const onMouseMove = (e: MouseEvent, isMoveDown: boolean): void => {
        for (const [key, value] of unitsRefs.current) {
            const rect = value?.getBoundingClientRect();
            if (!rect) {
                continue;
            }
            if (isMoveDown) {
                // rect.bottom > e.clientY保证泳道交换位置后不会再重复执行
                if (!(rect.bottom - e.clientY < UNIT_MOVE_THRESHOLD && rect.bottom > e.clientY)) {
                    continue;
                }
                const movingUnitIndex = flattenUnits.findIndex(item => key === getAutoKey(item));
                if (!isAncestorPinned(flattenUnits[movingUnitIndex])) {
                    unitBelowKeyRef.current = key;
                    isUnitBelowTopmostRef.current = true;
                }
                // 相邻泳道为最上层泳道，相邻泳道下方泳道也为最上层泳道，可交换
                // 相邻泳道为最上层泳道，相邻泳道下方泳道不为最上层泳道，说明相邻泳道展开，则直到移到第2个最上层泳道时说明移出展开区域，此时可交换整个展开泳道
                if (isUnitBelowTopmostRef.current &&
                    (movingUnitIndex === flattenUnits.length - 1 || !isAncestorPinned(flattenUnits[movingUnitIndex + 1]))) {
                    isUnitBelowTopmostRef.current = false;
                    switchUnitInSession(draggingUnitKeyRef.current, unitBelowKeyRef.current);
                    setDraggingUnitIndex(movingUnitIndex);
                }
                // 找到鼠标移入泳道即可退出此次循环
                break;
            } else {
                if (!(e.clientY - rect.top < UNIT_MOVE_THRESHOLD && e.clientY > rect.top)) {
                    continue;
                }
                const movingUnitIndex = flattenUnits.findIndex(item => key === getAutoKey(item));
                // 上方相邻泳道为根泳道，说明可以直接换位
                if (!isAncestorPinned(flattenUnits[movingUnitIndex])) {
                    switchUnitInSession(draggingUnitKeyRef.current, key);
                    setDraggingUnitIndex(movingUnitIndex - 1);
                }
                // 找到鼠标移入泳道即可退出此次循环
                break;
            }
        }
    };
    const getUnits = (): JSX.Element[] => {
        const units = [];
        for (let i = 0; i < flattenUnits.length; i++) {
            // 防止虚拟滚动造成可视范围内没有拖拽泳道
            if (getAutoKey(flattenUnits[i]) === draggingUnitKeyRef.current) {
                units.push(<Unit
                    key={getAutoKey(flattenUnits[i])}
                    laneInfoWidth={laneInfoWidth}
                    unit={flattenUnits[i]}
                    session={session}
                    hasPinButton={true}
                    hasExpandIcon={true}
                    isVisible={true}
                    isPinnedArea={true}
                    isPinned={isPinned(flattenUnits[i])}
                    isSonPinned={isSonPinned(flattenUnits[i])}
                    enableDrag={!isAncestorPinned(flattenUnits[i])}
                    getDraggingUnitIndexByKey={getDraggingUnitIndexByKey}
                    setNextUnitTop={setNextUnitTop}
                    onMouseMove={onMouseMove}
                />);
                continue;
            }
            if (first <= i && i <= last) {
                units.push(<Unit
                    ref={(el): void => {
                        unitsRefs.current.set(getAutoKey(flattenUnits[i]), el);
                    }}
                    key={getAutoKey(flattenUnits[i])}
                    laneInfoWidth={laneInfoWidth}
                    unit={flattenUnits[i]}
                    session={session}
                    hasPinButton={true}
                    hasExpandIcon={true}
                    isVisible={true}
                    isPinnedArea={true}
                    isPinned={isPinned(flattenUnits[i])}
                    isSonPinned={isSonPinned(flattenUnits[i])}
                    // 拖拽泳道下方泳道全部向下偏移拖拽泳道的高度，以保障拖拽泳道脱离文档流的整体布局不变
                    nextUnitTop={i > draggingUnitIndex ? nextUnitTop : 0}
                    enableDrag={!isAncestorPinned(flattenUnits[i])}
                    getDraggingUnitIndexByKey={getDraggingUnitIndexByKey}
                    setNextUnitTop={setNextUnitTop}
                    onMouseMove={onMouseMove}
                />);
            }
        }
        return units;
    };
    useEffect(() => {
        const element = wrapRef.current;
        if (element) {
            element.addEventListener('wheel', handleWheel, { passive: false });
            return () => { element.removeEventListener('wheel', handleWheel); };
        }
        return () => {};
    }, []);
    return <PinnedUnitsContainer ref={wrapRef} style={{ display: 'flex', flexDirection: 'column', height: totalHeight }}>
        <div className={INVISIBLE_UNITS_PLACEHOLDER} style={{ height: headOffset }} />
        {getUnits()}
        <div className={INVISIBLE_UNITS_PLACEHOLDER} style={{ height: tailOffset }} />
    </PinnedUnitsContainer>;
});

const PUnits = ({ session, height, laneInfoWidth }:
{ session: Session; height: number; laneInfoWidth: number }, ref: React.ForwardedRef<HTMLDivElement>): JSX.Element => {
    return <Scroller id={PINNED_UNIT_WRAPPER_SCROLLER_ID} session={session} unitsArea={session.pinnedUnits} supportJump={false}
        ref={ref} orderOptions={orderOptions} eventType={EventType.PINNEDUNITWRAPPERSCROLL}>
        <FlattenUnits
            session={session}
            height={height}
            laneInfoWidth={laneInfoWidth}
            eventType={EventType.PINNEDUNITWRAPPERSCROLL} />
    </Scroller>;
};

export const PinnedUnits = React.forwardRef(PUnits);
