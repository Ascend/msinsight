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
import { makeAutoObservable, when } from 'mobx';
import type { FC } from 'react';
import type { Theme } from '@emotion/react';
import { debounce, omit } from 'lodash';
import i18n from '@insight/lib/i18n';
import { type Caches } from '../cache/cache';
import { toLocalTimeString } from '../utils/humanReadable';
import { type TimeStamp } from './common';
import { Domain, DomainRange, MAX_ZOOM_DURATION } from './domain';
import type { InsightUnit, UnitMatcher, LinkLines } from './insight';
import { type TimeLineMaker, TIME_MAKER_DEFAULT } from './timeMaker';
import { platform } from '../platforms';
import { type Phase, stateTexts } from '../utils/constant';
import { SimpleCache } from '../cache/simplecache';
import { InsightUnitSet } from '../utils/PageSetting';
import { getTimeOffsetKey, setTimeOffsetForUnitTree, visitUnitTree } from '../insight/units/utils';
import { CardMetaData, SliceData, SliceMeta, ThreadMetaData, ThreadTrace } from './data';
import { CardRankInfo } from '../api/interface';
import { getRootUnit } from '../utils';
import { getAutoKey } from '../utils/dataAutoKey';
import type { FlowPoint } from '../insight/units/AscendUnit';
import { MergedThreadData } from './mergedThreadData';

export const MAX_ZOOM_COUNT = 10000;

export interface SelectedParams {
    baseRawId?: number;
    curRawId?: number;
}

export type SelectedData = Record<string, unknown> & {
    sourceUnit?: InsightUnit;
};

export type ValidSession = Session & { startRecordTime: TimeStamp; phase: Exclude<Phase, 'configuring'> };

export function isValidSession(session?: Session): session is ValidSession {
    return !(session === undefined || session.phase === 'configuring' || session.startRecordTime === undefined);
}
export type LinkDataType<T extends Record<string, unknown> = Record<string, unknown>> = T & { startTime: number; height: number; duration: number };
export type DataMatcher = (unit: InsightUnit) => boolean;
export interface LinkData {
    target: {
        data: LinkDataType;
        matcher: DataMatcher;
    };
    sources: Array<{ data: LinkDataType; matcher: DataMatcher }>;
}
export interface ContextMenu {
    isVisible: boolean;
    zoomHistory: DomainRange[];
    activeMenuKey: string;
}

interface UnitsConfig {
    jsAllocationUsage: {
        isRecordStackTraces: boolean;
    };
    nativeConfig: {
        filterSize: number;
        maxStackDepth: number;
    };
    offsetConfig: {
        timestampOffset: Record<string, number>;
    };
    filterConfig: {
        pythonFunction: Record<string, boolean>;
    };
}

export interface SelectedDataType extends Pick<ThreadTrace, 'duration' | 'startTime' | 'name'> {
    id?: SliceData['id'];
    type?: string;
    depth?: ThreadTrace['depth'];
    threadId: SliceMeta['threadId'] | SliceData['tid'];
    processId: SliceMeta['processId'];
    cardId?: SliceMeta['cardId'];
    dbPath?: string;
    metaType?: ThreadMetaData['metaType'];
    rawStartTime?: string;
    color?: keyof Theme['colorPalette'] | Array<[ number, keyof Theme['colorPalette'] ]>;
    startRecordTime?: number;
    showSelectedData?: boolean;
    showDetail?: boolean;
    timestamp?: number;
}

export type TimelineScale = (x: number) => number;

interface ScaleBag {
    timelineMarkerTimeScale: TimelineScale | null;
    timelineMarkerXScale: TimelineScale | null;
}

export interface MapValueOfLinkLines {
    cat: string;
    from: FlowPoint[];
    to: FlowPoint[];
    current: FlowPoint;
}

export class Session {
    language: 'zhCN' | 'enUS' = 'enUS';
    id = '';
    remoteAttrs: Map<string, Record<string, unknown>> = new Map();
    singleLinkLine: LinkLines = {};
    linkLineCategories: string[] = [];
    // 是否等待解析
    isPending: boolean = false;
    // 是否loading解析按钮
    isParserLoading: boolean = false;
    // 是否是算子仿真图
    isSimulation: boolean = false;
    // 记录所有数据的loading状态的对象 key为卡的dbPath
    asyncDataLoadingList: { [key: string]: any } = {};
    // 是否是服务化场景
    isIE: boolean = false;
    // 是否隐藏了算子调优flag事件
    areFlagEventsHidden: boolean = false;
    // 快捷键对齐触发渲染
    alignRender: boolean = false;
    isCluster: boolean = false;
    isMultiCluster: boolean = false;
    // 页面可视范围的打开的Card的CardId
    viewedExpandedCardIdSet: Set<string> = new Set<string>();
    selectedMultiSlice: string = '';
    isMultiDevice: boolean = false; // 判断项目是否是单Host多Device
    isFullDb: boolean = false;
    // 是否是ipynb文件
    isIpynb: boolean = false;
    ipynbUrl: string = '';
    // 当前项目已导入的数据类型，用于 System View 标签过滤
    hasFtraceData: boolean = false;
    hasNonFtraceData: boolean = false;
    // context menu state
    contextMenu: ContextMenu = {
        isVisible: false,
        zoomHistory: [],
        activeMenuKey: '',
    };

    // 是否有值为超过了最大安全值
    isOverflowMaxSafeNumber: boolean = false;
    // 是否更新覆盖RankId
    isNeedResetRankId: boolean = true;

    pinnedUnits: InsightUnit[] = [];
    icon?: JSX.Element;
    caches: Caches | null = null;
    simpleCache: SimpleCache;

    // Frontend start time of recording.
    startRecordTime: TimeStamp;

    // Any data out of max duration would be dropped, Number.MAX_SAFE_INTEGER means unlimited
    maxDuration = Number.MAX_SAFE_INTEGER;

    // should use ns time unit
    isNsMode: boolean = false;

    startTime: string;

    // some params for selected value which is not a range.
    selectedParams: SelectedParams = { baseRawId: undefined, curRawId: undefined };
    scrollTop: number = 0;
    // Timeline模块键盘滚动区域
    scrollArea: string = '';
    unitsConfig: UnitsConfig = {
        jsAllocationUsage: {
            isRecordStackTraces: false,
        },
        nativeConfig: {
            filterSize: 4096,
            maxStackDepth: 10,
        },
        offsetConfig: {
            timestampOffset: {},
        },
        filterConfig: {
            pythonFunction: {},
        },
    };

    searchData?: { content: string; isMatchCase: boolean; isMatchExact: boolean; rankId?: string };
    doContextSearch?: boolean;
    showEvent?: boolean;
    linkLines: LinkLines = {};
    drawLineMode: 'all' | 'single' = 'all';
    mapOfLinkLines: Map<string, MapValueOfLinkLines> = new Map();
    shouldRefetchLines: boolean = false;
    // 记录泳道第一次展开时Thread类型的子泳道
    threadsToFetch: Map<string, InsightUnit> = new Map();

    totalHeight: number = 0;
    renderTrigger: boolean = true;
    isTimeAnalysisMode: boolean = false; // 时间范围分析模式
    timeAnalysisRange?: [ TimeStamp, TimeStamp ]; // 时间分析范围
    /**
     * 页面是否有m快捷键的遮罩m
     */
    mKeyRender: boolean = false;
    /**
     * m快捷键遮罩范围
     */
    mMaskRange: number[] = [];
    /**
     * 框选的范围是否已经被锁定
     */
    selectedRangeIsLock: boolean = false;

    /**
     * 锁定的框选范围
     */
    lockRange?: [ TimeStamp, TimeStamp ];

    /**
     * 锁定的泳道个数
     */
    lockUnitCount: number = 0;

    /**
     * 锁定的泳道
     */
    lockUnit: InsightUnit[] = [];

    /**
     * 表格点击的连线类型
     */
    ridLineType: string = '';
    buttons: Array<FC<{ session: Session }>>;

    // set this field with a new matcher to trigger jump-to-target-lane
    locateUnit?: UnitMatcher;

    timer: ReturnType<typeof setInterval> | undefined;

    sharedState: Record<string, unknown> = {}; // used for sharing state across different units

    // timeline flag data source
    timelineMaker: TimeLineMaker = TIME_MAKER_DEFAULT;

    zoom?: {zoomCount: number; zoomPoint?: number };
    doReset: boolean = false;
    eventUnits: InsightUnit[] = [];
    projectName?: string;
    mergedThreadData: MergedThreadData;
    pageSetting: Record<string, {
        domainRange: DomainRange;
        units: InsightUnitSet[];
        pinnedUnits: InsightUnit[];
    } | undefined> = {};

    autoAdjustUnitHeight: boolean = false;
    // 是否处于平移模式
    panMode: boolean = false;
    // 是否临时按下了平移快捷键
    panModePressed: boolean = false;
    showBottomPanel: boolean | null = null;
    // 是否处于拖拽场景下
    isDragging: boolean = false;
    debouncedSetZoomingHistory;
    hoverMouseX: number | null = null; // 鼠标悬浮线的 x 轴坐标
    showCreateFlagMarkKey: boolean = false; // 是否显示创建旗帜标记快捷键图标
    scaleBag: ScaleBag = {
        timelineMarkerTimeScale: null,
        timelineMarkerXScale: null,
    };

    modeOfParse: 'auto_parse' | 'global_parse' = 'auto_parse';

    parseQueue: Array<() => void> = [];

    sliceSelection = {
        active: false, // 切换算子框选模式
        selecting: false, // 正在框选中
        startPos: [] as number[], // 框选起始点
        rangeOfLevels: [] as number[], // 框选覆盖层级
        targetUnit: null as (InsightUnit | null),
        activeIsChanged: false,
        searchOfSlice: false,
    };

    private _selectedRange?: [ TimeStamp, TimeStamp ];
    private readonly _domain: Domain;
    // Relative to the startTimeOffset, which means that it will start from 0.
    private _initEndTimeAll: TimeStamp | undefined;
    private _endTimeAll: TimeStamp | undefined;
    private _name: string | null;
    private _phase: Phase = 'configuring';
    private _units: InsightUnit[] = [];
    private readonly _rankCardInfoMap: Map<string, CardRankInfo> = new Map(); // rank key(clusterId+host+realRankId+deviceId) -> CardInfo
    private _availableUnits: InsightUnit[] = [];
    private _selectedData?: SelectedDataType;
    private _benchMarkData?: Record<string, unknown>;
    private _alignSliceData: SliceData[] = [];
    private _selectedRangeData?: Array<Record<string, unknown>>;
    private _interval: number;
    private _selectedUnitKeys: string[] = [];
    private _selectedUnits: InsightUnit[] = []; // redundant for reducing extra computation
    private _disableZoomingHistory: boolean = false; // 禁止生成缩放历史记录

    constructor(conf?: Partial<Session>) {
        makeAutoObservable(this, {
            timer: false,
            selectedUnits: false,
            caches: false,
            isNsMode: false,
            printSessionInfo: false,
            linkLines: false,
            mapOfLinkLines: false,
            singleLinkLine: false,
            resetOfSliceSelection: false,
            threadsToFetch: false,
        });
        this._name = conf?.name ?? this.id;
        this._interval = 100;
        this.startRecordTime = 0;
        this.startTime = '';
        if (conf) {
            Object.assign(this, conf);
        }
        this.debouncedSetZoomingHistory = debounce(this.setZoomingHistory.bind(this), 300);
        this._domain = new Domain(this.isNsMode, this.endTimeAll, this.debouncedSetZoomingHistory);
        this.mergedThreadData = new MergedThreadData();
        this.buttons = conf?.buttons ?? [];
        this.simpleCache = new SimpleCache();
        // 录制时长大于等于5min，建议结束录制
        const MAXTIME = this.isNsMode ? 5 * 60 * 1e9 : 5 * 60 * 1e3;
        when(
            () => this._endTimeAll !== undefined && this._endTimeAll >= MAXTIME,
            () => {
                const showTimeoutTip = window.localStorage.getItem('showTimeoutTip');
                if (showTimeoutTip === null) {
                    platform.notify(i18n.t('notify:5012'));
                    window.localStorage.setItem('showTimeoutTip', 'false');
                }
            },
        );
    }

    get endTimeAll(): TimeStamp | undefined {
        return this._endTimeAll;
    }

    get name(): string | null {
        return this._name;
    }

    get phase(): Phase {
        return this._phase;
    }

    get statusInfo(): string {
        if (this.phase === 'configuring') {
            return 'Idle';
        } else if (this.phase === 'download' && this.startRecordTime !== undefined) {
            return `Recorded at: ${toLocalTimeString(this.startRecordTime)}`;
        } else {
            return stateTexts[this.phase];
        }
    }

    get selectedUnitKeys(): (string[]) {
        return this._selectedUnitKeys;
    }

    get domainRange(): DomainRange {
        const { domainStart, domainEnd } = this._domain.domainRange;
        return { domainStart, domainEnd };
    }

    get realTimeUpdate(): boolean {
        return this._domain.realTimeUpdate && this.phase === 'recording';
    }

    get domain(): Domain {
        return this._domain;
    }

    get interval(): number {
        return this._interval;
    }

    get units(): InsightUnit[] {
        return this._units;
    }

    get rankCardInfoMap(): Map<string, CardRankInfo> {
        return this._rankCardInfoMap;
    }

    get availableUnits(): InsightUnit[] {
        return this._availableUnits;
    }

    get selectedData(): SelectedDataType | undefined {
        return this._selectedData;
    }

    get benchMarkData(): Record<string, unknown> | undefined {
        return this._benchMarkData;
    }

    get alignSliceData(): SliceData[] {
        return this._alignSliceData;
    }

    get selectedRangeData(): Array<Record<string, unknown>> | undefined {
        return this._selectedRangeData;
    }

    get selectedUnits(): InsightUnit[] {
        return this._selectedUnits;
    }

    get selectedRange(): [TimeStamp, TimeStamp] | undefined {
        return this._selectedRange;
    }

    get haveCreateFlagMarkPosition(): boolean {
        // 1. 有区间框选时，可以创建旗帜标记
        // 2. 没有区间框选，有鼠标悬浮线的坐标时，可以创建旗帜标记
        return this._selectedRange !== undefined || this.hoverMouseX !== null;
    }

    set endTimeAll(endTimeAll: TimeStamp | undefined) {
        this._initEndTimeAll = endTimeAll;
        this.updateEndTimeAll();
    }

    set name(value: string | null) {
        this._name = value;
    }

    set phase(value: Phase) {
        this._phase = value;
    }

    set selectedUnitKeys(value: string[]) {
        if (this.selectedRangeIsLock) {
            return;
        }
        this._selectedUnitKeys = value;
    }

    set domainRange(domainRange: DomainRange) {
        const { domainStart, domainEnd } = domainRange;
        let range = domainRange;

        if (domainEnd - domainStart > MAX_ZOOM_DURATION) {
            range = { domainStart, domainEnd: domainStart + MAX_ZOOM_DURATION };
        }
        this._domain.domainRange = range;
        if (!this._disableZoomingHistory) {
            this.debouncedSetZoomingHistory(range);
        }
    }

    set realTimeUpdate(realTime: boolean) {
        this._domain.realTimeUpdate = realTime && this.phase === 'recording';
    }

    set interval(value: number) {
        this._interval = value;
    }

    set units(units: InsightUnit[]) {
        const rootUnits = getRootUnit(this._units); // db 情况 this._units 不是根泳道，因此需要先获取根泳道
        if (rootUnits.length > 0) { // 更新根泳道到 units 的引用
            const map = units.reduce((map, unit) => {
                for (const root of rootUnits) {
                    if (root !== unit.parent) { continue; }
                    if (!map.has(root)) { map.set(root, []); }
                    map.get(root)?.push(unit);
                    break;
                }
                return map;
            }, new Map<InsightUnit, InsightUnit[]>());
            map.forEach((v, k) => { k.children = v; });
        }
        this._units = units;
    }

    set availableUnits(availableUnits: InsightUnit[]) {
        this._availableUnits = availableUnits;
    }

    set selectedData(data: SelectedDataType | undefined) {
        this._selectedData = data;
    }

    set benchMarkData(data: Record<string, unknown> | undefined) {
        this._benchMarkData = data;
    }

    set alignSliceData(data: SliceData[]) {
        this._alignSliceData = data;
    }

    set selectedRangeData(data: Array<Record<string, unknown>> | undefined) {
        this._selectedRangeData = data;
    }

    set selectedUnits(data: InsightUnit[]) {
        if (this.selectedRangeIsLock) {
            return;
        }
        this._selectedUnits = data;
        this._selectedUnitKeys = this._selectedUnits.map(getAutoKey);
    }

    set selectedRange(data: [TimeStamp, TimeStamp] | undefined) {
        if (this.selectedRangeIsLock) {
            console.warn('[WARN] selectedRange is locked, cannot set value.');
            return;
        }
        this._selectedRange = data;
    }

    updateEndTimeAll(): void {
        this._endTimeAll = this._initEndTimeAll === undefined ? undefined : (this._initEndTimeAll + this.getMaxRelativeOffset());
        this._domain !== undefined && (this._domain.endTimeAll = this._endTimeAll ?? this.domain.maxDuration);
    }

    setTimestampOffset(key: string, value: number): void {
        const prevObj = this.unitsConfig.offsetConfig.timestampOffset;
        this.unitsConfig.offsetConfig.timestampOffset = { ...prevObj, [key]: (value) };
        this.updateEndTimeAll();
    }

    setTimestampOffsetAll(offsetConfig: Record<string, number>): void {
        this.unitsConfig.offsetConfig.timestampOffset = { ...offsetConfig };
        this.updateEndTimeAll();
    }

    setTimestampOffsetByUnit(unit: InsightUnit, value: number, shouldUpdate: boolean = true): void {
        const prevObj = { ...this.unitsConfig.offsetConfig.timestampOffset };
        setTimeOffsetForUnitTree(this, unit, value, prevObj);
        this.unitsConfig.offsetConfig.timestampOffset = {
            ...prevObj,
            [(unit.metadata as unknown as CardMetaData).cardId]: (value),
        };
        if (shouldUpdate) { this.updateEndTimeAll(); }
    }

    printSessionInfo(): string {
        return `${JSON.stringify({ ...omit(this, ['caches', 'sharedState', '_units']) })}`;
    }

    // 对于 Text 的单 Host 多 Device 场景，只保留一个卡，对于 db 的单 Host 多 Device 场景，只保留一个 host
    updateUnitsForMultiDevice(): void {
        const rootUnits = getRootUnit(this._units); // db 情况 this._units 不是根泳道，因此需要先获取根泳道
        if (!this.isMultiDevice) {
            return;
        }
        const len = rootUnits.length;
        for (let i = 1; i < len; ++i) {
            this.setUnitMultiDeviceHidden(rootUnits[i], true);
        }
    }

    setDomainWithoutHistory(domainRange: DomainRange): void {
        this._disableZoomingHistory = true;
        this.domainRange = domainRange;
        this._disableZoomingHistory = false;
    }

    setZoomingHistory(domainRange: DomainRange): void {
        this.contextMenu.zoomHistory.push(domainRange);
        if (this.contextMenu.zoomHistory.length > MAX_ZOOM_COUNT) {
            this.contextMenu.zoomHistory = this.contextMenu.zoomHistory.slice(-MAX_ZOOM_COUNT);
        }
    }

    resetOfSliceSelection(isSelecting = true): void {
        this.sliceSelection.startPos = [];
        this.sliceSelection.rangeOfLevels = [];
        this.sliceSelection.selecting = isSelecting;
        this.sliceSelection.targetUnit = null;
    }

    deleteRankCardInfoMapItemByDbPath(dbPath: string): boolean {
        for (const [k, v] of this._rankCardInfoMap) {
            if (v.dbPath === dbPath) {
                this._rankCardInfoMap.delete(k);
                return true;
            }
        }
        return false;
    }

    // 用于更新 unit 下的全部泳道是否可见
    private setUnitMultiDeviceHidden(unit: InsightUnit, hidden: boolean): void {
        const visited = new Set<InsightUnit>();
        const setUnitDisplayable = (child: InsightUnit): void => {
            if (visited.has(child)) {
                return;
            }
            child.isMultiDeviceHidden = hidden;
            visited.add(child);
            if (child.children) {
                for (const item of child.children) {
                    setUnitDisplayable(item);
                }
            }
        };
        setUnitDisplayable(unit);
    }

    private getMaxRelativeOffset(): number {
        if (!Array.isArray(this._units) ||
            this.unitsConfig.offsetConfig.timestampOffset === undefined ||
            Object.keys(this.unitsConfig.offsetConfig.timestampOffset).length === 0) {
            return 0;
        }

        const getRelativeOffset = (unit: InsightUnit): number => {
            const initTimeOffset = unit.alignStartTimestamp ?? 0;
            const timeOffsetKey = getTimeOffsetKey(this, unit.metadata as { cardId?: string; processId?: string });
            const currentTimeOffset = this.unitsConfig.offsetConfig.timestampOffset[timeOffsetKey] ?? 0;
            return Math.max(0, initTimeOffset - currentTimeOffset);
        };

        const result = this._units.flatMap((unit: InsightUnit): number[] => {
            const relativeOffsets: number[] = [];
            visitUnitTree(unit, (item) => {
                relativeOffsets.push(getRelativeOffset(item as InsightUnit));
            });
            return relativeOffsets;
        });
        return Math.max(...result);
    }
}
