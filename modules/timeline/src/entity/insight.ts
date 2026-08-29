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
import { makeAutoObservable, observable, runInAction } from 'mobx';
import type React from 'react';
import type { CommonStateProto } from '../components/details/base/Tabs';
import type { ChartConfig, ChartDecorator, ChartReaction, ChartType, GetChartConfig, MapFunc, RenderTooltip } from './chart';
import type { ElementType, TreeNode } from './common';
import type { Session } from './session';
import type { TabState } from './tabDependency';
import { MetaDataBase } from './data';

// #region chart desc
/**
 * chart description
 *
 * @template T: chart type
 * @member dataFields fields you want in this unit, @see {@link DataFields}
 * @member mapFunc a mapping from Data Insight Core type to chart data type
 */
export interface ChartDesc<T extends ChartType> {
    type: T;
    offline?: boolean;
    mapFunc: MapFunc<T>;
    config: ChartConfig<T> | GetChartConfig<T>;
    height: UnitHeight;
    renderTooltip?: RenderTooltip<T>;
    onHover?: ChartReaction<T>;
    onClick?: ChartReaction<T>;
    decorator?: ChartDecorator<T>;
    error?: boolean;
}

export enum ProjectType {
    DB = 0,
    BIN = 1,
    IPYNB = 2,
    TEXT_CLUSTER = 3,
    SIMULATION = 4,
    TRACE = 5,
    DB_CLUSTER = 6,
    IE = 7,
    DB_NPUMONITOR = 8,
    OTHER = 9,
}

export const isGetChartConfig = <T extends ChartType>(config: ChartDesc<T>['config']): config is GetChartConfig<T> => {
    return typeof config === 'function';
};

type FixedType = 'scroll' | 'right' | 'left';
type ColumnWidth = number | 'max-content' | 'auto';

// factory method to capture the generics for precise code completion for ChartDesc fields
export const chart = <T extends ChartType>(param: ChartDesc<T>): ChartDesc<ChartType> => ({ offline: false, ...param }) as unknown as ChartDesc<ChartType>;
// #endregion

// #region detail desc
export type MoreDescriptor = MoreDesc<Record<string, any[]>, string>;

interface MoreDesc<DataType extends object, Field extends keyof DataType> {
    field: Field;
    columns: Array<ColumnDef<ElementType<DataType[Field]>>>;
    childrenColumnName?: string;
    rowKey?: (data: ElementType<DataType[Field]>) => string;
    onExpand?: (session: Session, data: TreeNode<DataType>) => Promise<TreeNode<DataType> | undefined>;
};

type CellRenderer<DataType> = (d: DataType, session: Session, tabState: TabState | undefined) => (string | JSX.Element);

export type ColumnDef<DataType> = // name, renderer, width?, fixed?
    [string, CellRenderer<DataType>] |
    [string, CellRenderer<DataType>, ColumnWidth] |
    [string, CellRenderer<DataType>, ColumnWidth, FixedType] |
    [string, CellRenderer<DataType>, ColumnWidth, FixedType | undefined, (p: any) => boolean];

export type SummaryFunction<DataType> = (dataSource: DataType[]) => (string | JSX.Element);

export interface TableDataAdapter<DataType extends Record<string, unknown>> {
    columns: Array<ColumnDef<DataType>>;
    actions?: Array<FilterDef<DataType> & SorterDef<DataType>>;
    summaries?: Map<string, SummaryFunction<DataType>>;
};

interface FilterDef<DataType extends Record<string, unknown>> {
    filterKey?: keyof DataType | Array<keyof DataType>;
};

export interface SorterDef<DataType extends Record<string, unknown>> {
    sorter?: (a: DataType, b: DataType) => number | boolean;
};

export { type MetaDataBase as MetaData };

interface TabularClickCacallbackArgs<CommonState extends CommonStateProto, DataType> {
    row: DataType;
    session: Session;
    detail: DetailDescriptor<MetaDataBase>;
    commonState?: CommonState;
    unit?: InsightUnit;
};

interface TabularEnterCacallbackArgs<DataType> {
    row: DataType;
    session: Session;
};

type DetailDesc<
    DataType extends Record<string, unknown>,
    ExtraDataType extends Record<string, unknown>,
    MoreDataType extends Record<string, unknown>,
    MetaData,
    Field extends keyof DataType,
> = {
    childrenColumnName?: string;
    name?: string;
    fetchData: (session: Session, metadata: MetaData) => Promise<DataType[]>;
    fetchExtraData?: (session: Session, metadata: MetaData) => Promise<ExtraDataType[]>;
    fetchMoreData?: (session: Session, metadata: MetaData) => Promise<MoreDataType[]>;
    onExpand?: (session: Session, data: TreeNode<DataType>) => Promise<TreeNode<DataType> | undefined>; // if data is not updated, return promise of undefined
    rowKey?: (data: DataType) => string;
    more?: MoreDesc<DataType, Field>;
    clickCallback?: <CommonState extends CommonStateProto>(args: TabularClickCacallbackArgs<CommonState, DataType>) =>
    void; // execute statement in onClick event
    doubleClickCallback?: <CommonState extends CommonStateProto>(args: TabularClickCacallbackArgs<CommonState, DataType>) => void;
    mouseEnterCallback?: (args: TabularEnterCacallbackArgs<DataType>) => void;
    mouseLeaveCallback?: (args: TabularEnterCacallbackArgs<DataType>) => void;
} & TableDataAdapter<DataType>;

export type renderFieldsType<DataType> =
[string, (data: DataType, session: Session, metadata?: unknown) => (string | JSX.Element), (data: DataType, session?: Session,) => boolean ] |
[string, (data: DataType, session: Session, metadata?: unknown) => (string | JSX.Element) ];

export interface SingleDataDesc<DataType extends Record<string, unknown>, MetaData> {
    name?: string;
    fetchData: (session: Session, metadata: MetaData) => Promise<DataType>;
    renderFields: Array<renderFieldsType<DataType>>;
    clickCallback?: (args: SingleDataDesc<Record<string, unknown>, unknown>) => void; // execute statement in onClick event
};

export interface LinkDataDesc<DataType extends Record<string, unknown>> {
    fetchData: (session: Session, metadata: MetaDataBase) => Promise<DataType[] | DataType>;
    templateField?: renderFieldsType<DataType>;
    renderFields: Array<renderFieldsType<DataType>>;
    onDestroy?: (session: Session) => void;
};

export const linkData = <T extends Record<string, unknown>>(desc: LinkDataDesc<T>): LinkDataDesc<Record<string, unknown>> =>
    desc as unknown as LinkDataDesc<Record<string, unknown>>;

export const singleData = <T extends Record<string, unknown>, MetaData>(desc: SingleDataDesc<T, MetaData>): SingleDataDesc<Record<string, unknown>, unknown> =>
    desc as unknown as SingleDataDesc<Record<string, unknown>, unknown>;

export type DetailDescriptor<UnitInfo> = DetailDesc<Record<string, unknown>, Record<string, unknown>, Record<string, unknown>, UnitInfo, string>;

export function detail<
    T extends Record<string, unknown>,
    K extends Record<string, unknown>,
    V extends Record<string, unknown>,
    MetaData,
>(desc: DetailDesc<T, K, V, MetaData, keyof T>): DetailDescriptor<MetaData> {
    return {
        ...desc,
        childrenColumnName: desc.childrenColumnName ?? 'children',
    } as unknown as DetailDescriptor<MetaData>;
}

export type BottomPanelSingleRender = <Metadata>(session: Session, metadata: Metadata) => {
    DetailTitle?: React.FC<{ session: Session }> | string;
    Detail?: React.FC<{ session: Session; height: number }>;
    More?: React.FC<{ session: Session; height: number }>;
    Toolbar?: React.FC<{ session: Session }>;
    MoreTitle?: React.FC<{ session: Session }> | string;
    moreWh?: number;
    open?: boolean;
};

export type BottomPanelRender = <Metadata>(session: Session, metadata: Metadata) => Array<{
    DetailTitle?: React.FC<{ session: Session }> | string;
    Detail?: React.FC<{ session: Session; height: number }>;
    More?: React.FC<{ session: Session; height: number }>;
    Toolbar?: React.FC<{ session: Session }>;
    MoreTitle?: React.FC<{ session: Session }> | string;
    moreWh?: number;
    open?: boolean;
}>;

export interface MenuType {
    value?: string; // 传给服务端的值
    showValue?: string; // 下拉列表显示的值
    key?: string; // 传给服务端的键
    showKey?: string; // 已选列表标签里显示的键
    children?: MenuType[];
    isTrigger?: boolean; // 当前节点是否触发搜索
    type?: 'number' | 'string';
    mode?: 'key' | 'value' | 'input' | 'keyValue';
}; // #endregion

export interface InsightUnitParams<
    MetaData extends MetaDataBase,
    DetailType extends Record<string, unknown>,
    ExtraDataType extends Record<string, unknown>,
    MoreDataType extends Record<string, unknown>,
> {
    name: string;
    pinType?: 'move' | 'copied';
    configBar?: (session: Session, metadata: MetaData, onClick?: () => void, isHovered?: boolean, isSelected?: boolean) => JSX.Element | null;
    tag?: string | ((session: Session, metadata: MetaData) => string | null);
    description?: string;
    chart?: ChartDesc<ChartType> | Array<ChartDesc<ChartType>>;
    renderInfo?: (session: Session, metadata: MetaData, thisUnit: InsightUnit) => JSX.Element | string | null;
    detail?: DetailDesc<DetailType, ExtraDataType, MoreDataType, MetaData, keyof DetailType>;
    bottomPanelRender?: BottomPanelRender;
    metadata: MetaData;
    tabState?: TabState;
    spreadUnits?: SpreadDesc;
    notifications?: Array<(metaData: MetaData) => (false | string)>; // false代表无需提示信息 不为false返回提示的信息
    searchConfig?: MenuType;
    buttons?: Array<React.FC<{ session: Session }>>;
    collapseAction?: (unit: InsightUnit) => void;
    collapsible?: boolean;
    alignStartTimestamp?: number;
}

export interface InsightUnitClass { new(metadata: never): InsightUnit };

export enum UnitHeight {
    SUPER_UPPER = 120,
    HIGHTER_UPPER = 105,
    UPPER = 40,
    // 算子缩略图泳道高度
    COLL = 25,
    // 展开的算子标准高度
    STANDARD = 20,
    LOWER = 10,
    SUPER_LOWER = 6,
}

/**
 * Defines some runtime states that are kept in insight unit instances.
 */
export interface InsightUnit extends InsightUnitParams<MetaDataBase, Record<string, unknown>, Record<string, unknown>, Record<string, unknown>> {
    expandable: boolean;
    isExpanded: boolean;
    hasExpanded: boolean;
    height: () => number;
    isMultiDeviceHidden: boolean; // 判断是否是单 Host 多 Device 中需要隐藏的泳道
    isDisplay: boolean;
    isOverlapAnalysisLoading: boolean;
    children?: InsightUnit[];
    type: 'basic' | 'transparent';
    phase: string;
    isUnitVisible: boolean;
    isMerged: boolean; // 是否被合并
    isAutoMergedUnit?: boolean; // 是否由导入时的自动合并配置生成
    isSummaryLoading: boolean; // 泳道缩略图是否加载中
    isTraceLoading: boolean; // 泳道内容是否加载中
    parent?: InsightUnit;
    isParseLoading: boolean;
    shouldParse: boolean; // 判断timeline卡是否需要解析
    progress: number; // 解析进度：实际解析进度
    showProgress: boolean; // 解析进度：是否显示进度条
    havePythonFunction?: boolean;
    onceExpand?: boolean; // 只展开一次
    projectType?: ProjectType;
}
// recursiveSpreadUnits 已被删除，rendering 似乎不再使用 - 增加rendering状态，用于unit analyze完成后、session变为download之前的状态设置(主要是进行await recursiveSpreadUnits)
// 增加initializing状态，用于用户点击session start按钮后，unit plugin start完成之前的状态设置
// 解析状态在parseSucceess之后设置unit的phase为download
export type UnitPhase = 'configuring' | 'initializing' | 'recording' | 'analyzing' | 'rendering' | 'download' | 'error' | 'loading';

const heightOf = (chartDesc: InsightUnit['chart'], name: string): number => {
    if (chartDesc === undefined) {
        if (name === 'Root') {
            return UnitHeight.UPPER;
        }
        return UnitHeight.STANDARD;
    }
    return Array.isArray(chartDesc) ? chartDesc.reduce((prev, cur) => prev + cur.height + 1, 0) - 1 : chartDesc.height;
};

// #region spreadUnits desc
export type SpreadPhase = 'create' | 'analyze' | 'expand';

interface SpreadDesc {
    phase: SpreadPhase;
    action: (self: InsightUnit, session?: Session) => Promise<void>;
};

// factory method to capture SpreadDesc
export const on = (phase: SpreadPhase, action: (self: InsightUnit, session?: Session) => Promise<void>): SpreadDesc => ({ phase, action }); // #endregion

const transformDetail = <T>(detailDesc?: DetailDesc<Record<string, unknown>, Record<string, unknown>, Record<string, unknown>, T, string>):
DetailDesc<Record<string, unknown>, Record<string, unknown>, Record<string, unknown>, unknown, string> | undefined => {
    return detailDesc
        ? {
            ...detailDesc,
            // coerce the signature of fetchData to a set of more general types
            fetchData: detailDesc.fetchData as (session: Session, metadata: unknown) => Promise<Array<Record<string, unknown>>>,
            fetchExtraData: detailDesc?.fetchExtraData as (session: Session, metadata: unknown) => Promise<Array<Record<string, unknown>>>,
            fetchMoreData: detailDesc?.fetchMoreData as (session: Session, metadata: unknown) => Promise<Array<Record<string, unknown>>>,
        }
        : undefined;
};

const wrapSpread = (original?: SpreadDesc): SpreadDesc | undefined => {
    if (!original) { return undefined; }
    let isSpread = false;
    return {
        ...original,
        action: async (self: InsightUnit, session?: Session): Promise<void> => {
            if (isSpread) { return; }
            await original?.action(self, session);
            isSpread = true;
        },
    };
};

// eslint-disable-next-line max-lines-per-function
export const unitBase = <T extends MetaDataBase = MetaDataBase>(params:
Omit<InsightUnitParams<T, Record<string, unknown>, Record<string, unknown>, Record<string, unknown>>, 'metadata'>): typeof basicUnitClass => {
    const basicUnitClass = class implements InsightUnit {
        parent?: InsightUnit;
        _children?: InsightUnit[];
        isMultiDeviceHidden = false;
        isUnitVisible = true;
        isMerged = false;
        isAutoMergedUnit = false;
        isSummaryLoading: boolean = false; // 泳道缩略图是否加载中
        isTraceLoading: boolean = false;
        type = 'basic' as const;
        pinType = params.pinType ?? 'copied';
        name = params.name;
        tag = params.tag as string | ((session: Session, metadata: unknown) => string | null);
        buttons = params.buttons ?? [];
        description = params.description;
        configBar = params.configBar as ((session: Session, metadata: unknown, onClick?: () => void, isHovered?: boolean, isSelected?: boolean) => JSX.Element | null);
        metadata: T;
        chart = params.chart;
        tabState = params.tabState ?? undefined;
        notifications = params.notifications as Array<(metaData: unknown) => (false | string)>;

        expandable = false;
        isExpanded = false;
        hasExpanded = false;
        isOverlapAnalysisLoading = true;
        isDisplay = true;
        detail = transformDetail(params.detail);
        bottomPanelRender = params.bottomPanelRender;
        collapseAction = params.collapseAction;
        spreadUnits = wrapSpread(params.spreadUnits);
        phase: UnitPhase = 'configuring';
        searchConfig = params.searchConfig;
        collapsible = params.collapsible ?? true;
        isParseLoading: boolean = false; // 是否正在解析
        shouldParse: boolean = false; // 是否需要解析
        progress: number = 0; // 解析进度：实际解析进度
        showProgress: boolean = false; // 解析进度：是否显示进度条
        havePythonFunction: boolean = false; // 是否采集了调用栈信息
        projectType?: ProjectType;
        constructor(metadata: T, projectType?: ProjectType) {
            const excludeAttrs = ['searchConfig', 'parent', 'renderInfo', 'height', 'type', 'configBar', 'projectType', 'hasExpanded'];
            makeAutoObservable(this, { _children: observable.shallow, ...Object.fromEntries(excludeAttrs.map(k => [k, false])) });
            this.metadata = metadata;
            if (projectType !== undefined) {
                this.projectType = projectType;
            }
            this.children = params.spreadUnits ? [] : undefined;
            const spreadUnits = params.spreadUnits;
            if (spreadUnits?.phase === 'create') { spreadUnits.action(this); }
        }

        get children(): InsightUnit[] | undefined {
            if (Array.isArray(this._children)) {
                for (const child of this._children) {
                    child.parent = this;
                }
            }
            return this._children;
        }

        set children(ch: InsightUnit[] | undefined) {
            if (Array.isArray(ch)) {
                for (const child of ch) {
                    child.parent = this;
                }
            }
            this._children = ch;
        }

        renderInfo = (session: Session): (string | JSX.Element | null) => {
            return params.renderInfo?.(session, this.metadata, this) ?? null;
        };

        height = (): number => heightOf(this.chart, this.name);
    };
    return basicUnitClass;
};

export { unitBase as unit };

export interface UnitMatcher {
    target: (ele: InsightUnit) => boolean;
    onSuccess: (ele: InsightUnit) => void;
    showDetail?: boolean;
    tuneToSelectedSlice?: boolean;
};

export type LinkLine = Array<Record<string, unknown>>;
export interface LinkLines {
    [x: string]: LinkLine | undefined;
};

/**
 * @member source the source file defining this template
 */
export interface InsightTemplate {
    id: string;
    name: string;
    source: '<internal>' | string;
    icon?: JSX.Element;
    description: string;
    units: InsightUnitClass[];
    availableUnits: InsightUnitClass[];
    imgColor?: string;
    isNsMode: boolean;
    hasTraceAnalyzing?: boolean;
    buttons?: React.FC<{ session: Session }>;
};

/**
 * Recursive set unit
 *
 * @param unit unit
 * @param unitPhase unitPhase
 */
const MAX_RECURSIVE_COUNT = 10;
export const recursiveSetUnits = (unit: InsightUnit, unitPhase: string, count = 1): void => {
    if (!unit.children || count > MAX_RECURSIVE_COUNT) {
        return;
    }
    for (let index = 0; index < unit.children.length; index++) {
        runInAction(() => {
            if (unit.children !== undefined) {
                unit.children[index].phase = unitPhase;
            }
        });
        recursiveSetUnits(unit.children[index], unitPhase, count + 1);
    }
};

/**
 * Compatible with older version exports that do not include the unit phase attribute.
 * This method is also used in the setting of off-line lane unit phase for real-time recording.
 *
 * @param units session units
 * @param unitPhase unitPhase
 */
export const processUnits = (units: InsightUnit[], unitPhase: string): void => {
    runInAction(() => {
        units.forEach(unit => {
            if (unit.phase === 'error' || unit.phase === 'download') {
                return;
            }
            unit.phase = unitPhase;
            recursiveSetUnits(unit, unitPhase);
        });
    });
};

/**
 * Set the unit phase corresponding to the plugin.
 *
 * @param unit unit
 * @param phase unit phase
 */
export function setUnitPhase(unit: InsightUnit, phase: UnitPhase): void {
    if (unit.phase === 'error') {
        return;
    }
    unit.phase = phase;
    recursiveSetUnits(unit, phase);
}

/**
 * Set the unit phase corresponding to the plugin.
 *
 * @param cardId 待设置状态的cardId
 * @param session session
 * @param phase unit phase
 */
export function setUnitPhaseByCardId(cardId: string, session: Session, phase: UnitPhase): void {
    session.units.forEach(unit => {
        if (unit.metadata.cardId !== cardId) {
            return unit;
        }
        setUnitPhase(unit, phase);
        return unit;
    });
}

/**
 * 设置每个卡的进度条组件是否显示
 * @param unitData
 * @param session
 */
export function setUnitProgressByFileId(unitData: any, session: Session): void {
    const targetUnit = session.units.find(
        (unit) => unit.metadata.cardId === unitData.unit.metadata.cardId,
    );

    if (!targetUnit) return;

    // 更新基本状态
    targetUnit.progress = 100;
    targetUnit.showProgress = false;

    // 延时更新 shouldParse
    scheduleShouldParseUpdate(targetUnit);
}

function scheduleShouldParseUpdate(unit: any): void {
    setTimeout(() => {
        runInAction(() => {
            unit.shouldParse = false; // 设置卡已经解析完成
        });
    }, 300); // 给进度条动画时间
}
