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
import { clamp } from 'lodash';
import { makeAutoObservable } from 'mobx';
import type { TimeStamp } from './common';

export const GOLDEN_RATE = 0.1236;
export const PAN_RATE = 0.05; // 平移倍率
export const MAX_INITIAL_DURATION = 5 * 60 * 1e9; // 最大初始时间范围 5分钟
export const MAX_ZOOM_DURATION = 30 * 60 * 1e9; // 最大缩放时间范围 30分钟

export interface DomainRange {
    domainStart: TimeStamp;
    domainEnd: TimeStamp;
}

const validNumber = (num: number | undefined): number | undefined => {
    if (num === undefined || isNaN(num) || !isFinite(num)) {
        return undefined;
    }
    return num;
};

interface ZoomCenterifyArgs {
    domain: [ TimeStamp, TimeStamp ];
    newDuration: number;
    upperBound: TimeStamp;
    point?: TimeStamp;
};
const zoomCenterify = ({ domain: [originStart, originEnd], newDuration, upperBound, point }: ZoomCenterifyArgs): [ number, number ] => {
    let start = originStart;
    let end = originEnd;
    const centerPoint = validNumber(point) ?? (start + end) / 2;
    const prevDuration = end - start;
    if (prevDuration > 0) {
        const leftRatio = (centerPoint - start) / prevDuration;
        const leftDuration = leftRatio * newDuration;
        const rightRatio = (end - centerPoint) / prevDuration;
        const rightDuration = rightRatio * newDuration;
        if (centerPoint - leftDuration < 0) {
            start = 0;
            end = Math.min(newDuration, upperBound);
        } else if (centerPoint + rightDuration > upperBound) {
            end = upperBound;
            start = Math.max(upperBound - newDuration, 0);
        } else {
            start = centerPoint - leftDuration;
            end = centerPoint + rightDuration;
        }
    }
    return [start, end];
};

export class Domain {
    maxDuration: TimeStamp;
    readonly ZOOM_RATE: number = 1 + GOLDEN_RATE;
    readonly BOUNDARY_ZOOM_RATE: number = 1 + (GOLDEN_RATE * 10);
    private readonly _DEFAULT_DURATION: TimeStamp;
    private readonly _UPPER_BOUND: TimeStamp = Number.MAX_VALUE;
    private readonly _LOWER_BOUND: TimeStamp;

    private _domainStart: TimeStamp;
    private _domainEnd: TimeStamp;
    private _endTimeAll: TimeStamp;
    private _realTimeUpdate: boolean = true;
    private _chartViewWidth: number = 0;
    private readonly _debouncedSetZoomingHistory;

    constructor(isNsMode: boolean, endTimeAll: TimeStamp | undefined, debouncedSetZoomingHistory: (range: DomainRange) => void) {
        makeAutoObservable(this);
        // this is var is the viewport represents default time duration
        // return ns time duration or ms time duration
        this._DEFAULT_DURATION = isNsMode ? 2e10 : 2e4;
        // one screen represent 5min at most
        // one screen represent 10[us] or 625[ms] at least
        this._LOWER_BOUND = isNsMode ? 1 : 625;
        this._endTimeAll = endTimeAll ?? this._DEFAULT_DURATION;
        this._domainStart = 0;
        this.maxDuration = endTimeAll === undefined ? this._DEFAULT_DURATION : Math.min(this._UPPER_BOUND, endTimeAll * this.BOUNDARY_ZOOM_RATE);
        this._domainEnd = this.maxDuration;
        this._debouncedSetZoomingHistory = debouncedSetZoomingHistory;
    }

    // temp api for WASD shortcut
    get lowerBound(): TimeStamp {
        return this._LOWER_BOUND;
    }

    get defaultDuration(): TimeStamp {
        return this._DEFAULT_DURATION;
    }

    get duration(): TimeStamp {
        return this._domainEnd - this._domainStart;
    }

    get isLowerBound(): boolean {
        return this.duration === this._LOWER_BOUND;
    }

    get isUpperBound(): boolean {
        return this.duration >= this.maxDuration;
    }

    get timePerPx(): number {
        if (this._chartViewWidth === 0) {
            return this.duration;
        }
        return this.duration / this._chartViewWidth;
    }

    set endTimeAll(endTimeAll: TimeStamp) {
        this._endTimeAll = endTimeAll;
        if (this._endTimeAll > 0 && this.maxDuration / this._endTimeAll < 2 * this.BOUNDARY_ZOOM_RATE) {
            this.maxDuration = Math.min(this._UPPER_BOUND, this._endTimeAll * this.BOUNDARY_ZOOM_RATE);
        }
        if (this._endTimeAll > this._domainEnd) {
            this._realTimeUpdate && ([this._domainStart, this._domainEnd] = [endTimeAll - this.duration, endTimeAll]);
        }
    }

    set zoom({ zoomCount, zoomPoint }: { zoomCount: number; zoomPoint?: TimeStamp }) {
        // deprecate once operation when the domain is up to boundary and this operation is invalid.
        const notFit = (this.duration === this.maxDuration && zoomCount > 0) ||
            (this.duration === this._LOWER_BOUND && zoomCount < 0) ||
            (this.duration >= MAX_ZOOM_DURATION && zoomCount > 0); // 新增：限制缩小

        if (notFit) {
            return;
        }

        const zoomEnd = (this._domainEnd > this._endTimeAll) && zoomCount < 0 ? this._endTimeAll : this._domainEnd;
        const zoomCenterLine = (zoomPoint ?? 0) > zoomEnd ? zoomEnd : zoomPoint;

        // 修改 clamp 的上界
        const newDuration = clamp(
            this.duration * Math.pow(this.ZOOM_RATE, zoomCount),
            this._LOWER_BOUND,
            Math.min(this.maxDuration, MAX_ZOOM_DURATION), // 使用两者中的较小值
        );

        [this._domainStart, this._domainEnd] = zoomCenterify({
            domain: [this._domainStart, zoomEnd],
            newDuration,
            upperBound: this.maxDuration,
            point: zoomCenterLine,
        });
        if (this._domainEnd !== this._endTimeAll) {
            this._realTimeUpdate = false;
        }
        this._debouncedSetZoomingHistory({ domainStart: this._domainStart, domainEnd: this._domainEnd });
    };

    get domainRange(): DomainRange {
        return { domainStart: this._domainStart, domainEnd: this._domainEnd };
    }

    set domainRange({ domainStart, domainEnd }: DomainRange) {
        if (domainStart === this._domainStart && domainEnd === this._domainEnd) { return; }
        const positiveStart = Math.abs(domainStart);
        const positiveEnd = Math.abs(domainEnd);
        if (positiveStart === 0 && positiveEnd === 0) {
            // reset domainRange
            [this._domainStart, this._domainEnd] = [0, this._DEFAULT_DURATION];
        } else if (positiveEnd - positiveStart < this._LOWER_BOUND || positiveEnd - positiveStart > this.maxDuration) {
            // when domain oversize, zoom adaptably.
            const newDuration = clamp(this.duration, this._LOWER_BOUND, this.maxDuration);
            [this._domainStart, this._domainEnd] = zoomCenterify({
                domain: [positiveStart, positiveEnd],
                newDuration,
                upperBound: this.maxDuration,
            });
        } else {
            [this._domainStart, this._domainEnd] = [domainStart, domainEnd];
        }
    }

    get realTimeUpdate(): boolean {
        return this._realTimeUpdate;
    }

    set realTimeUpdate(realTimeUpdate: boolean) {
        this._realTimeUpdate = realTimeUpdate;
    }

    set chartViewWidth(width: number) {
        if (width === this._chartViewWidth) { return; }
        if (this._chartViewWidth !== 0) {
            this.maxDuration = width / this._chartViewWidth * this.maxDuration;
        } else {
            this._chartViewWidth = width;
        }
        [this._domainStart, this._domainEnd] = zoomCenterify({
            domain: [this._domainStart, this._domainEnd],
            newDuration: this.timePerPx * width,
            upperBound: this.maxDuration,
        });
        this._chartViewWidth = width;
    }
}
