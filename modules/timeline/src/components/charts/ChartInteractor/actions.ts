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

import { clamp, throttle } from 'lodash';
import { runInAction } from 'mobx';
import type React from 'react';
import type { Session } from '../../../entity/session';
import { traceStart } from '../../../utils/traceLogger';
import type { InteractorMouseState, InteractorParams, XReverseScaleRef } from './ChartInteractor';
import { INTERACTOR_WIDTH } from './ChartInteractor';
import { isOnSideline, SINGLE_DRAG_OFFSET } from './common';
import type { Pos } from './common';
import { draw, drawOnMove, calcLevelsOfSlice, MIN_BRUSH_SIZE, checkIsSliceMode } from './draw';
import type { DrawArgs, DrawCanvasArgs } from './draw';
import { changeRangeMarkerTimestamp } from '../../TimelineMarker';
import type { Theme } from '@emotion/react';
import { isMac } from '../../../utils/is';

const dragInitData = {
    isDragging: false,
    xPos: 0,
    domainStart: 0,
    domainEnd: 0,
};

let dragData = { ...dragInitData };
function resetDragInitData(): void {
    dragData = { ...dragInitData };
}

export const resetCanvasSize = (canvas: React.RefObject<HTMLCanvasElement>, rect: DOMRectReadOnly | null): void => {
    if (!canvas.current) { return; }
    canvas.current.width = (rect?.width ?? 0) * devicePixelRatio;
    canvas.current.height = (rect?.height ?? 0) * devicePixelRatio;
};

const updateSessionStatus = (e: MouseEvent, session: Session, newSelected: [number, number]): void => {
    runInAction(() => {
        if (e.altKey) {
            session.domainRange = { domainStart: newSelected[0], domainEnd: newSelected[1] };
        }
        session.selectedRange = newSelected;
        changeRangeMarkerTimestamp(session, newSelected);
        const selectedRange = session.selectedRange[1] - session.selectedRange[0];
        traceStart('selectBrushScope', {
            action: 'selectBrushScope',
            units: session.selectedUnits.map((unit) => unit?.name),
            selectedRange: session.isNsMode ? Math.ceil(selectedRange / 1e6) : selectedRange,
        });
    });
};

export const mouseUpAction = (interactorParams: InteractorParams, interactorMouseState: InteractorMouseState, e: MouseEvent): void => {
    const { normalCanvas: canvas, hoverCanvas, session, xReverseScaleRef, xScale, isNsMode, customRenderers, theme } = interactorParams;
    const clickPos = interactorMouseState.clickPos.current;
    const lastPos = interactorMouseState.lastPos.current;
    resetDragInitData();
    if (hoverCanvas.current) {
        hoverCanvas.current.style.pointerEvents = 'none';
    }
    if (session.endTimeAll === undefined) {
        return;
    }
    const isInValid = (clickPos === undefined || canvas.current === null || lastPos === undefined);
    if (isInValid) { return; }

    if (session.contextMenu.isVisible) {
        return;
    }

    if (checkIsSliceMode(session) && !session.selectedRangeIsLock) {
        calcLevelsOfSlice(lastPos.y, session);
    }

    const clickPosX = xReverseScaleRef.current(clickPos.timeAxisX);
    if (Math.abs(lastPos.x - clickPosX) >= MIN_BRUSH_SIZE) {
        const mouseRange: [number, number] = [clickPos.timeAxisX, xScale(lastPos.x)];
        const newSelected = mouseRange.sort((a, b) => a - b);

        if (newSelected[0] < session.endTimeAll && session.endTimeAll < newSelected[1]) { newSelected[1] = session.endTimeAll; }
        updateSessionStatus(e, session, newSelected);
    }

    interactorMouseState.clickPos.current = undefined;
    const drawArgs: DrawCanvasArgs = {
        ctx: canvas.current.getContext('2d'),
        width: canvas.current.clientWidth,
        height: canvas.current.clientHeight,
        xReverseScaleRef,
        xScale,
        interactorMouseState,
        selectedRange: session.selectedRange,
        isNsMode,
        session,
        customRenderers,
        theme,
    };
    draw(drawArgs);
};

interface GetDrawOnMoveArgs {
    canvas: React.RefObject<HTMLCanvasElement>;
    xReverseScaleRef: XReverseScaleRef;
    xScale: (pos: number) => number;
    session: Session;
    interactorMouseState: InteractorMouseState;
    theme: Theme;
};
const getDrawOnMoveArgs = ({
    canvas,
    session,
    ...props
}: GetDrawOnMoveArgs): DrawArgs => {
    if (!canvas.current) { throw Error('missed canvas'); }
    const ctx = canvas.current.getContext('2d');
    if (!ctx) {
        throw Error('Failed to get CanvasRenderingContext2D');
    }
    return {
        ctx,
        width: canvas.current.clientWidth,
        height: canvas.current.clientHeight,
        isNsMode: session.isNsMode,
        selectedRange: session.selectedRange,
        session,
        ...props,
    };
};

export const mouseLeaveAction = (interactorParams: InteractorParams, interactorMouseState: InteractorMouseState): void => {
    const { hoverCanvas: canvas, session, xReverseScaleRef, xScale, theme } = interactorParams;
    if (canvas.current === null || session.endTimeAll === undefined) { return; }
    // 算子模式下光标离开canvas区间不做任何处理
    if (checkIsSliceMode(session)) {
        return;
    }
    const clickPos = interactorMouseState.clickPos.current;
    const lastPos = interactorMouseState.lastPos.current;
    if (clickPos && lastPos) {
        // mouse leave when mouse down
        const x = lastPos.x < 0 ? 0 : lastPos.x;
        const mouseRange: [number, number] = [xScale(clickPos.x), xScale(x)];
        const newSelected = mouseRange.sort((a, b) => a - b);

        if (newSelected[0] < session.endTimeAll && session.endTimeAll < newSelected[1]) {
            newSelected[1] = session.endTimeAll;
        }

        if (Math.abs(x - clickPos.x) >= MIN_BRUSH_SIZE) {
            runInAction(() => { session.selectedRange = newSelected; });
        } else {
            runInAction(() => { session.selectedRange = undefined; });
        }
    }
    interactorMouseState.clickPos.current = undefined; interactorMouseState.lastPos.current = undefined;
    runInAction(() => { session.hoverMouseX = null; });
    drawOnMove(getDrawOnMoveArgs({ canvas, session, theme, xReverseScaleRef, xScale, interactorMouseState }));
};

export enum MouseDownActionResult {
    NO_MOUSEDOWN_REQUIRED = 0,
    NEED_DRAG_ONE_SIDE = 1,
    NO_NEED_TO_DRAG_ONE_SIDE = 2,
}

export enum MouseButton {
    LEFT = 0,
    MIDDLE = 1,
    RIGHT = 2,
}

const getOffsetTop = (ele: HTMLElement): number => {
    return (
        ele.offsetTop + (ele.offsetParent ? getOffsetTop(ele.offsetParent as HTMLElement) : 0)
    );
};

const isInSplitLineY = (offsetY: number, splitLineRef: React.RefObject<HTMLDivElement> | undefined): boolean => {
    if (splitLineRef?.current) {
        const splitLinePosTop = getOffsetTop(splitLineRef.current as HTMLElement);
        /**
         * offsetHeight：返回元素的高度，包含内边距（padding）和边框（border），但不包括外边距（margin）和滚动条。
         * clientHeight：返回元素的高度，包含内边距但不包括边框、外边距和滚动条。
         */
        const splitLineHeight = splitLineRef.current.offsetHeight ?? 0;
        const splitLinePosY = [splitLinePosTop, splitLinePosTop + splitLineHeight];
        if (offsetY >= splitLinePosY[0] && offsetY <= splitLinePosY[1]) {
            return true;
        }
    }
    return false;
};

// 点击放置框选标记按钮则屏蔽mouseDownAction，避免当前框选丢失
const shouldIgnoreRangeMarkerButton = (session: Session, lastPos: Pos, xReverseScale: XReverseScaleRef): boolean => {
    const rangeButtonCanvasHeight = 30;
    const offsetX = lastPos.x;

    if (session.selectedRange !== undefined && lastPos.y <= rangeButtonCanvasHeight) {
        const rangeMarkerButtonWidth = 18;
        const rangeEndTimestamp = session.selectedRange[0] > session.selectedRange[1] ? session.selectedRange[0] : session.selectedRange[1];
        const rangeEndOffsetX = xReverseScale.current(rangeEndTimestamp);
        if (offsetX >= rangeEndOffsetX - rangeMarkerButtonWidth && offsetX <= rangeEndOffsetX) {
            return true;
        }
    }

    return false;
};

const isSingleLine = (session: Session): boolean => {
    return Object.values(session.linkLines).some(linkLine =>
        linkLine?.some(item => Boolean(item.cat)),
    );
};

interface MouseActionParams {
    session: Session;
    xReverseScaleRef: XReverseScaleRef;
    interactorMouseState: InteractorMouseState;
    e: React.MouseEvent;
    splitLineRef: React.RefObject<HTMLDivElement>;
    interactorParams: InteractorParams;
}
// 泳道画布内的元素点击都会经过这个函数，根据该函数的返回结果确定是否有点击下去，是否点击在选中区间的边界
export const mouseDownAction = ({
    session, xReverseScaleRef, interactorMouseState, e, splitLineRef, interactorParams,
}: MouseActionParams): MouseDownActionResult => {
    const lastPos = interactorMouseState.lastPos.current;
    const isPressingKey = session.panMode || (isMac && e.metaKey) || (!isMac && e.ctrlKey);
    // 点击context menu选项时屏蔽mouseDownAction
    const contextMenuVisible = session.contextMenu.isVisible as boolean;
    const rightClickOrNoLastPos = session.endTimeAll === undefined || !lastPos || e.button === MouseButton.RIGHT;
    if (isPressingKey) {
        dragData = { isDragging: true, xPos: e.nativeEvent.x, domainStart: session.domainRange.domainStart, domainEnd: session.domainRange.domainEnd };
        return MouseDownActionResult.NO_MOUSEDOWN_REQUIRED;
    }

    if (rightClickOrNoLastPos || shouldIgnoreRangeMarkerButton(session, lastPos, xReverseScaleRef) || contextMenuVisible) {
        return MouseDownActionResult.NO_MOUSEDOWN_REQUIRED;
    }

    const offsetX = lastPos.x;
    const absoluteOffsetY = lastPos.absoluteY;
    if (offsetX > xReverseScaleRef.current(session.endTimeAll as number)) {
        runInAction(() => {
            if (isSingleLine(session)) {
                session.linkLines = {};
            }
            session.selectedRange = undefined;
            session.timelineMaker.oldMarkedRange = undefined;
        });
        return MouseDownActionResult.NO_MOUSEDOWN_REQUIRED;
    }
    if (isInSplitLineY(absoluteOffsetY, splitLineRef)) {
        return MouseDownActionResult.NO_MOUSEDOWN_REQUIRED;
    }
    let needDragOneSide = false;
    let timeAxisX;
    if (session.selectedRange !== undefined && isOnSideline(lastPos, session.selectedRange, xReverseScaleRef)) { // 已经有选中区间，且点击到区间的边界
        const isOnLeftSide = offsetX <= xReverseScaleRef.current(session.selectedRange[0]) + SINGLE_DRAG_OFFSET &&
            offsetX >= xReverseScaleRef.current(session.selectedRange[0]) - SINGLE_DRAG_OFFSET;
        timeAxisX = isOnLeftSide ? session.selectedRange[1] : session.selectedRange[0];
        needDragOneSide = true;
    } else {
        timeAxisX = interactorParams.xScale(offsetX);
        session.timelineMaker.oldMarkedRange = undefined;
    }
    interactorMouseState.clickPos.current = {
        x: xReverseScaleRef.current(timeAxisX),
        y: lastPos.y,
        absoluteX: lastPos.absoluteX,
        absoluteY: lastPos.absoluteY,
        timeAxisX,
    };
    session.sliceSelection.startPos = [xReverseScaleRef.current(timeAxisX), lastPos.y];
    runInAction(() => { session.selectedRange = undefined; });
    return needDragOneSide ? MouseDownActionResult.NEED_DRAG_ONE_SIDE : MouseDownActionResult.NO_NEED_TO_DRAG_ONE_SIDE;
};

export const mouseMoveAction = (interactorParams: InteractorParams, interactorMouseState: InteractorMouseState, e: React.MouseEvent): void => {
    const { hoverCanvas: canvas, session } = interactorParams;
    if (canvas.current === null) { return; }
    const canDrag = (session.panMode || (isMac && e.metaKey) || (!isMac && e.ctrlKey)) && dragData.isDragging;
    if (canDrag) {
        moveDomainByDragging(session, dragData.xPos - e.clientX, canvas.current?.clientWidth ?? INTERACTOR_WIDTH);
        canvas.current.style.cursor = 'grabbing';
        canvas.current.style.pointerEvents = 'initial';
        return;
    }
    handleMousePosChange(interactorParams, interactorMouseState);
};

export const handleMousePosChange = (interactorParams: InteractorParams, interactorMouseState: InteractorMouseState): void => {
    const { hoverCanvas: canvas, session, xReverseScaleRef, xScale, theme } = interactorParams;
    if (canvas.current === null) { return; }
    const lastPos = interactorMouseState.lastPos.current;
    if (isOnSideline(lastPos, session.selectedRange, xReverseScaleRef)) {
        document.querySelectorAll('canvas.drawCanvas').forEach(canvasItem => {
            (canvasItem as HTMLElement).style.cursor = 'e-resize';
        });
    } else {
        document.querySelectorAll('canvas.drawCanvas').forEach(canvasItem => {
            (canvasItem as HTMLElement).style.cursor = 'default';
        });
    }
    runInAction(() => {
        session.hoverMouseX = lastPos !== undefined ? lastPos.x : null;
    });
    drawOnMove(getDrawOnMoveArgs({ canvas, session, theme, xReverseScaleRef, xScale, interactorMouseState }));
};

const handleZoom = throttle((session: Session, accumulativeZoomRef: React.MutableRefObject<number>, zoomPoint: number | undefined): void => {
    runInAction(() => {
        session.zoom = { zoomCount: accumulativeZoomRef.current, zoomPoint };
    });
    accumulativeZoomRef.current = 0;
}, 50);

export const mouseWheelAction = (
    session: Session,
    accumulativeZoomRef: React.MutableRefObject<number>,
    zoomPoint: number | undefined,
    wheelEvent: { ctrlKey: boolean; deltaY: number},
): void => {
    if (wheelEvent.ctrlKey) {
        accumulativeZoomRef.current += Math.sign(wheelEvent.deltaY);
        handleZoom(session, accumulativeZoomRef, zoomPoint);
    }
};

const moveDomainByDragging = throttle((session: Session, offset: number, canvasWidth: number): void => {
    if (!dragData.isDragging || canvasWidth === 0) {
        return;
    }
    const { domainRange: { domainStart, domainEnd } } = session;
    const timeDuration = domainEnd - domainStart;
    const timeOffset = offset * timeDuration / canvasWidth;
    const newEnd = clamp(dragData.domainEnd + timeOffset, timeDuration, session.endTimeAll ?? session.domain.defaultDuration);
    runInAction(() => {
        session.domainRange = { domainStart: newEnd - timeDuration, domainEnd: newEnd };
    });
}, 100);
