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
import { clamp } from 'lodash';
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { ArrowDownIcon, ArrowUpIcon } from '../icon/Icon';
import { themeInstance } from '../theme';
import { disableIframePointerEvent, recoverIframePointerEvent } from '../utils';

interface CssProps {
    column: boolean;
    translateXY: number;
    draggableWH: string;
    dragDirection: DragDirection;
    minWH: number;
    padding: number | string;
}
export interface ViewProps {
    mainContainer: JSX.Element;
    draggableContainer?: JSX.Element;
    slot?: JSX.Element;
    id: string;
    padding?: number | string;
}

export enum DragDirection {
    TOP = 0,
    BOTTOM = 1,
    LEFT = 2,
    RIGHT = 3,
}

export interface DraggableContext {
    dragDirection: DragDirection;
    container: [number, number];
    isOpen: React.MutableRefObject<boolean>;
    minDragWh: number;
    draggable: React.RefObject<HTMLDivElement>;
    dragTranslate: number;
    setDragTranslate: React.Dispatch<React.SetStateAction<number>>;
    sizeMethod?: SizeMethod;
}

export enum SizeMethod {
    NUMBER = 'number',
    PERCENT = 'percent',
}

/**
 * @param dragDirection 拖动方向,DragDirection
 * @param draggableWH 可拖动容器默认宽/高
 * @param open 可拖动容器是否默认开启,可选
 */
interface DCProps {
    dragDirection: DragDirection;
    draggableWH: number;
    open?: boolean;
    minWH?: number;
    foldWH?: number;
    sizeMethod?: SizeMethod;
}

const MIN_HORIZONTAL_WH = 24;
const MIN_VERTICAL_WH = 36;

const ContainerBase = styled.div<CssProps>`
    display: flex;
    flex-grow: 1;
    overflow: hidden;
    width: 100%;

    .bottomC {
        background-color: ${(p): string => p.theme.contentBackgroundColor};
        svg + .buttonShow {
            position: absolute;
            .caret {
                position: absolute;
                cursor: pointer;
                top: 50%;
                right: 0;
                color: ${(p): string => p.theme.switchIconColor};
                svg {
                    width: 10px;
                }
            }
        }

        & > .dragContainer {
            height: 100%;
            width: 100%;
        }
    }
`;
const ContainerLeft = styled(ContainerBase)`
    flex-direction: row-reverse; // 应用 flex-direction: row-reverse; 并不会改变子元素或父容器相对于视口的位置。也就是说，如果弹性容器原本位于页面的某个位置（比如距离顶部50像素），那么即使你改变了内部子元素的排列顺序，这个容器及其内容相对于视口的位置保持不变。
    height: 100%;
    & > .topC {
        flex: 1;
        flex-flow: row;
        overflow: hidden;
    }
    & > .bottomC {
        position: relative;
        height: 100%;
        width: ${(p): string => p.draggableWH};
        border-right: ${(p): string => p.theme.bgColorDark} 2px solid;
        display: flex;
        & > .dragContainer {
            overflow: hidden;
        }
        & > .splitLine {
            position: absolute;
            height: 100%;
            width: 10px;
            right: 0;
            background-color: transparent;
            border-right: ${(p): string => p.theme.dividerColor} 0 solid;
            user-select: none;
            &:hover[aria-disabled=false] {
                border-right-width: 1px;
                cursor: e-resize;
            }
        }
        & > .buttonShow {
            position: absolute;
            cursor: pointer;
            top: 50%;
            z-index: 2;
            transform: rotate(${(p): number => p.translateXY === 0 ? 90 : 270}deg);
            right: -38px;
            svg polygon {
                fill: ${(p): string => p.translateXY === 0 ? p.theme.bgColorLight : p.theme.contentBackgroundColor} !important;
            }
        }
        & > .caret {
            position: absolute;
            cursor: pointer;
            transform: rotate(180deg);
            z-index: 2;
            top: 50%;
            right: 0;
            color: ${(p): string => p.theme.switchIconColor};
            svg {
                width: 10px;
            }
        }
        &.width0 {
            & > .buttonShow {
                right: -53px;
            }
        }
    }
`;
const ContainerRight = styled(ContainerBase)`
    flex-direction: row;
    height: 100%;
    & > .topC {
        flex: 1;
        flex-flow: row;
        overflow: hidden;
    }
    & > .bottomC {
        position: relative;
        height: 100%;
        width: ${(p): string => p.draggableWH};
        overflow: hidden;
        display: flex;
        & > .dragContainer {
            z-index: 1;
        }
        & > .dragContainer[aria-disabled=true] {
            border-left: ${(p): string => p.theme.dividerColor} 2px solid;
            padding-left: 15px;
        }
        & > .splitLine {
            position: absolute;
            height: 100%;
            width: 10px;
            left: 0;
            z-index: 1;
            background-color: transparent;
            border-left: ${(p): string => p.theme.dividerColor} 0 solid;
            user-select: none;
            &:hover[aria-disabled=false] {
                border-left-width: 3px;
                cursor: e-resize;
            }
        }
        & > .buttonShow {
            position: absolute;
            cursor: pointer;
            top: 50%;
            z-index: 2;
            transform: rotate(-90deg);
            left: -37px;
            opacity: 0.8;
        }
        & > .caret {
            position: absolute;
            cursor: pointer;
            z-index: 2;
            top: 50%;
            left: 2px;
            color: ${(p): string => p.theme.switchIconColor};
            svg {
                width: 10px;
            }
        }
    }
`;
const ContainerBottom = styled(ContainerBase)`
    flex-direction: column;
    & > .topC {
        width: 100%;
        flex: 1;
        flex-flow: row;
        overflow: hidden;
        padding: ${(p): number | string => p.padding ?? 0}px;
        user-select: none;
    }

    & > .bottomC {
        width: 100%;
        height: ${(p): string => p.draggableWH};
        position: relative;
        user-select: none;
        & > .splitLine {
            position: absolute;
            z-index: 3;
            height: 10px;
            width: 100%;
            top: 0;
            background-color: transparent;
            border-top: ${(p): string => p.theme.dividerColor} 0 solid;
            user-select: none;
            &:hover[aria-disabled=false] {
                border-top-width: 1px;
                cursor: n-resize;
            }
        }
        & > .buttonShow {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: calc(50% - 39px);
            z-index: 4;
            svg polygon {
                fill: ${(p): string => p.theme.bgColorLight} !important;
            }
            svg path {
                transform: rotate(${(p): number => p.translateXY === 0 ? 180 : 0}deg);
                transform-box: fill-box;
                transform-origin: center;
            }
        }

        & > .caret {
            position: absolute;
            cursor: pointer;
            z-index: 4;
            left: calc(50% - 6px);;
            top: -2px;
            transform: rotate(90deg);
            color: ${(p): string => p.theme.switchIconColor};

            svg {
                width: 10px;
            }
        }
    }
    & > .bottomC::before {
        width: 100%;
        top: -10px;
        left: 0;
    }

`;
const ContainerTop = styled(ContainerBase)`
    flex-direction: column-reverse;
    .topC {
      height: calc(100vh - ${(p): string | number => p.translateXY === 0 ? p.draggableWH : p.minWH}px);
      user-select: none;
    }
    .bottomC {
        user-select: none;
        top: 0;
        left: 0;
        right: 0;
        height: ${(p): string => p.draggableWH}px;
        .buttonShow {
            position: absolute;
            bottom: 0;
            left: calc(50% - 39px);
            z-index: 4;
        }
    }
`;

interface MovingState {
    stat: 'idle' | 'movable' | 'moved';
    startX: number;
    startY: number;
    screenX: number;
    screenY: number;
}
const getHandleMouseDown = (dragDirection: DragDirection, draggable: React.RefObject<HTMLDivElement>,
    movingState: React.MutableRefObject<MovingState>, isOpen: React.MutableRefObject<boolean>) => (e: MouseEvent): void => {
    const domDrag = draggable.current;
    if (!domDrag) { return; }
    disableIframePointerEvent();

    let offset; const baseMS: MovingState = { stat: 'movable', startX: 0, startY: 0, screenX: e.screenX, screenY: e.screenY };
    const domDragRect = domDrag.getBoundingClientRect();
    switch (dragDirection) {
        case DragDirection.TOP:
            offset = domDragRect.bottom - e.clientY;
            if (offset <= 8 && offset > 0 && isOpen.current) {
                movingState.current = {
                    ...baseMS,
                    startX: domDragRect.x,
                    startY: domDragRect.bottom,
                };
            }
            break;
        case DragDirection.BOTTOM:
            offset = e.clientY - domDragRect.top;
            if (offset <= 8 && offset > 0 && isOpen.current) {
                movingState.current = {
                    ...baseMS,
                    startX: domDragRect.x,
                    startY: domDragRect.top,
                };
            }
            break;
        case DragDirection.LEFT:
            offset = domDragRect.right - e.clientX;
            if (offset <= 8 && offset > 0 && isOpen.current) {
                movingState.current = {
                    ...baseMS,
                    startX: domDragRect.right,
                    startY: domDragRect.y,
                };
            }
            break;
        default:
            offset = e.clientX - domDragRect.left;
            if (offset <= 8 && offset > 0 && isOpen.current) {
                movingState.current = {
                    ...baseMS,
                    startX: domDragRect.left,
                    startY: domDragRect.y,
                };
            }
            break;
    }
};

const RIGHT_PERCENT = 0.99;

const handleMouseMove = (container: React.RefObject<HTMLDivElement>, draggable: React.RefObject<HTMLDivElement>,
    movingState: React.MutableRefObject<MovingState>, dragDirection: DragDirection, minDragWh: number) => (e: MouseEvent): void => {
    const dom = container.current;
    const domDrag = draggable.current;
    const moving = movingState.current;
    if (e.buttons !== 1) {
        moving.stat = 'idle';
        return;
    }
    if (!dom || !domDrag) { return; }
    if (moving.stat === 'idle') { return; }
    if (Math.abs(e.screenY - moving.screenY) < 2 && Math.abs(e.screenX - moving.screenX) < 2) { return; }
    let offsetY: number;
    let offsetX: number;
    const domRect = dom.getBoundingClientRect();
    switch (dragDirection) {
        case DragDirection.BOTTOM:
            offsetY = e.y - moving.startY;
            if (Math.abs(offsetY) >= 5) {
                domDrag.style.height = `${clamp(dom.clientHeight - e.y, minDragWh, dom.clientHeight - minDragWh)}px`;
            }
            break;
        case DragDirection.TOP:
            offsetY = e.y - moving.startY;
            if (Math.abs(offsetY) >= 5) {
                domDrag.style.height = `${clamp(e.y, minDragWh, dom.clientHeight - minDragWh)}px`;
            }
            break;
        case DragDirection.LEFT:
            offsetX = e.x - moving.startX;
            if (Math.abs(offsetX) >= 5) {
                domDrag.style.width = `${clamp(e.clientX - domRect.left, 245, dom.clientWidth - minDragWh)}px`;
            }
            break;
        default:
            offsetX = e.x - moving.startX;
            if (Math.abs(offsetX) >= 5) {
                domDrag.style.width = `${clamp(domRect.left + dom.clientWidth - e.clientX, minDragWh, dom.clientWidth * RIGHT_PERCENT)}px`;
            }
            break;
    }
    moving.stat = 'moved';
    e.preventDefault();
};

const handleMouseUp = ({ container, draggable, movingState, dragDirection, minDragWh, sizeMethod }: {
    container: React.RefObject<HTMLDivElement>;
    draggable: React.RefObject<HTMLDivElement>;
    movingState: React.MutableRefObject<MovingState>;
    dragDirection: DragDirection;
    minDragWh: number;
    sizeMethod?: SizeMethod;
}) => (e: MouseEvent): void => {
    recoverIframePointerEvent();
    const dom = container.current;
    const domDrag = draggable.current;
    const moving = movingState.current;
    const isDomInvalid = !dom || !domDrag || dom.clientHeight === 0 || dom.clientWidth === 0;
    if (moving.stat !== 'moved' || isDomInvalid) {
        moving.stat = 'idle';
        return;
    }
    const domRect = dom.getBoundingClientRect();
    let dragWHTmp: number;
    switch (dragDirection) {
        case DragDirection.TOP:
            dragWHTmp = clamp(e.y, minDragWh, dom.clientHeight - minDragWh);
            domDrag.style.height = sizeMethod === SizeMethod.NUMBER ? `${dragWHTmp}px` : `${dragWHTmp / dom.clientHeight * 100}%`;
            window.dispatchEvent(new Event('topResize'));
            break;
        case DragDirection.BOTTOM:
            dragWHTmp = clamp(dom.clientHeight - e.y, minDragWh, dom.clientHeight - minDragWh);
            domDrag.style.height = sizeMethod === SizeMethod.NUMBER ? `${dragWHTmp}px` : `${dragWHTmp / dom.clientHeight * 100}%`;
            window.dispatchEvent(new Event('bottomResize'));
            break;
        case DragDirection.LEFT:
            dragWHTmp = clamp(e.clientX - domRect.left, 245, dom.clientWidth - minDragWh);
            domDrag.style.width = sizeMethod === SizeMethod.NUMBER ? `${dragWHTmp}px` : `${dragWHTmp / dom.clientWidth * 100}%`;
            window.dispatchEvent(new Event('leftResize'));
            break;
        case DragDirection.RIGHT:
            dragWHTmp = clamp(domRect.left + dom.clientWidth - e.clientX, minDragWh, dom.clientWidth * RIGHT_PERCENT);
            domDrag.style.width = sizeMethod === SizeMethod.NUMBER ? `${dragWHTmp}px` : `${dragWHTmp / dom.clientWidth * 100}%`;
            window.dispatchEvent(new Event('rightResize'));
            break;
        default:
            break;
    }
    movingState.current = {
        stat: 'idle',
        startX: 0,
        startY: 0,
        screenY: 0,
        screenX: 0,
    };
    window.dispatchEvent(new Event('resize'));
};

// 主容器宽高未确定时,初始化为设定的px值,主容器挂载结束后,后续宽高设置为百分比
const pxConvert = (px: number, container: [number, number], dragDirection: DragDirection, sizeMethod?: SizeMethod): string => {
    let tempPx = px;
    if (container[0] === 0 || container[1] === 0 || sizeMethod === SizeMethod.NUMBER || tempPx <= 40) {
        return `${tempPx}px`;
    }
    if (dragDirection <= 1) {
        if (dragDirection === 1) { tempPx += 4; } // bottom面板需要加上分割线的宽度
        return `${tempPx / container[1] * 100}%`;
    } else {
        return `${tempPx / container[0] * 100}%`;
    }
};

const handleDraggableShow = (draggableProps: DraggableContext) => (): void => {
    const domDrag = draggableProps.draggable.current;
    if (!domDrag) { return; }
    if (draggableProps.dragDirection <= 1) {
        if (draggableProps.isOpen.current) { // open -> close
            draggableProps.setDragTranslate(domDrag.clientHeight);
            domDrag.style.height = `${draggableProps.minDragWh}px`;
        } else { // close -> open
            domDrag.style.height = `${pxConvert(draggableProps.dragTranslate, draggableProps.container, draggableProps.dragDirection, draggableProps.sizeMethod)}`;
            draggableProps.setDragTranslate(0);
        }
    } else {
        if (draggableProps.isOpen.current) { // open -> close
            draggableProps.setDragTranslate(domDrag.clientWidth);
            domDrag.style.width = `${draggableProps.minDragWh}px`;
            if (draggableProps.minDragWh === 0) {
                domDrag.classList.add('width0');
            }
        } else { // close -> open
            domDrag.style.width = `${pxConvert(draggableProps.dragTranslate, draggableProps.container, draggableProps.dragDirection, draggableProps.sizeMethod)}`;
            draggableProps.setDragTranslate(0);
            domDrag.classList.remove('width0');
        }
    }
    draggableProps.isOpen.current = !draggableProps.isOpen.current;
    window.dispatchEvent(new Event('resize'));
};

const containerMap: Map<DragDirection, typeof ContainerBase> = new Map([
    [DragDirection.TOP, ContainerTop],
    [DragDirection.BOTTOM, ContainerBottom],
    [DragDirection.LEFT, ContainerLeft],
    [DragDirection.RIGHT, ContainerRight],
]);

const getMinDragWidth = (dragDirection: DragDirection, foldWH?: number): number => {
    if (foldWH !== undefined && typeof foldWH === 'number') {
        return foldWH;
    }
    return dragDirection <= 1 ? MIN_VERTICAL_WH : MIN_HORIZONTAL_WH;
};

/**
 * 在当前布局下创建两个容器，其中draggable容器可拖动改变宽/高，可点击收起隐藏，main容器自适应改变，根据传入拖动方向不同，提供leftResize/rightResize等事件
 * @param props
 * @return [view, handleOpen]
 * view：可拖动布局构造函数；
 * handleOpen：显示/隐藏可拖动容器；
 */
export const useDraggableContainer = (props: DCProps): [((props: ViewProps) => JSX.Element), ((needOpen?: boolean) => void), () => void] => {
    const { draggableWH, dragDirection, foldWH, sizeMethod = SizeMethod.PERCENT, open = true } = props;
    const container = useRef<HTMLDivElement>(null);
    const draggable = useRef<HTMLDivElement>(null);
    const [dragWh, setDragWh] = useState(String(draggableWH));
    const [autoPopUp, setAutoPopUp] = useState(true);
    const [containerWH, setContainerWH] = useState([0, 0] as [number, number]);
    useEffect(() => { setDragWh(pxConvert(draggableWH, containerWH, dragDirection, sizeMethod)); }, [draggableWH, containerWH, dragDirection, sizeMethod]);
    const MIN_DRAG_WH = useMemo(() => getMinDragWidth(dragDirection, foldWH), [dragDirection, foldWH]);
    const [dragTranslate, setDragTranslate] = useState(open ? 0 : draggableWH); // 可拖动的距离范围。0 | 具体某个值
    const isOpen = useRef(dragTranslate === 0);
    useEffect(() => {
        const dom = container.current;
        if (dom) {
            setContainerWH([dom.clientWidth, dom.clientHeight]);
        }
    }, [isOpen.current, setContainerWH]);
    const movingState = useRef<MovingState>({ stat: 'idle', startX: 0, startY: 0, screenY: 0, screenX: 0 });
    const onMousedown = getHandleMouseDown(dragDirection, draggable, movingState, isOpen);
    const onMousemove = handleMouseMove(container, draggable, movingState, dragDirection, MIN_DRAG_WH);
    const onMouseup = handleMouseUp({ container, draggable, movingState, dragDirection, minDragWh: MIN_DRAG_WH, sizeMethod });
    const showDraggable = handleDraggableShow({
        dragDirection,
        container: containerWH,
        isOpen,
        minDragWh: MIN_DRAG_WH,
        draggable,
        dragTranslate,
        setDragTranslate,
        sizeMethod,
    });
    const handleOpen = (needOpen = false): void => { if ((needOpen && !isOpen.current) || autoPopUp) { showDraggable(); setAutoPopUp(false); } };
    const Container = containerMap.get(dragDirection) as typeof ContainerBase;
    // 分割线混乱问题解决
    useEffect(() => {
        const isDragTranslate = !isOpen.current && dragTranslate !== 0;
        if (isDragTranslate && draggable.current && dragDirection === DragDirection.RIGHT) {
            draggable.current.style.width = `${MIN_DRAG_WH}px`;
        }
    });
    const useRotatedDrawerButton = dragDirection === DragDirection.LEFT || dragDirection === DragDirection.BOTTOM;
    const DrawerButton = useRotatedDrawerButton
        ? ArrowDownIcon
        : dragTranslate ? ArrowUpIcon : ArrowDownIcon;
    const view = (viewProps: ViewProps): JSX.Element => {
        return <Container
            key={viewProps.id} ref={container} column translateXY={dragTranslate}
            draggableWH={open ? dragWh : pxConvert(MIN_DRAG_WH, containerWH, dragDirection, sizeMethod)}
            dragDirection={dragDirection} minWH={MIN_DRAG_WH}
            padding={viewProps.padding ?? 0}
            onMouseUp={(e): void => onMouseup(e.nativeEvent)} onMouseDown={(e): void => onMousedown(e.nativeEvent)}
            onMouseMove={(e): void => onMousemove(e.nativeEvent)}>
            <div className={'topC'}> {viewProps.mainContainer} </div>
            <div className={'bottomC'} ref={draggable}>
                <div className={'dragContainer'} aria-disabled={dragTranslate !== 0}>{viewProps.draggableContainer}</div>
                <DrawerButton
                    data-testid={'drawer-btn'}
                    className={'buttonShow'}
                    onClick={(): void => showDraggable()}
                    theme={themeInstance.getCurrentTheme()}
                />
                <div className={'splitLine'} aria-disabled={dragTranslate !== 0} />
            </div>
            {viewProps.slot}
        </Container>;
    };
    return [view, handleOpen, showDraggable];
};
