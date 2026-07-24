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
import type { Theme } from '@emotion/react';
import type { CustomCrossRenderer } from './custom';
import type { Session } from '../../../entity/session';
import { getTimeDifference } from './common';
import type { Pos, ExtendPos } from './common';
import type { InteractorMouseState, XReverseScaleRef } from './ChartInteractor';
import { THUMB_WIDTH_PX } from '../../base';
import { getTextParser } from '../TimelineAxis';
import { TIME_LINE_AXIS_HEIGHT_PX } from '../../ChartContainer/ChartContainer';
import type { DataBlock, FlowEvent } from '../../FilterLinkLine';
import { hashToNumber } from '../../../utils/colorUtils';
import { ChartDesc, UnitHeight } from '../../../entity/insight';
import type { InsightUnit } from '../../../entity/insight';
import type { ThreadMetaData } from '../../../entity/data';
import { buildCardIdIndex, type CardIdIndex, colorPalette } from '../../../insight/units/utils';
import { handlerEmptyString } from '../../../utils/string';
import { forEach, isNil } from 'lodash';
import { calculateLinkLines, LinkLineData, checkIsValidArrow } from './calculateLinkLines';
import { ThemeName } from '@insight/lib/theme';
import { getClassNameByMetadata } from '../../ChartContainer/Units/Units';
import type { ChartType } from '../../../entity/chart';

const UP_LINE: number = 30;
const DOWN_LINE: number = 45;
export const MIN_BRUSH_SIZE = 2;
export const PAGE_PADDING = 16;
const MAX_RECURSIVE_COUNT = 10;

interface DrawArrowOption {
    toX: number;
    toY: number;
    fromX: number;
    fromY: number;
    length: number;
    angle: number;
    color: string;
}

interface MaskRangeOption {
    session: Session;
    xReverseScaleRef: XReverseScaleRef;
    selectedRange?: [number, number];
    clickPos?: ExtendPos;
    mousePosNow?: Pos;
}

interface DrawLinesByLayerParams {
    ctx: CanvasRenderingContext2D;
    rawList: Array<Record<string, unknown>>;
    theme: Theme;
    session: Session;
    category: string;
    units: InsightUnit[];
    cardIdIndex: CardIdIndex;
}

function drawArrowPath(ctx: CanvasRenderingContext2D, option: Omit<DrawArrowOption, 'color'>): void {
    const { toX, toY, fromX, fromY, length, angle } = option;
    const a = Math.atan2((toY - fromY), (toX - fromX));
    const xC = toX - (length * Math.cos(a + (angle * Math.PI / 180)));
    const yC = toY - (length * Math.sin(a + (angle * Math.PI / 180)));
    const xD = toX - (length * Math.cos(a - (angle * Math.PI / 180)));
    const yD = toY - (length * Math.sin(a - (angle * Math.PI / 180)));
    ctx.moveTo(toX, toY);
    ctx.lineTo(xC, yC);
    ctx.lineTo(xD, yD);
    ctx.lineTo(toX, toY);
}

export function drawArrow(ctx: CanvasRenderingContext2D, option: DrawArrowOption): void {
    ctx.save();
    ctx.fillStyle = option.color;
    ctx.beginPath();
    drawArrowPath(ctx, option);
    ctx.fill();
    ctx.closePath();
    ctx.restore();
}

interface DrawTimeDiffArgs {
    ctx: CanvasRenderingContext2D | null;
    maskRange: number[];
    xScale: (posX: number) => number;
    isNsMode: boolean;
    theme: Theme;
    selectedRange?: [number, number];
}
const drawTimeDiff = (props: DrawTimeDiffArgs): void => {
    const {
        ctx, maskRange, xScale,
        isNsMode, theme, selectedRange,
    } = props;
    if (!ctx) {
        return;
    }
    ctx.textBaseline = 'top';
    ctx.textAlign = 'center';
    let timeString;
    if (selectedRange !== undefined) {
        timeString = getTimeDifference(Math.floor(selectedRange[0]), Math.floor(selectedRange[1]), isNsMode);
    } else {
        timeString = getTimeDifference(Math.floor(xScale(maskRange[0])), Math.floor(xScale(maskRange[1])), isNsMode);
    }
    drawTimeDiffText(ctx, timeString, maskRange, theme, isNsMode);
    // left
    drawArrow(ctx, {
        toX: maskRange[0],
        toY: (UP_LINE + DOWN_LINE) / 2,
        fromX: maskRange[0] + 1,
        fromY: (UP_LINE + DOWN_LINE) / 2,
        length: 10,
        angle: 30,
        color: theme.selectedChartColor,
    });

    // right
    drawArrow(ctx, {
        toX: maskRange[1],
        toY: (UP_LINE + DOWN_LINE) / 2,
        fromX: maskRange[1] - 1,
        fromY: (UP_LINE + DOWN_LINE) / 2,
        length: 10,
        angle: 30,
        color: theme.selectedChartColor,
    });
};

// hover时的蓝色圆角方框以及时间字符串
const drawHoverTimeRect = (ctx: CanvasRenderingContext2D,
    width: number, mousePosNow: Pos, xScale: (posX: number) => number, { isNsMode, domainRange }: Session): void => {
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.font = '12px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif';
    const displayText = getTextParser(isNsMode)(Math.floor(xScale(mousePosNow.x)), [domainRange.domainStart, domainRange.domainEnd]);
    const textLength = ctx.measureText(displayText).width;
    const roundRectWidth = textLength + 10;
    const roundRectHeight = 16;
    ctx.fillStyle = '#3778ED';
    if (mousePosNow.x < 0 || mousePosNow.x > ctx.canvas.clientWidth - THUMB_WIDTH_PX) {
        return;
    }
    if (mousePosNow.x <= roundRectWidth / 2) {
        ctx.fillRect(0, 0, roundRectWidth, roundRectHeight);
        ctx.fillStyle = 'white';
        ctx.fillText(displayText, roundRectWidth / 2, 3);
        return;
    }

    if (mousePosNow.x >= width - (roundRectWidth / 2) && mousePosNow.x <= ctx.canvas.clientWidth - THUMB_WIDTH_PX) {
        ctx.fillRect(width - roundRectWidth, 0, roundRectWidth, roundRectHeight);
        ctx.fillStyle = 'white';
        ctx.fillText(displayText, width - (roundRectWidth / 2), 3);
        return;
    }
    ctx.fillRect(mousePosNow.x - (roundRectWidth / 2), 0, roundRectWidth, roundRectHeight);
    ctx.fillStyle = 'white';
    ctx.fillText(displayText, mousePosNow.x, 3);
};

export const drawTimeDiffText = (ctx: CanvasRenderingContext2D, timeString: string, maskRange: number[], theme: Theme, isNsMode: boolean): void => {
    ctx.font = '12px sans-serif';
    const textLength = ctx.measureText(timeString).width + 20;
    if (textLength >= Math.abs(maskRange[1] - maskRange[0])) {
        ctx.beginPath();
        ctx.moveTo(maskRange[0], (UP_LINE + DOWN_LINE) / 2);
        ctx.lineTo(maskRange[1], (UP_LINE + DOWN_LINE) / 2);
        ctx.strokeStyle = theme.timeDiffPictureColor;
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo((maskRange[1] + maskRange[0]) / 2, (UP_LINE + DOWN_LINE) / 2);
        ctx.lineTo(((maskRange[1] + maskRange[0]) / 2) - 5, DOWN_LINE);
        ctx.lineTo(((maskRange[1] + maskRange[0]) / 2) + 5, DOWN_LINE);
        ctx.fillStyle = theme.timeDiffBackgroundColor;
        ctx.fill();
        // back
        ctx.beginPath();
        ctx.rect((maskRange[1] + maskRange[0] - textLength) / 2, DOWN_LINE, textLength, DOWN_LINE - UP_LINE);
        ctx.fillStyle = theme.timeDiffBackgroundColor;
        ctx.fill();

        // text
        ctx.fillStyle = theme.fontColor;
        ctx.font = '12px sans-serif';
        ctx.fillText(timeString, (maskRange[1] + maskRange[0]) / 2, DOWN_LINE + 4);
    } else {
        // back
        ctx.beginPath();
        ctx.rect(maskRange[0], UP_LINE, maskRange[1] - maskRange[0], DOWN_LINE - UP_LINE);
        ctx.fillStyle = theme.timeDiffBackgroundColor;
        ctx.fill();
        // text
        ctx.fillStyle = theme.fontColor;
        ctx.font = '12px sans-serif';
        ctx.fillText(timeString, (maskRange[1] + maskRange[0]) / 2, ((UP_LINE + DOWN_LINE) / 2) - 5);
        // line
        ctx.beginPath();
        ctx.moveTo(maskRange[0], (UP_LINE + DOWN_LINE) / 2);
        ctx.lineTo(((maskRange[0] + maskRange[1]) / 2) - (textLength / 2), (UP_LINE + DOWN_LINE) / 2);
        ctx.moveTo(maskRange[1], (UP_LINE + DOWN_LINE) / 2);
        ctx.lineTo(((maskRange[0] + maskRange[1]) / 2) + (textLength / 2), (UP_LINE + DOWN_LINE) / 2);
        ctx.strokeStyle = theme.timeDiffPictureColor;
        ctx.stroke();
    }
};

const drawSelectedRange = (ctx: CanvasRenderingContext2D | null, selectedRange: Session['selectedRange'], xReverseScaleRef: XReverseScaleRef): void => {
    if (ctx !== null && selectedRange !== undefined) {
        ctx.beginPath();
        ctx.moveTo(xReverseScaleRef.current(selectedRange[0]), 0);
        ctx.setLineDash([4, 2]);
        ctx.strokeStyle = '#5291FF';
        ctx.lineTo(xReverseScaleRef.current(selectedRange[0]), 9999);
        ctx.stroke();
        ctx.setLineDash([]);

        ctx.beginPath();
        ctx.moveTo(xReverseScaleRef.current(selectedRange[1]), 0);
        ctx.setLineDash([4, 2]);
        ctx.strokeStyle = '#5291FF';
        ctx.lineTo(xReverseScaleRef.current(selectedRange[1]), 9999);
        ctx.stroke();
        ctx.setLineDash([]);
    }
};

const getParentNodeByClassName = (el: Element, parentClassName: string): Element | null => {
    if (!el) {
        return null;
    }
    const parent = el.parentElement;
    if (parent !== null) {
        if (parent.classList.contains(parentClassName)) {
            return parent;
        } else {
            return getParentNodeByClassName(parent, parentClassName);
        }
    }
    return parent;
};

function getMaskRange({ session, xReverseScaleRef, clickPos, mousePosNow, selectedRange }: MaskRangeOption): number[] | undefined {
    let maskRange: number[] | undefined;
    if (session.selectedRangeIsLock && session.lockRange !== undefined) {
        maskRange = [xReverseScaleRef.current(session.lockRange[0]), xReverseScaleRef.current(session.lockRange[1])];
    } else if (clickPos !== undefined && mousePosNow !== undefined && clickPos.x !== mousePosNow.x) {
        // 1st priority
        // is brushing
        maskRange = [xReverseScaleRef.current(clickPos.timeAxisX), mousePosNow.x];
    } else if (selectedRange !== undefined) {
        // 2nd priority
        // not brushing now but has selected range
        maskRange = [xReverseScaleRef.current(selectedRange[0]), xReverseScaleRef.current(selectedRange[1])];
    } else {
        maskRange = undefined;
    }
    return maskRange;
}

const drawMaskRange = ({
    ctx, width, height, xReverseScaleRef, xScale, interactorMouseState: {
        clickPos: { current: clickPos },
        lastPos: { current: mousePosNow },
    }, selectedRange, isNsMode, session, theme,
}: DrawArgs & { ctx: CanvasRenderingContext2D }): void => {
    const maskRange = getMaskRange({ session, selectedRange, xReverseScaleRef, clickPos, mousePosNow });
    if (!maskRange) {
        return;
    }
    maskRange.sort((a, b) => a - b);
    const elements = document.getElementsByClassName('chart-selected');
    const unitLength = session.selectedRangeIsLock ? session.lockUnitCount : session.selectedUnits.length;
    ctx.fillStyle = theme.mode === 'dark' ? 'rgba(0,0,0,0.4)' : 'rgba(0,0,0,0.3)';
    if (unitLength === 0) {
        ctx.fillRect(0, TIME_LINE_AXIS_HEIGHT_PX, maskRange[0], height);
        ctx.fillRect(maskRange[1], TIME_LINE_AXIS_HEIGHT_PX, width - maskRange[1], height);
        drawTimeDiff({ ctx, maskRange, xScale, isNsMode, theme, selectedRange });
        return;
    }
    ctx.fillRect(0, TIME_LINE_AXIS_HEIGHT_PX, width, height);
    if (elements.length === 0) {
        drawTimeDiff({ ctx, maskRange, xScale, isNsMode, theme, selectedRange });
        return;
    }
    const elementNodes = Array.of(...elements);
    if (!checkIsSliceMode(session)) {
        elementNodes.forEach(element => {
            const rect = element.getBoundingClientRect();
            const scrollContainer = getParentNodeByClassName(element, 'laneWrapper');
            const containerRect = scrollContainer?.getBoundingClientRect();
            const top = (containerRect ? Math.max(rect.top, containerRect.top) : rect.top) - PAGE_PADDING;
            const bottom = (containerRect ? Math.min(rect.bottom, containerRect.bottom) : rect.bottom) - PAGE_PADDING;
            if (bottom > top) {
                const maskRangeTemp = maskRange as number[];

                /**
                 * 改进点：纵向扩展清除范围
                 * 减去 0.5 或 1 像素，确保把泳道的 border 完全包进“洞”里
                 */
                const offset = 0.5;
                ctx.clearRect(
                    maskRangeTemp[0],
                    top - offset,
                    maskRangeTemp[1] - maskRangeTemp[0],
                    (bottom - top) + (offset * 2), // 高度相应增加，覆盖上下边缘
                );
            }
        });
    } else {
        const element = getTargetElement(session, elementNodes);
        if (!element) { return; }
        const elementRect = element.getBoundingClientRect();
        const startY = getSelectionStartPos(elementRect, session);
        const rectHeight = calcHeightOfSlice(session);
        if (startY > elementRect.bottom - PAGE_PADDING) { return; }
        const maskRangeTemp = maskRange as number[];
        ctx.clearRect(maskRangeTemp[0], startY, maskRangeTemp[1] - maskRangeTemp[0], rectHeight);
    }
    drawTimeDiff({ ctx, maskRange, xScale, isNsMode, theme, selectedRange });
};

/**
 * 校验是否为算子框选模式
 * @param session
 */
export function checkIsSliceMode(session: Session): boolean {
    const { active, targetUnit } = session.sliceSelection;
    return active && targetUnit?.name === 'Thread';
}

/**
 * 检验算子框选是否有效框选
 * @param session
 */
export function checkIsValidSlice(session: Session): boolean {
    return checkIsSliceMode(session) && session.sliceSelection.rangeOfLevels[1] > -1;
}

/**
 * units多选时，获取目标unit
 * @param session
 * @param elements
 */
function getTargetElement(session: Session, elements: Element[]): Element | undefined {
    if (!elements || elements.length === 0 || !session.sliceSelection.targetUnit) {
        return undefined;
    }
    return elements.find(ele => ele.classList.contains(getClassNameByMetadata(session.sliceSelection.targetUnit as InsightUnit)));
}

export function getElementRects(session: Session): Array<DOMRect | undefined> {
    const rects: Array<DOMRect | undefined> = [];
    const elements = document.getElementsByClassName('chart-selected');
    const element = getTargetElement(session, Array.of(...elements));
    if (!element) {
        return [];
    }
    const elementRect = element.getBoundingClientRect();
    const wrapper = getParentNodeByClassName(element, 'laneWrapper');
    const wrapperRect = wrapper?.getBoundingClientRect();
    rects.push(elementRect, wrapperRect);
    return rects;
}

/**
 * 计算算子框选选择的层级
 * @param posY 结束点Y坐标
 * @param session
 */
export function calcLevelsOfSlice(posY: number, session: Session): void {
    const [elementRect] = getElementRects(session);
    if (!elementRect) {
        return;
    }
    const top = elementRect.top - PAGE_PADDING;
    const bottom = elementRect.bottom - PAGE_PADDING;
    const { startPos } = session.sliceSelection;
    // 判断移动点是否位于起始点上方
    const lastPosIsUp = startPos[1] > posY;
    const startActualY = Math.max(!lastPosIsUp ? startPos[1] : posY, top);
    const endActualY = Math.min(lastPosIsUp ? startPos[1] : posY, bottom);
    if (startActualY > bottom || endActualY < top) {
        session.sliceSelection.rangeOfLevels = [0, -1];
        return;
    }
    const rowHeight = getUnitRowHeight(session);
    if (!rowHeight) {
        return;
    }
    // 最大深度
    const maxLevel = (bottom - top) / rowHeight - 1;
    const startLevel = Math.floor((startActualY - top) / rowHeight);
    const endLevel = Math.min(Math.floor((Math.min(bottom, endActualY) - top) / rowHeight), maxLevel);
    // 记录框选覆盖的层级
    session.sliceSelection.rangeOfLevels = [startLevel, endLevel];
}

/**
 * 获取展开/收起算子的高度
 * @param session
 */
function getUnitRowHeight(session: Session): number {
    const targetUnit = session.sliceSelection.targetUnit;
    const maxDepth = ((targetUnit?.chart as ChartDesc<ChartType>)?.config as any)?.maxDepth ?? 0;
    if (!maxDepth) {
        return 0;
    }
    return targetUnit?.isExpanded ? UnitHeight.STANDARD : UnitHeight.COLL / maxDepth;
}

/**
 * 计算框选的算子在画板的高度
 * @param session
 */
export function calcHeightOfSlice(session: Session): number {
    const { rangeOfLevels: [startLevel, endLevel] } = session.sliceSelection;
    const rowHeight = getUnitRowHeight(session);
    if (endLevel === -1 || endLevel === undefined || rowHeight === 0) {
        return 0;
    }
    return (endLevel - startLevel + 1) * rowHeight;
}

/**
 * 获取框选区域的top位置
 * @param elementRect
 * @param session
 */
function getSelectionStartPos(elementRect: DOMRect, session: Session): number {
    const { rangeOfLevels } = session.sliceSelection;
    const rowHeight = getUnitRowHeight(session);
    return elementRect.top - PAGE_PADDING + rowHeight * rangeOfLevels[0];
}

export interface DrawArgs {
    ctx: CanvasRenderingContext2D | null;
    width: number;
    height: number;
    xReverseScaleRef: XReverseScaleRef;
    xScale: (posX: number) => number;
    interactorMouseState: InteractorMouseState;
    selectedRange?: [number, number];
    isNsMode: boolean;
    session: Session;
    theme: Theme;
}
export const drawOnMove = ({ interactorMouseState, ...args }: DrawArgs): void => {
    const { ctx, session } = args;
    if (ctx === null) { return; }
    const { clickPos: { current: clickPos }, lastPos: { current: mousePosNow } } = interactorMouseState;
    if (mousePosNow !== undefined && clickPos !== undefined && Math.abs(mousePosNow.x - clickPos.x) < MIN_BRUSH_SIZE) {
        return;
    }
    if (session.mKeyRender) {
        return;
    }

    ctx.save();
    ctx.resetTransform();
    ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    ctx.restore();

    const { width, height, xReverseScaleRef, xScale, selectedRange } = args;
    if (checkIsSliceMode(session)) {
        drawRectOfSlice({ interactorMouseState, ...args });
        return;
    }
    // draw mask
    // 因为拖动结束时normal canvas也会绘制mask，避免绘制双层mask，这里限制只有在拖动过程中才hover canvas才绘制mask
    // session.selectedRangeIsLock 时 normal canvas 的 mask 不会删除，避免绘制双层 mask
    if (clickPos !== undefined && !session.selectedRangeIsLock) {
        drawMaskRange({ interactorMouseState, ...args, ctx });
        // should filter on data type
        drawSelectedRange(ctx, selectedRange, xReverseScaleRef);
    }

    // draw hoverline and timeaxis highlight
    // session.selectedRangeIsLock 时绘制基准线没有意义
    if (clickPos !== undefined && !session.selectedRangeIsLock) {
        const startX = xReverseScaleRef.current(clickPos.timeAxisX);
        ctx.strokeStyle = '#3778ED';
        ctx.beginPath();
        ctx.moveTo(startX, 0);
        ctx.lineTo(startX, height);
        ctx.stroke(); // 绘制基准连线
    }

    if (mousePosNow !== undefined) {
        // ---hover line---
        ctx.strokeStyle = '#3778ED';
        ctx.beginPath();
        ctx.moveTo(mousePosNow.x, 0);
        ctx.lineTo(mousePosNow.x, height);
        ctx.stroke(); // 绘制鼠标位置连线

        // timeRect & text
        drawHoverTimeRect(ctx, width, mousePosNow, xScale, session); // 绘制鼠标连线上方的时间方框
    }
};

const heightMap = new Map();
// 是否是线程缩略图
const threadIsCol: Map<string, boolean> = new Map();
// 是否是进程缩略图
export const processIsCol: Map<string, boolean> = new Map();
// 泳道是否已隐藏
const unitIsHidden: Set<string> = new Set();

const markUnitHidden = (unit: InsightUnit, metadata: ThreadMetaData): void => {
    if (isNil(metadata.cardId) || metadata.cardId === '') {
        return;
    }
    if (!unit.isUnitVisible) {
        let key = metadata.cardId;
        if (!isNil(metadata.processId) && metadata.processId !== '') {
            key += `_${metadata.processId}`;
        }
        if (!isNil(metadata.threadId) && metadata.threadId !== '') {
            key += `_${metadata.threadId}`;
        }
        unitIsHidden.add(key);
    }
};

const markUnitCollapsed = (metadata: ThreadMetaData): void => {
    if (metadata.threadIdList?.length) {
        for (const threadId of metadata.threadIdList) {
            threadIsCol.set(`${metadata.cardId}-${metadata.processId}-${threadId}`, true);
        }
    } else if (metadata.threadId !== undefined) {
        threadIsCol.set(`${metadata.cardId}-${metadata.processId}-${metadata.threadId}`, true);
    }
};

const recordUnitHeight = (unit: InsightUnit, height: number): void => {
    const metadata = unit.metadata as ThreadMetaData;

    if (metadata.cardId !== undefined) {
        heightMap.set(`${metadata.cardId}`, height);
    }
    if (metadata.processId !== undefined) {
        heightMap.set(`${metadata.cardId}-${metadata.processId}`, height);
    }

    // 此处记录 CANN 泳道所在高度，为了适配 MSTX 泳道导致 HostToDevice 连线不准的问题
    if (metadata.processName === 'CANN') {
        heightMap.set(`${metadata.cardId}-${metadata.processId}-CANN`, height);
    }

    if (metadata.threadId !== undefined && metadata.processId !== undefined) {
        if (metadata.threadIdList) {
            for (const threadId of metadata.threadIdList) {
                heightMap.set(`${metadata.cardId}-${metadata.processId}-${threadId}`, height);
            }
        } else {
            heightMap.set(`${metadata.cardId}-${metadata.processId}-${metadata.threadId}`, height);
        }

        if (unit.collapsible && !unit.isExpanded) {
            markUnitCollapsed(metadata);
        }
    }
};

const updateUnitHeight = (session: Session, pinnedAreaHeight: number): void => {
    const rootUnits = Array.from(new Set<InsightUnit>(session.units.flatMap(unit => unit.parent ?? unit)));

    const computeUnitHeight = (units: InsightUnit[], initialHeight: number, count: number = 1): number => {
        let height = initialHeight;

        for (const unit of units) {
            if (!unit.isDisplay || unit.isMerged || unit.isMultiDeviceHidden) {
                continue;
            }

            const metadata = unit.metadata as ThreadMetaData;

            recordUnitHeight(unit, height);

            if (unit.isUnitVisible) {
                height += unit.height() + 1;
            }

            if (unit.children && unit.isExpanded && count <= MAX_RECURSIVE_COUNT) {
                height = computeUnitHeight(unit.children, height, count + 1);
            }
            markUnitHidden(unit, metadata);
        }
        return height;
    };

    computeUnitHeight(rootUnits, pinnedAreaHeight);
};

export interface DrawCanvasArgs {
    ctx: CanvasRenderingContext2D | null;
    width: number;
    height: number;
    xReverseScaleRef: XReverseScaleRef;
    xScale: (posX: number) => number;
    interactorMouseState: InteractorMouseState;
    selectedRange?: [number, number];
    isNsMode: boolean;
    session: Session;
    customRenderers: CustomCrossRenderer[];
    theme: Theme;
}
export const draw = (props: DrawCanvasArgs): void => {
    const {
        ctx, width, height,
        xReverseScaleRef, xScale,
        interactorMouseState, selectedRange, isNsMode,
        session, theme,
    } = props;

    if (ctx === null) { return; }
    if (session.mKeyRender) {
        return;
    }
    // clear all
    ctx.clearRect(0, 0, width, height);
    if (checkIsSliceMode(session) && session.sliceSelection.rangeOfLevels[1] === -1) {
        return;
    }
    if (selectedRange !== undefined) {
        drawMaskRange({ ctx, width, height, interactorMouseState, xReverseScaleRef, xScale, selectedRange, session, isNsMode, theme });
        // should filter on data type
        drawSelectedRange(ctx, selectedRange, xReverseScaleRef);
    }

    updateDrawLines(ctx, session, theme);
};

export const hasLinkLinesToDraw = (session: Session): boolean => {
    if (session.drawLineMode === 'single') {
        return Object.values(session.singleLinkLine).some(list => (list?.length ?? 0) > 0);
    }
    return session.drawLineMode === 'all' && (session.linkLineCategories.length > 0 || session.ridLineType !== '');
};

const updateDrawLines = (ctx: CanvasRenderingContext2D, session: Session, theme: Theme): void => {
    if (!hasLinkLinesToDraw(session)) {
        return;
    }
    heightMap.clear();
    threadIsCol.clear();
    processIsCol.clear();
    unitIsHidden.clear();
    const pinnedScrollArea = document.querySelector('#main-container .topC');
    const pinnedAreaHeight = pinnedScrollArea?.clientHeight ?? 0;
    updateUnitHeight(session, pinnedAreaHeight);
    drawLinkLines(ctx, session, theme, pinnedAreaHeight);
};

/**
 *
 * @param props
 */
export const drawMEventMask = (props: DrawCanvasArgs): void => {
    const {
        ctx, width, height,
        xReverseScaleRef, xScale,
        selectedRange, isNsMode,
        session, theme,
    } = props;
    if (ctx === null) { return; }
    // clear all
    ctx.clearRect(0, 0, width, height);
    if (session.mKeyRender) {
        session.mMaskRange.sort((a, b) => a - b);
        ctx.fillStyle = 'rgba(0,0,0,0.3)';
        const start = xReverseScaleRef.current(session.mMaskRange[0]);
        const end = xReverseScaleRef.current(session.mMaskRange[1]);
        const maskRange = [start, end];
        ctx.fillRect(0, TIME_LINE_AXIS_HEIGHT_PX, maskRange[0], height);
        ctx.fillRect(maskRange[1], TIME_LINE_AXIS_HEIGHT_PX, width - maskRange[1], height);
        drawTimeDiff({ ctx, maskRange, xScale, isNsMode, theme, selectedRange });
        updateDrawLines(ctx, session, theme);
    }
};

export const UNDRAW_HEIGHT = 45 + 2; // 45 指时间轴+旗帜轴的高度之和，2 指 useDraggableContainerEx css 中的 border-top: ${(p): string => p.theme.dividerColor} 2px solid;
export const getHeight = (session: Session, data: DataBlock, cardId: string, category: string): number | undefined => {
    let height;
    const unitHeight = heightMap.get(`${cardId}-${data.pid}-${data.tid}`);
    let processHeight = heightMap.get(`${cardId}-${data.pid}`);
    // 卡折叠的情况
    if (unitHeight === undefined && processHeight === undefined) {
        return undefined;
    }
    // 进程折叠的情况
    if (unitHeight === undefined && processHeight !== undefined) {
        // 此处单独适配 MSTX 泳道导致 HostToDevice 连线不准的问题
        if (category === 'HostToDevice' && heightMap.has(`${cardId}-${data.pid}-CANN`)) {
            processHeight = heightMap.get(`${cardId}-${data.pid}-CANN`);
        }
        height = UNDRAW_HEIGHT + processHeight - session.scrollTop + (0.5 * UnitHeight.UPPER);
        if (!session.isFullDb) { // 非全量 db 情况，进程折叠时隐藏
            processIsCol.set(`${cardId}-${data.pid}`, true);
        }
        return height;
    }
    const isCol = threadIsCol.get(`${cardId}-${data.pid}-${data.tid}`);
    if (isCol) {
        // 缩略泳道连线位置
        height = UNDRAW_HEIGHT + unitHeight - session.scrollTop + (0.5 * UnitHeight.COLL);
    } else {
        // 展开泳道连线位置
        height = UNDRAW_HEIGHT + unitHeight - session.scrollTop + ((data.depth + 0.5) * UnitHeight.STANDARD);
    }
    return height;
};

function sourceOrTargetLinkUnitIsHidden({
    targetCardId,
    sourceCardId,
    to,
    from,
}: {
    targetCardId: string;
    sourceCardId: string;
    to: DataBlock;
    from: DataBlock;
}): boolean {
    return (
        unitIsHidden.has(targetCardId) ||
        unitIsHidden.has(`${targetCardId}_${to.pid}`) ||
        unitIsHidden.has(`${targetCardId}_${to.pid}_${to.tid}`) ||
        unitIsHidden.has(sourceCardId) ||
        unitIsHidden.has(`${sourceCardId}_${from.pid}`) ||
        unitIsHidden.has(`${sourceCardId}_${from.pid}_${from.tid}`)
    );
}

/**
 * 校验连线是否显示（隐藏泳道时）
 * @param data
 * @param checkedCategory
 */
export function checkLineIsVisible(data: Record<string, unknown>, checkedCategory: string): boolean {
    const { category, from, to, cardId } = data as unknown as FlowEvent;
    if (category !== checkedCategory) {
        return false;
    }
    const [targetCardId, sourceCardId] = [handlerEmptyString(to.rankId ?? '', cardId), handlerEmptyString(from.rankId ?? '', cardId)];
    return !sourceOrTargetLinkUnitIsHidden({ targetCardId, sourceCardId, to, from });
}

/**
 * 根据连线末端坐标升序绘制连线
 * @param ctx
 * @param rawList
 * @param theme
 * @param session
 * @param category
 */
function drawLinkLinesByLayer({ ctx, rawList, theme, session, category, units, cardIdIndex }: DrawLinesByLayerParams): void {
    const linkLineMap = calculateLinkLines(rawList, session, ctx, category, units, cardIdIndex);
    const sortedKeys = Object.keys(linkLineMap).sort((a, b) => Number(a) - Number(b));
    const strokeStyle = theme.colorPalette[colorPalette[hashToNumber(category, colorPalette.length)]];
    forEach(sortedKeys, (key) => batchDrawLinkLines(ctx, linkLineMap[key], strokeStyle, theme.selectedChartColor));
}

/**
 * 批量绘制连线
 * @param ctx
 * @param dataList
 * @param strokeStyle
 * @param fillStyle
 */
function batchDrawLinkLines(ctx: CanvasRenderingContext2D, dataList: LinkLineData[], strokeStyle: string, fillStyle: string): void {
    ctx.strokeStyle = strokeStyle;
    ctx.beginPath();
    const arrowOptions: Array<Omit<DrawArrowOption, 'color'>> = [];
    for (const { sourceX, sourceY, targetX, targetY, targetPos, offset } of dataList) {
        ctx.moveTo(sourceX, sourceY);
        ctx.bezierCurveTo(sourceX + offset, sourceY, targetX - offset, targetY, targetX, targetY);
        const len = targetPos.length;
        if (len === 0 || !checkIsValidArrow({ targetX, targetY, width: ctx.canvas.width, height: ctx.canvas.height })) {
            continue;
        }
        let [fromX, fromY] = targetPos.reduce(([prevX, prevY], [x, y]) => [prevX + x + offset, prevY + y], [0, 0]);
        fromX = fromX / len;
        fromY = fromY / len;
        const yLen = Math.abs(fromY - targetY);
        const xLen = Math.abs(fromX - targetX);
        if (xLen === 0) {
            continue;
        }
        arrowOptions.push({
            toX: targetX,
            toY: targetY,
            fromX: targetX - offset,
            fromY: targetY + (((fromY - targetY) * Math.sqrt(yLen) / xLen) + Math.abs(fromY - targetY)),
            length: 10,
            angle: 30,
        });
    }
    ctx.stroke();
    ctx.closePath();
    // draw arrow
    ctx.fillStyle = fillStyle;
    ctx.beginPath();
    for (const option of arrowOptions) {
        drawArrowPath(ctx, option);
    }
    ctx.fill();
    ctx.closePath();
}

const drawLinkLines = (ctx: CanvasRenderingContext2D, session: Session, theme: Theme, pinnedAreaHeight: number): void => {
    ctx.save();
    ctx.beginPath();
    const clipTop = pinnedAreaHeight + UNDRAW_HEIGHT;
    ctx.rect(-1, clipTop, ctx.canvas.width + 1, ctx.canvas.height + 1);
    ctx.clip();
    const units = session.units;
    const cardIdIndex = buildCardIdIndex(units);
    if ((session.linkLineCategories.length > 0 || session.ridLineType !== '') && session.drawLineMode === 'all') {
        const checkedCategories = [...session.linkLineCategories, session.ridLineType];
        for (const checkedCategory of checkedCategories) {
            const rawList = session.linkLines[checkedCategory] ?? [];
            drawLinkLinesByLayer({ ctx, rawList, theme, session, category: checkedCategory, units, cardIdIndex });
        }
    }
    if (session.drawLineMode === 'single') {
        forEach(session.singleLinkLine, (list: any, category: string): void => {
            drawLinkLinesByLayer({ ctx, rawList: list, theme, session, category, units, cardIdIndex });
        });
    }
    ctx.restore();
};

/**
 * 算子框选是时绘制框选区域
 * @param clickPos
 * @param mousePosNow
 * @param args
 */
export const drawRectOfSlice = ({ interactorMouseState: { clickPos: { current: clickPos }, lastPos: { current: mousePosNow } }, ...args }: DrawArgs): void => {
    const ctx = args.ctx;
    if (!clickPos || !mousePosNow || !ctx || args.session.selectedRangeIsLock) {
        return;
    }
    const [elementRect, wrapperRect] = getElementRects(args.session);
    if (!elementRect) {
        return;
    }
    const { x: startX, y: startY } = clickPos;
    const { x: endX, y: endY } = mousePosNow;
    const { width, height, xReverseScaleRef, session, selectedRange, xScale, isNsMode, theme } = args;
    ctx.clearRect(0, 0, width, height);
    const top = (wrapperRect ? Math.max(elementRect.top, wrapperRect.top) : elementRect.top) - PAGE_PADDING;
    const bottom = (wrapperRect ? Math.min(elementRect.bottom, wrapperRect.bottom) : elementRect.bottom) - PAGE_PADDING;
    const lastPosIsUp = endY < startY;
    const drawStartY = lastPosIsUp ? Math.min(bottom, startY) : Math.max(top, startY);
    const drawEndY = lastPosIsUp ? Math.max(top, endY) : Math.min(bottom, endY);
    // 滚动时光标坐标无变化，需单独校验是否超出界限
    const isOverWhenScroll = (drawStartY >= bottom && drawEndY > bottom) || (drawEndY <= top && drawEndY < top);
    if ((lastPosIsUp && startY < top) || (!lastPosIsUp && startY > bottom) || isOverWhenScroll) {
        return;
    }
    const maskRange = getMaskRange({ session, xReverseScaleRef, selectedRange, clickPos, mousePosNow }) ?? [];
    drawTimeDiff({ ctx, maskRange, xScale, isNsMode, theme, selectedRange });
    drawSliceSelectedLine(ctx, maskRange);
    const drawHeight = drawEndY - drawStartY;

    ctx.strokeStyle = '#5291FF';
    ctx.fillStyle = theme.mode === ThemeName.DARK ? 'rgba(255, 255, 255, 0.1)' : 'rgba(82, 145, 255, 0.1)';
    ctx.strokeRect(startX, drawStartY, endX - startX, drawHeight);
    ctx.fillRect(startX - 1, drawStartY - 1, endX - startX - 2, drawHeight - 2);
};

/**
 * 绘制算子框选时箭头两侧端线
 * @param ctx
 * @param maskRange
 */
function drawSliceSelectedLine(ctx: CanvasRenderingContext2D, maskRange: number[]): void {
    if (!ctx || !maskRange.length) {
        return;
    }
    const lineStyle = { color: '#5291FF', width: 2 };

    const drawLine = (x: number): void => {
        ctx.beginPath();
        ctx.moveTo(x, UP_LINE);
        ctx.strokeStyle = lineStyle.color;
        ctx.lineWidth = lineStyle.width;
        ctx.lineTo(x, DOWN_LINE);
        ctx.stroke();
    };

    drawLine(maskRange[0]);
    drawLine(maskRange[1]);
}
