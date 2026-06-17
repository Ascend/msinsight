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
import { observable, observe } from 'mobx';
import * as d3 from 'd3';
import { getTimeOffset } from '../../../insight/units/utils';
import { Session } from '../../../entity/session';
import { checkLineIsVisible, getHeight, processIsCol, UNDRAW_HEIGHT } from './draw';
import { DataBlock, FlowEvent } from '../../FilterLinkLine';
import { handlerEmptyString } from '../../../utils/string';
import { InsightUnit } from '../../../entity/insight';

function generateCalculateWHWithCache(): {
    calculateWHWithCacheFunc: Function;
    disposer: () => void; // 停止监听函数
} {
    const WIDTH_DEPENDENCIES = observable({
        canvasWith: 0,
        domainStart: 0,
        domainEnd: 0,
    });
    const WIDTH_CACHE: Map<string, number> = new Map();
    const HEIGHT_CACHE: Map<string, number | undefined> = new Map();
    const disposer = observe(WIDTH_DEPENDENCIES, () => {
        WIDTH_CACHE.clear(); // 根据宽度变化更新宽度缓存
    });
    function getWidthWithCache(paramsOfCache: { timestamp: number; cardId: string; pid: string; getTimeOffset?: Record<string, number>},
        li: d3.ScaleLinear<number, number>, session: Session, units: InsightUnit[]): number {
        const key = `${paramsOfCache.timestamp}-${paramsOfCache.cardId}-${paramsOfCache.pid}`;
        if (WIDTH_CACHE.has(key)) {
            return WIDTH_CACHE.get(key) ?? 0;
        }
        const width = li(paramsOfCache.timestamp - getTimeOffset(session, { cardId: paramsOfCache.cardId, processId: paramsOfCache.pid }, units, paramsOfCache?.getTimeOffset));
        WIDTH_CACHE.set(key, width);
        return width;
    }

    function generateDataBlockKey(block: DataBlock): string {
        return `${block.pid}-${block.tid}-${block.depth}`;
    }

    function getHeightWithCache(block: DataBlock, cardId: string, category: string, session: Session): number | undefined {
        const key = `${generateDataBlockKey(block)}-${cardId}`;
        if (HEIGHT_CACHE.has(key)) {
            return HEIGHT_CACHE.get(key);
        }
        const height = getHeight(session, block, cardId, category);
        HEIGHT_CACHE.set(key, height);
        return height;
    }

    return {
        calculateWHWithCacheFunc: (canvasWith: number, domainStart: number, domainEnd: number) => {
            HEIGHT_CACHE.clear(); // 每次计算重置高度缓存
            WIDTH_DEPENDENCIES.canvasWith = canvasWith;
            WIDTH_DEPENDENCIES.domainStart = domainStart;
            WIDTH_DEPENDENCIES.domainEnd = domainEnd;
            return { getWidthWithCache, getHeightWithCache };
        },
        disposer,
    };
}

const { calculateWHWithCacheFunc } = generateCalculateWHWithCache();

export interface LinkLineData {
    category: string;
    targetX: number;
    targetY: number;
    sourceX: number;
    sourceY: number;
    targetPos: Array<[x: number, y: number]>;
    offset: number;
    arrowWidth?: number;
}

export interface CalculateArgOption {
    rawList: Array<Record<string, unknown>>;
    session: Session;
    ctx: CanvasRenderingContext2D;
    category: string;
}

interface LineOption {
    width: number;
    height: number;
    targetX: number;
    targetY: number;
    sourceX?: number;
    sourceY?: number;
}

/**
 * 校验待绘制的连线是否在画布内
 * @param lineOption
 */
export function checkIsValidLine(lineOption: LineOption): boolean {
    const { sourceX = 0, sourceY = 0, targetX, targetY, width, height } = lineOption;
    return !((sourceX < 0 && targetX < 0) || (sourceX > width && targetX > width) || (sourceY < 0 && targetY < 0) || (sourceY > height && targetY > height));
}

/**
 * 校验待绘制的箭头是否在画布内
 * @param lineOption
 */
export function checkIsValidArrow(lineOption: LineOption): boolean {
    const { targetX, targetY, width, height } = lineOption;
    return targetX >= 0 && targetY >= 0 && targetX <= width && targetY <= height;
}

/**
 * 计算需要绘制的连线信息（画布位置、偏移量等）
 * @param rawList
 * @param session
 * @param ctx
 * @param category
 * @param units
 */
export function calculateLinkLines(rawList: Array<Record<string, unknown>>, session: Session, ctx: CanvasRenderingContext2D, category: string, units: InsightUnit[] = []): { [key: string]: LinkLineData[] } {
    const canvasWidth = ctx.canvas.clientWidth;
    const canvasHeight = ctx.canvas.clientHeight;
    const domainStart = session.domainRange.domainStart;
    const domainEnd = session.domainRange.domainEnd;
    const li = d3.scaleLinear().range([0, canvasWidth]).domain([domainStart, domainEnd]);
    const { getWidthWithCache, getHeightWithCache } = calculateWHWithCacheFunc(canvasWidth, domainStart, domainEnd);
    const timestampOffset = session.unitsConfig.offsetConfig.timestampOffset;

    const validLinesMap: { [key: string]: LinkLineData[] } = {};
    for (const data of rawList) {
        if (!checkLineIsVisible(data, category)) { continue; }
        const { category: cat, from, to, cardId } = data as unknown as FlowEvent;
        const [targetCardId, sourceCardId] = [handlerEmptyString(to.rankId ?? '', cardId), handlerEmptyString(from.rankId ?? '', cardId)];

        const [targetX, targetY] = [getWidthWithCache({ timestamp: to.timestamp, cardId: targetCardId, pid: to.pid }, li, session, units, timestampOffset),
            getHeightWithCache(to, targetCardId, cat, session)];
        const [sourceX, sourceY] = [getWidthWithCache({ timestamp: from.timestamp, cardId: sourceCardId, pid: from.pid }, li, session, units, timestampOffset),
            getHeightWithCache(from, sourceCardId, cat, session)];
        const targetPos: Array<[x: number, y: number]> = [[targetX, targetY]];
        const offset = ((targetX - sourceX) / 2);
        const isAllCol = (processIsCol.get(`${targetCardId}-${to.pid}`) ?? false) &&
            (processIsCol.get(`${sourceCardId}-${from.pid}`) ?? false);
        if (isAllCol || sourceY === undefined || targetY === undefined) { continue; }
        const isInUnit = !(sourceY < UNDRAW_HEIGHT && targetY < UNDRAW_HEIGHT);
        const isInside = checkIsValidLine({ targetX, targetY, sourceX, sourceY, width: canvasWidth, height: canvasHeight });
        // 跳过隐藏泳道或不在画布内的连线
        if (!isInUnit || !isInside) { continue; }
        if (!validLinesMap[targetY]) {
            validLinesMap[targetY] = [];
        }
        validLinesMap[targetY].push({ category: cat, targetX, targetY, sourceX, sourceY, targetPos, offset });
    }
    return validLinesMap;
}
