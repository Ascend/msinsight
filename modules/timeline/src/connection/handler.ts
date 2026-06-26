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
import { store } from '../store';
import type { CardMetaData, ThreadMetaData, ThreadTrace, ThreadTraceRequest } from '../entity/data';
import { runInAction } from 'mobx';
import { updateDataSourceAndParentMetaDataMap, recursiveExpandUnit, clearParentMap } from '../insight/units/unitFunc';
import { setUnitPhaseByCardId, setUnitProgressByFileId } from '../entity/insight';
import type { InsightUnit } from '../entity/insight';
import { CardUnit, ROOT_UNIT, ThreadUnit } from '../insight/units/AscendUnit';
import type { ImportCardInfo } from '../components/ImportSelect';
import { Session } from '../entity/session';
import { ImportResult, NotificationHandler } from './defs';
import connector from '../connection/index';
import { message } from 'antd';
import { getTimeOffset, getTimeOffsetKey, setTimeOffsetForUnitTree } from '../insight/units/utils';
import { calculateDomainRange } from '../components/CategorySearch';
import i18n from '@insight/lib/i18n';
import { forEach, groupBy, isEmpty, cloneDeep } from 'lodash';
import { savePageSetting, recoverPageSetting, updatePageSetting } from '../utils/PageSetting';
import { errorCenter, getRankInfoKey, WsError, ErrorCode } from '@insight/lib/utils';
import type { RankInfo } from '../api/interface';
import { queryOneKernel } from '../components/detailViews/Common';
import { TIME_MAKER_DEFAULT } from '../entity/timeMaker';
import { MAX_INITIAL_DURATION } from '../entity/domain';
import { mergeUnitsWhenLoadProject } from '../actions/actionMergeUnits';
import { buildBaselineCardMetadata, type TimelineCard } from './baseline';

const DEFAULT_EXPAND_UNIT_NUMBER = 1;
const MAX_PARSE_SIZE = 32;
const getPropFromData = function <T extends keyof U, U extends Record<string, unknown>>(data: U, key: T): U[T] {
    if (data[key] === undefined) {
        console.warn(`cannot get ${key.toString()} of `, data);
        throw new Error('missed key');
    }
    return data[key];
};
function updateRankDbPathMap(rankList: RankInfo[], dbPath: string, isFtrace?: boolean): void {
    const { sessionStore } = store;
    const session = sessionStore.activeSession;
    runInAction(() => {
        if (!session) {
            return;
        }
        rankList.forEach((rankInfo) => {
            session.rankCardInfoMap.set(getRankInfoKey(rankInfo), { rankInfo, dbPath, isFtrace });
        });
    });
}

/**
 * 更新 dbPath label 到 unit
 * @param unit
 * @param unitData
 */
function updateDbPathAndLabelForCardUnit(unit: InsightUnit, unitData: any): void {
    (unit.metadata as CardMetaData).label = unitData.unit.metadata.cardAlias;
    (unit.metadata as CardMetaData).dbPath = unitData.dbPath;
    unitData.unit.metadata.dbPath = unitData.dbPath;
}

const setObserveAction = (session: Session): void => {
    let start = 0;
    function next(): void {
        const end = Math.min(start + MAX_PARSE_SIZE, session.parseQueue.length);
        for (let i = start; i < end; i++) {
            session.parseQueue[i]?.();
        }
        start = end;
        if (start < session.parseQueue.length) {
            setTimeout(next, 0);
        } else {
            completeAction(session);
        }
    }
    next();
};

const completeAction = (session: Session): void => {
    session.parseQueue = [];
    clearParentMap();
};

/**
 * 处理解析成功的通知，更新会话状态和相关数据。
 *
 * @param data - 包含解析结果的数据对象
 * @returns void
 */
export const parseSuccessHandler: NotificationHandler = (data): void => {
    try {
        // 将数据转换为 any 类型
        const unitData = data as any;
        // 从数据中获取数据源
        const dataSource = getPropFromData(data, 'dataSource') as DataSource;
        // 从 store 中获取 sessionStore
        const { sessionStore } = store;
        // 获取当前活跃的会话
        const session = sessionStore.activeSession;
        // 如果没有活跃的会话，直接返回
        if (!session) { return; }
        // 第一次 parse/success 返回时，更新 isRL 字段
        session.rankCardInfoMap.size === 0 && connector.send({ event: 'updateSession', body: { isRL: data.isRl } });
        const isFtrace = unitData.isFtrace === true;
        // 更新排名数据库路径映射
        updateRankDbPathMap(unitData.rankList ?? [], unitData.dbPath, isFtrace);
        runInAction(() => {
            // 更新会话的 isFullDb 和 startTime 属性
            session.isFullDb = unitData.isFullDb;
            session.startTime = unitData.startTime;
            if (isFtrace) {
                session.hasFtraceData = true;
            } else {
                session.hasNonFtraceData = true;
            }
            // 判断是否为全局解析模式
            const isGlobal = session.modeOfParse === 'global_parse';
            // parse success之后关闭进度条
            setUnitProgressByFileId(unitData, session);
            session.units.forEach((unit) => {
                // 如果 unit 的 cardId 与 unitData 的 cardId 匹配
                if ((unit.metadata as CardMetaData).cardId === unitData.unit.metadata.cardId) {
                    // 更新数据库路径和标签
                    updateDbPathAndLabelForCardUnit(unit, unitData);
                    (unit.metadata as CardMetaData).isFtrace = isFtrace;
                    // 更新对齐开始时间戳
                    unit.alignStartTimestamp = unitData.offset as number;
                    const nextOffset = { ...session.unitsConfig.offsetConfig.timestampOffset };
                    setTimeOffsetForUnitTree(session, unitData.unit, unit.alignStartTimestamp, nextOffset);
                    // 如果 unitData 的 children 不为空
                    if (unitData.unit.children !== undefined && unitData.unit.children.length > 0) {
                        for (const item of unitData.unit.children) {
                            // 获取时间偏移键
                            const key = getTimeOffsetKey(session, item.metadata);
                            // 更新子项的对齐开始时间戳
                            item.alignStartTimestamp = unit.alignStartTimestamp;
                            session.unitsConfig.offsetConfig.timestampOffset[key] = unit.alignStartTimestamp;
                        }
                    }
                    // 更新时间戳偏移配置
                    session.unitsConfig.offsetConfig.timestampOffset = nextOffset;
                    // 更新数据源和父元数据映射
                    updateDataSourceAndParentMetaDataMap(unitData.unit, (unit.metadata as CardMetaData).dataSource, !isGlobal);
                    // 根据是否为全局解析模式，决定是否将解析任务推入队列
                    const expandParsedUnits = (): void => {
                        recursiveExpandUnit(unitData.unit.children ?? [], unit, 0);
                        const expandedOffset = { ...session.unitsConfig.offsetConfig.timestampOffset };
                        setTimeOffsetForUnitTree(session, unit, unit.alignStartTimestamp, expandedOffset);
                        session.unitsConfig.offsetConfig.timestampOffset = expandedOffset;
                    };
                    isGlobal ? session.parseQueue.push(expandParsedUnits) : expandParsedUnits();
                }
            });
            // 重置记录时间
            session.startRecordTime = 0;
            // 计算默认结束时间  如果时间超出 MAX_SAFE_INTEGER , 会取 MAX_SAFE_INTEGER *1.1（*1.1是为了给右侧一些空间） 为最大值
            const defaultEndTimeAll = (typeof unitData.maxTimeStamp === 'number' ? Math.min(Number.MAX_SAFE_INTEGER, unitData.maxTimeStamp) : 1000000000) * 1.1;
            // 如果 defaultEndTimeAll 等于最大值（MAX_SAFE_INTEGER *2） 给出提示
            if (defaultEndTimeAll === Number.MAX_SAFE_INTEGER * 1.1) {
                session.isOverflowMaxSafeNumber = true;
            }
            // 更新会话的 endTimeAll 属性
            session.endTimeAll = session.endTimeAll === undefined ? defaultEndTimeAll : Math.max(session.endTimeAll, defaultEndTimeAll);
            // 更新远程属性
            const remoteAttrs = session.remoteAttrs.get(dataSource.remote);
            if (remoteAttrs === undefined) {
                session.remoteAttrs.set(dataSource.remote, { maxTimeStamp: defaultEndTimeAll });
            } else {
                remoteAttrs.maxTimeStamp = defaultEndTimeAll;
            }
            // 设置单元阶段为下载
            setUnitPhaseByCardId(unitData.unit.metadata.cardId, session, 'download');
            // 如果 startTimeUpdated 为 true，清除缓存
            if (unitData.startTimeUpdated === true) {
                session.simpleCache.clear();
            }
            // 设置会话的域范围
            let domainEnd = session.endTimeAll ?? session.domain.defaultDuration;
            if (domainEnd > MAX_INITIAL_DURATION) {
                domainEnd = MAX_INITIAL_DURATION;
            }
            session.setDomainWithoutHistory({ domainStart: 0, domainEnd });
            // 设置各卡的默认合并泳道
            session.mergedThreadData.concatThreadGroupList(unitData.threadGroupList);

            // 检查所有卡是否解析完成
            const parseCompleted = !(session.units.find(item => item.phase === 'analyzing'));
            if (parseCompleted) {
                // 如果存在超出最大值的情况添加message解释
                if (session.isOverflowMaxSafeNumber) {
                    message.warning(i18n.t('timeline:InterceptedMaximum'));
                    session.isOverflowMaxSafeNumber = false;
                }
                // 如果是全局解析模式，设置观察动作
                isGlobal && setObserveAction(session);
                // 发送更新会话通知
                connector.send({ event: 'updateSession', body: { parseCompleted, isFullDb: session.isFullDb } });
                // 恢复上次页面设置，包括历史合并泳道
                recoverPageSetting();
                // 初始化合并泳道
                mergeUnitsWhenLoadProject(session);
            }
        });
    } catch (error) {
        console.error(error);
    }
};

export const parseProgressHandler: NotificationHandler = (data): void => {
    try {
        const unitData = data as any;
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        runInAction(() => {
            if (!session) {
                return;
            }
            session.units.forEach((unit) => {
                if ((unit.metadata as CardMetaData).cardId === unitData.fileId) {
                    unit.progress = unitData.progress;
                    unit.showProgress = true;
                }
            });
        });
    } catch (error) {
        console.error(error);
    }
};
export const parseFailHandler: NotificationHandler = (data): void => {
    const { sessionStore } = store;
    const session = sessionStore.activeSession;
    runInAction(() => {
        if (!session) {
            return;
        }
        setUnitPhaseByCardId((data as any).rankId, session, 'error');
    });
    errorCenter.handleError(new WsError(ErrorCode.PARSE_FAIL, data.error as string));
};

const getRootUnit = (session: Session, host: string, dataSource: DataSource): InsightUnit | undefined => {
    if (isEmpty(host)) {
        return undefined;
    }
    const insightUnit = session.units.find(unit => (unit.metadata as CardMetaData)?.cardId.includes(host))?.parent;
    return insightUnit !== undefined ? insightUnit : new ROOT_UNIT({ dataSource, host });
};

const initUnitSessionInfo = (session: Session, result: ImportResult, dataSource: DataSource): void => {
    session.projectName = dataSource.projectName;
    session.phase = 'download';
    session.endTimeAll = undefined;
    session.isPending = result.isPending as boolean;
    session.isParserLoading = !(result.isPending as boolean);
    session.isSimulation = result.isSimulation;
    session.isIE = result.isIE;
    session.isIpynb = result.isIpynb;
    session.isCluster = result.isCluster;
    session.isMultiCluster = new Set(result.result.map(({ cluster }) => cluster)).size > 1;
    session.isMultiDevice = result.isMultiDevice;
};

const resetImportedDataTypeState = (session: Session, result: ImportResult, isNeedResetRankId: boolean): void => {
    if (result.reset || isNeedResetRankId) {
        session.hasFtraceData = false;
        session.hasNonFtraceData = false;
    }
};

const initUnitInfo = (session: Session | undefined, result: ImportResult, dataSource: DataSource, isNeedResetRankId: boolean): void => {
    if (!session) { return; }
    if (result.reset as boolean) { resetPage({ dataSource }); }
    resetImportedDataTypeState(session, result, isNeedResetRankId);
    if (!session.units.length) {
        session.threadsToFetch.clear();
    }
    initUnitSessionInfo(session, result, dataSource);
    // 更新 units
    const hostInfo = groupBy(result.result, (item: ImportCardInfo) => item.host ?? '');
    forEach(hostInfo, (cards, host) => {
        const unit = getRootUnit(session, host, dataSource);
        const cardUnits: InsightUnit[] = [];
        if (unit?.children !== undefined) {
            cardUnits.push(...unit.children);
        }
        forEach(cards, (item: ImportCardInfo) => {
            const oldUnit = session.units.find(unitItem => (unitItem.metadata as CardMetaData)?.cardId === item.rankId);
            if (oldUnit !== undefined) { return; }
            const curDataSource = cloneDeep(dataSource);
            curDataSource.dataPath = item.dataPathList;
            const { rankId, dbPath, cluster, cardName, cardPath } = item;
            const cardUnit = new CardUnit({
                dataSource: curDataSource,
                cardId: rankId,
                dbPath,
                cluster,
                cardName,
                cardPath,
            }, item.projectType);
            if (item.result as boolean) {
                cardUnit.isParseLoading = !(result.isPending as boolean);
                cardUnit.shouldParse = item.cardName !== 'Host';
                cardUnit.phase = 'analyzing';
                cardUnit.progress = 0;
                cardUnit.showProgress = true;
            } else {
                cardUnit.phase = 'error';
            }
            cardUnits.push(cardUnit);
            session.units = session.units.concat([cardUnit]);
            if (isNeedResetRankId) {
                session.rankCardInfoMap.clear();
            }
        });
        if (unit) {
            unit.isExpanded = cardUnits.length > 0 ? cardUnits[0].isExpanded : false;
            unit.children = cardUnits;
        }
    });
    session.mergedThreadData.clear(); // units 完全刷新，清空合并数据

    if (session?.units?.[0]) {
        session.units[0].isExpanded = true;
        const rootUnit = getRootUnit(session, session.units[0].metadata.cardId as string, dataSource);
        if (rootUnit) {
            rootUnit.isExpanded = true;
        }
    }
    session.updateUnitsForMultiDevice();
};

export const createBaselineCard = (session: Session | undefined, result: TimelineCard[], dataSource: DataSource): void => {
    if (!session) { return; }
    const singleDataPath = dataSource.dataPath[0];
    const isSamePath = session?.units.some((unit) => {
        const metadata = unit.metadata as any;
        if ((metadata.dataSource.dataPath as string[]) === undefined) {
            return false;
        }
        // metadata.dbPath===singleDataPath的判断仅用于npumonitor场景
        return metadata.dataSource.remote !== dataSource.remote || (metadata.dataSource.dataPath as string[]).includes(singleDataPath) || metadata.dbPath === singleDataPath;
    });
    if (isSamePath) { return; }
    session.phase = 'download';
    const hostInfo = groupBy(result, (item: TimelineCard) => item.host ?? '');
    forEach(hostInfo, (cards, host) => {
        const unit = isEmpty(host) ? undefined : new ROOT_UNIT({ dataSource, host });
        const cardUnits: InsightUnit[] = [];
        forEach(cards, (item: TimelineCard) => {
            const cardUnit = new CardUnit(buildBaselineCardMetadata(item, dataSource));
            if (item.result as boolean) {
                cardUnit.isParseLoading = true;
                cardUnit.shouldParse = true;
                cardUnit.phase = 'analyzing';
                cardUnit.progress = 0;
                cardUnit.showProgress = true;
            } else {
                cardUnit.phase = 'error';
            }
            if (session.units.length < DEFAULT_EXPAND_UNIT_NUMBER) {
                cardUnit.isExpanded = true;
            }
            cardUnits.push(cardUnit);
            session.units.push(cardUnit);
        });
        if (unit) {
            unit.isExpanded = cardUnits.length > 0 ? cardUnits[0].isExpanded : false;
            unit.children = cardUnits;
        }
    });
    session.updateUnitsForMultiDevice();
    session.baselineCardInfo = result[0];
};
export const savePageSettingRemoteHandler: NotificationHandler = async (): Promise<unknown> => {
    const { sessionStore } = store;
    const session = sessionStore.activeSession;
    if (!session) {
        return;
    }
    // 导入新Remote前，保存当前页面
    // 所有卡解析完成
    const parseCompleted = !(session.units.some(item => item.phase !== 'download'));
    if (parseCompleted) {
        savePageSetting();
    }
};

export const parseUnitCompletedHandler: NotificationHandler = async (data): Promise<void> => {
    const session = store.sessionStore.activeSession as Session;
    const dbId = data.dbId as string;
    if (dbId && !session.asyncDataLoadingList[dbId]) {
        session.asyncDataLoadingList[dbId] = {
            WAIT_TIME: false,
            OVERLAP_ANALYSIS: false,
            CONNECTION_CATEGORY: false,
        };
    }

    switch (data.unitName) {
        case 'WAIT_TIME':
            session.asyncDataLoadingList[dbId].WAIT_TIME = true;
            // 发送更新等待时间消息，更新所有表格
            connector.send({
                event: 'updateAllTable',
                body: { data },
                to: 'Operator',
            });
            break;
        case 'OVERLAP_ANALYSIS':
            session.asyncDataLoadingList[dbId].OVERLAP_ANALYSIS = true;
            // 发送更新泳道loading消息
            connector.send({
                event: 'updateAnalysisLoading',
                body: { data },
                to: 'Timeline',
            });

            // 发送更新泳道内容消息
            connector.send({
                event: 'updateAnalysisData',
                body: { data },
                to: 'Timeline',
            });

            // 发送更新综合指标消息
            connector.send({
                event: 'OverallMetrics',
                body: { data },
                to: 'Timeline',
            });
            break;
        default:
            // 发送更新连线消息
            session.asyncDataLoadingList[dbId].CONNECTION_CATEGORY = true;
            connector.send({
                event: 'updateCategory',
                body: { data: false },
                to: 'Timeline',
            });
            break;
    }
};

export const importRemoteHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const dataSource = getPropFromData(data, 'dataSource') as DataSource;
        const result = getPropFromData(data, 'importResult') as ImportResult;
        const isNeedResetRankId = getPropFromData(data, 'switchProject') as boolean;
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        if (!session) {
            return;
        }
        session.isNeedResetRankId = isNeedResetRankId;
        runInAction(() => {
            initUnitInfo(session, result, dataSource, isNeedResetRankId);
            clearTimeMarkerFlags(session);
        });
        sendSessionUpdate(result, session);
        connector.send({
            event: 'updateCategory',
            body: { data: true },
            to: 'Timeline',
        });
    } catch (error) {
        console.error(error);
    }
};

/**
 * 处理基线添加通知的异步函数
 * @param data - 包含数据源和基线信息的数据对象
 * @returns void
 */
export const baselineAddHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const dataSource = getPropFromData(data, 'dataSource') as DataSource;
        const baseLineInfo = getPropFromData(data, 'baseLine') as TimelineCard[];
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        runInAction(() => {
            createBaselineCard(session, baseLineInfo, dataSource);
        });
    } catch (error) {
        console.error(error);
    }
};

/**
 * 发送会话更新信息的函数
 * @param result - 包含更新信息的对象
 * @param session - 当前会话对象
 * @returns void
 */
const sendSessionUpdate = (result: any, session: any): void => {
    connector.send({
        event: 'updateSession',
        body: {
            isCluster: result.isCluster,
            isReset: result.reset,
            isIpynb: result.isIpynb,
            ipynbUrl: '',
            startTime: 0,
            endTimeAll: session?.endTimeAll,
            unitcount: result.result?.length ?? 0,
            isBinary: result.isBinary,
            coreList: result.coreList,
            sourceList: result.sourceList,
        },
    });
};

/**
 * 清除Ipynb Notebook相关信息的函数
 * @param session - 当前会话对象
 * @returns 无返回值。
 */
const clearIpynbInfo = (session: Session): void => {
    session.isIpynb = false;
    session.ipynbUrl = '';
    connector.send({
        event: 'updateSession',
        body: { isIpynb: session.isIpynb, ipynbUrl: session.ipynbUrl },
    });
};

export const removeRemoteHandler: NotificationHandler = async (data): Promise<void> => {
    resetPage(data);
    updatePageSetting({ type: 'removeDataSource', data: getPropFromData(data, 'dataSource') });
};

export const resetRemoteHandler: NotificationHandler = async (): Promise<void> => {
    resetPage();
    updatePageSetting({ type: 'reset' });
};

/**
 * 清除会话中的泳道信息的函数
 * @param session
 * @param data
 * @returns 无返回值
 */
const clearUnits = (session: Session, data?: Record<string, unknown>): void => {
    let dataSource: DataSource | null = null;
    if (data) {
        dataSource = getPropFromData(data, 'dataSource') as DataSource;
    }
    if (dataSource !== null && dataSource !== undefined) {
        const removeUnits = session.pinnedUnits.concat(session.units).filter((unit) => {
            const metadata = unit.metadata as any;
            if (metadata.dataSource.dataPath === undefined) {
                return true;
            }
            const isSameDataPath = dataSource?.dataPath?.filter((item) => metadata.dataSource.dataPath.includes(item)).length !== 0;
            return metadata.dataSource.remote === dataSource?.remote && isSameDataPath;
        });
        for (const unit of removeUnits) {
            const metadata = unit.metadata as any;
            session.remoteAttrs.delete(metadata.dataSource.remote);
        }
        session.units = session?.units.filter((unit) => {
            const metadata = unit.metadata as any;
            return metadata.dataSource.remote !== dataSource?.remote && !removeUnits.includes(unit);
        });
        session.pinnedUnits = session?.pinnedUnits.filter((unit) => {
            const metadata = unit.metadata as any;
            return metadata.dataSource.remote !== dataSource?.remote && !removeUnits.includes(unit);
        });
    } else {
        session.remoteAttrs.clear();
        session.units = [];
        session.pinnedUnits = [];
        session.mergedThreadData.clear(); // 当 session.units 清空时，触发 clear
    }
    if (session.isNeedResetRankId) {
        session.rankCardInfoMap.clear();
    }
};

/**
 * 重置页面的函数。
 * @param data - 可选的数据对象，包含数据源信息
 * @returns void
 */
const resetPage = (data?: Record<string, unknown>): void => {
    runInAction(() => {
        const session = store.sessionStore.activeSession;
        if (!session) {
            return;
        }
        session.isMultiDevice = false;
        session.isFullDb = false;
        session.hasFtraceData = false;
        session.hasNonFtraceData = false;
        session.isNeedResetRankId = true;
        clearUnits(session, data);
        session.simpleCache.clear();
        let remoteMaxTimeStamps = 0;
        session.remoteAttrs.forEach((attrs) => {
            remoteMaxTimeStamps = Math.max(<number>attrs.maxTimeStamp, remoteMaxTimeStamps);
        });
        session.endTimeAll = remoteMaxTimeStamps;
        if (session.selectedUnits[0] !== undefined && !session.units.includes(session.selectedUnits[0])) {
            session.selectedUnits = [];
        }
        if (session.eventUnits[0] !== undefined) {
            session.eventUnits = [];
        }
        if (session.units.length === 0) {
            session.selectedRange = undefined;
        }
        clearIpynbInfo(session);
        clearTimeMarkerFlags(session);
        resetSession();
    });
};

/**
 * 重置会话的函数
 * @returns void
 */
function resetSession(): void {
    runInAction(() => {
        const defaultSessionData = new Session();
        const session = store.sessionStore.activeSession as Session;
        Object.assign(session, {
            doReset: !session.doReset,
            expandedUnitKeys: [],
            contextMenu: defaultSessionData.contextMenu,
            unitsConfig: defaultSessionData.unitsConfig,
            scrollTop: 0,
            selectedDetailKeys: [],
            selectedDetails: [],
            searchData: undefined,
            linkLines: {},
        });
        session.clearSelectionLock();
        session.singleLinkLine = {};
        session.isTimeAnalysisMode = false;
        session.timeAnalysisRange = undefined;
        // 因为时间范围分析利用了M键的竖向遮罩，所以移除时需要清空
        session.mKeyRender = false;
        session.mMaskRange = [];
        session.ridLineType = '';
        session.drawLineMode = 'all';
    });
}

const clearTimeMarkerFlags = (session: Session): void => {
    session.timelineMaker = {
        ...TIME_MAKER_DEFAULT,
        refreshTrigger: (session.timelineMaker.refreshTrigger + 1) % 10,
    };
};

/**
 * 执行远程删除请求，处理卡片信息并更新会话属性, async异步函数
 *
 * @param session - 当前会话对象，用于管理远程属性和时间戳。
 * @param dataSource - 数据源对象，用于发送请求。
 * @param removeCardInfos - 要删除的卡片信息数组，每个元素包含卡片ID和数据库路径。
 * @returns void
 */
const remoteDeleteRequest = async (session: Session, dataSource: DataSource, removeCardInfos: Array<{ cardId: string; dbPath: string }>): Promise<void> => {
    const [removeCardIds, removeCardDbPaths] = removeCardInfos.reduce((acc, { cardId, dbPath }) => {
        acc[0].push(cardId);
        acc[1].push(dbPath);
        return acc;
    }, [[] as string[], [] as string[]]);
    const result = await window.request(dataSource, { command: 'remote/delete', params: { rankId: removeCardIds, dbPaths: removeCardDbPaths } });
    if (result?.startTimeUpdated as boolean) {
        session.remoteAttrs.set(dataSource.remote, { maxTimeStamp: result.maxTimeStamp });

        let remoteMaxTimeStamps = 0;
        session.remoteAttrs.forEach((attrs) => {
            remoteMaxTimeStamps = Math.max(<number>attrs.maxTimeStamp, remoteMaxTimeStamps);
        });
        session.endTimeAll = remoteMaxTimeStamps;
    }
};

/**
 * 处理移除基线数据的函数
 * @param data - 包含数据源和单个数据路径的对象
 * @returns Promise<void>
 */
export const removeBaselineHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        runInAction(() => {
            const dataSource = getPropFromData(data, 'dataSource') as DataSource;
            const singleDataPath = getPropFromData(data, 'singleDataPath') as string;
            const session = store.sessionStore.activeSession as Session;
            const removeUnits = getRemoveUnits(session, dataSource, singleDataPath);
            session.units = session?.units.filter((unit) => {
                const metadata = unit.metadata as any;
                if (!((metadata.cardName.startsWith('Baseline')) as boolean)) {
                    return true;
                }
                if ((metadata.dataSource.dataPath as string[]) === undefined) {
                    return false;
                }
                return metadata.dataSource.remote !== dataSource.remote || !(metadata.dataSource.dataPath as string[]).includes(singleDataPath);
            });
            if (session.selectedUnits[0] !== undefined && !session.units.includes(session.selectedUnits[0])) {
                session.selectedUnits = [];
            }
            if (session.units.length === 0) {
                session.selectedRange = undefined;
            }
            if (session.eventUnits[0] !== undefined) {
                session.eventUnits = [];
            }
            session.doReset = !session.doReset;
            for (const unit of removeUnits) {
                const metadata = unit.metadata as any;
                const remote = metadata.dataSource.remote;
                if (session.remoteAttrs.has(remote) && !session.units.find(item => (item.metadata as any)?.dataSource.remote === remote)) {
                    session.remoteAttrs.delete(remote);
                }
            }
            session.pinnedUnits = session?.pinnedUnits.filter((unit) => {
                const metadata = unit.metadata as any;
                return metadata.dataSource.remote !== dataSource.remote || !(metadata.dataSource.dataPath as string[]).includes(singleDataPath);
            });
            clearTimeMarkerFlags(session);
            // 取消对比时，删除底部面板Rank ID下拉框对比卡的数据选项
            for (const [key] of session.rankCardInfoMap) {
                // text数据比对情形，基线卡显式含有"Baseline"
                if (!session.isFullDb) {
                    if (key.includes('_Baseline')) {
                        session.rankCardInfoMap.delete(key);
                    }
                }
                // db数据比对情形，基线卡的信息保存在baselineCardInfo中
                // 仅跨工程比对会进入以下逻辑，同工程比对timeline不会新增baseline卡，因此baselineCardInfo值为null
                if (session.isFullDb && session.baselineCardInfo) {
                    const { cardName, host } = session.baselineCardInfo;
                    if (cardName?.includes('Baseline') && key.includes(host)) {
                        session.rankCardInfoMap.delete(key);
                    }
                }
            }
            session.baselineCardInfo = null;
        });
    } catch (error) {
        console.error(error);
    }
};

/**
 * 处理移除单个远程数据的函数
 * @param data - 包含数据源和单个数据路径的对象
 * @returns Promise<void>
 */
export const removeSingleRemoteHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const dataSource = getPropFromData(data, 'dataSource') as DataSource;
        const singleDataPath = getPropFromData(data, 'singleDataPath') as string;
        const session = store.sessionStore.activeSession as Session;
        const removeUnits = getRemoveUnits(session, dataSource, singleDataPath);
        const removeCardInfos = getRemoveCardInfos(removeUnits);
        session.units = session?.units.filter((unit) => {
            const metadata = unit.metadata as any;
            if ((metadata.dataSource.dataPath as string[]) === undefined) {
                return true;
            }
            return metadata.dataSource.remote !== dataSource.remote || !(metadata.dataSource.dataPath as string[]).includes(singleDataPath);
        });
        if (session.selectedUnits[0] !== undefined && !session.units.includes(session.selectedUnits[0])) {
            session.selectedUnits = [];
        }
        if (session.units.length === 0) {
            session.selectedRange = undefined;
        }
        if (session.eventUnits[0] !== undefined) {
            session.eventUnits = [];
        }
        session.doReset = !session.doReset;
        for (const unit of removeUnits) {
            const metadata = unit.metadata as any;
            const remote = metadata.dataSource.remote;
            if (session.remoteAttrs.has(remote) && !session.units.find(item => (item.metadata as any)?.dataSource.remote === remote)) {
                session.remoteAttrs.delete(remote);
            }
            session.deleteRankCardInfoMapItemByDbPath(metadata.dbPath);
        }
        session.pinnedUnits = session?.pinnedUnits.filter((unit) => {
            const metadata = unit.metadata as any;
            return metadata.dataSource.remote !== dataSource.remote || !(metadata.dataSource.dataPath as string[]).includes(singleDataPath);
        });
        await remoteDeleteRequest(session, dataSource, removeCardInfos);
        clearTimeMarkerFlags(session);
        clearIpynbInfo(session);
        connector.send({ event: 'deleteCard', body: { info: removeCardInfos } });
        // 更新已保存的页面设置
        updatePageSetting({ type: 'removeSingleDataPath', data });
    } catch (error) {
        console.error(error);
    }
    connector.send({ event: 'updateSession', body: { loading: false } });
};

const getRemoveUnits = (session: Session, dataSource: DataSource, singleDataPath: string): InsightUnit[] => {
    return session.units.filter((unit) => {
        const metadata = unit.metadata as any;
        if ((metadata.dataSource.dataPath as string[]) === undefined) {
            return true;
        }
        const isSameDataPath = metadata.dataSource.dataPath.includes(singleDataPath);
        return metadata.dataSource.remote === dataSource.remote && isSameDataPath;
    });
};

const getRemoveCardInfos = (removeUnits: any[]): Array<{ cardId: string; dbPath: string }> => {
    return removeUnits.map((unit) => {
        const metadata = unit.metadata as any;
        return { cardId: metadata.cardId, dbPath: metadata.dbPath };
    });
};

/**
 * 设置主题的处理函数。
 * @param data - 包含主题信息的对象，其中 `isDark` 表示是否为暗黑模式。
 * @returns 无返回值。
 */
export const setTheme: NotificationHandler = (data): void => {
    window.setTheme(Boolean(data.isDark)); // 将 `isDark` 转换为布尔值并设置主题
};

/**
 * 集群完成处理函数。
 * @param data - 包含集群解析结果和路径的对象。
 * @returns 无返回值。
 */
export const clusterCompletedHandler: NotificationHandler = (data): void => {
    // 检查解析结果是否为 'ok' 或 'none'
    const clusterRes = (data?.parseResult === 'ok' || data?.parseResult === 'none');
    // 获取集群路径，若不存在则设为空字符串
    const clusterPath = data?.clusterPath as string ?? '';
    // 获取是否显示集群的标志
    const isShowCluster = data?.isShowCluster as boolean;
    const session = store.sessionStore.activeSession as Session;
    // 更新会话中的集群显示状态
    session.isCluster = isShowCluster;
    // 判断集群是否完成
    const clusterCompleted = isShowCluster && clusterRes;
    connector.send({ // 发送集群完成事件
        event: 'frame:parseClusterCompleted',
        body: { isCluster: clusterCompleted, clusterPath },
    });
};

/**
 * 集群持续时间完成处理函数。
 * @param data - 包含集群解析结果和路径的对象。
 * @returns void
 */
export const clusterDurationCompletedHandler: NotificationHandler = (data): void => {
    const clusterRes = data?.parseResult === 'ok';
    // 获取集群路径，若不存在则设为空字符串
    const clusterPath = data?.clusterPath as string ?? '';
    // 发送集群持续时间完成事件
    connector.send({
        event: 'frame:parseClusterDurationCompleted',
        body: { isCluster: clusterRes, clusterPath },
    });
};

/**
 * 查找块的处理函数。
 * @param data - 包含块信息的对象，包括 `rankId`, `dbPath`, `name`, `startTime` 等。
 * @returns void
 */
export const findBlock: NotificationHandler = (data): void => {
    try {
        // 查询内核
        queryOneKernel({
            rankId: data.rankId as string,
            dbPath: data.rankId as string,
            name: data.name as string,
            timestamp: data.startTime as number,
            duration: 0,
        }).then(res => {
            const temp = res as Record<string, any>; // 将结果转换为记录类型
            const input = {
                id: temp.id as string,
                processId: temp.pid as string,
                depth: temp.depth as number,
                rankId: temp.rankId as string,
                threadId: temp.threadId as string,
                name: data.name as string,
                startTime: parseInt(data.startTime as string),
                duration: temp.duration,
            };
            locateUnitHandler(input); // 调用定位单元处理函数
        });
    } catch (err) {
        console.error(err);
    }
};

/**
 * 定位单元处理函数。
 * @param data - 包含定位信息的对象
 * @returns void
 */
export const locateUnitHandler: NotificationHandler = (data): void => {
    const { sessionStore } = store; // 获取会话存储
    const session = sessionStore.activeSession as Session; // 获取当前会话
    const slice = data as ThreadTrace; // 将数据转换为线程追踪类型
    runInAction(() => { // 在动作中执行
        session.locateUnit = { // 设置定位单元
            target: (unit: InsightUnit): boolean => { // 定位目标
                return unit instanceof ThreadUnit && (Boolean(unit.metadata.cardId.includes(slice.rankId))) &&
                    unit.metadata.processId === slice.processId && unit.metadata.threadId === slice.threadId;
            },
            onSuccess: (unit): void => { // 定位成功后的操作
                const startTime = slice.startTime - getTimeOffset(session, unit.metadata as ThreadMetaData); // 计算开始时间
                const [rangeStart, rangeEnd] = calculateDomainRange(session, startTime, slice.duration); // 计算域范围
                session.domainRange = { domainStart: rangeStart, domainEnd: rangeEnd }; // 更新域范围
                session.selectedData = { // 更新选中数据
                    id: slice.id,
                    name: slice.name,
                    startTime,
                    duration: slice.duration,
                    depth: slice.depth,
                    threadId: slice.threadId,
                    processId: slice.processId as string,
                    cardId: slice.rankId as string,
                    metaType: (unit.metadata as ThreadMetaData).metaType,
                    showSelectedData: (slice.showSelectedData as boolean) ?? false,
                };
            },
            showDetail: true, // 显示详细信息
        };
    });
};

/**
 * 切换语言处理函数。
 * @param data - 包含语言信息的对象，其中 `lang` 表示语言类型。
 * @returns void。
 */
export const switchLanguageHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    const lang = data.lang as 'zhCN' | 'enUS';
    if (session) {
        runInAction(() => {
            session.language = lang;
        });
    }
    i18n.changeLanguage(lang);
};

/**
 * 卡片偏移接口，包含卡片 ID 和偏移量。
 */
interface CardOffset {
    cardId: string;
    offset: number;
}

/**
 * 所有成功处理函数。
 * @param data - 包含卡片偏移信息和解析状态的对象。
 * @returns 无返回值。
 */
export const allSuccessHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        // 获取卡片偏移信息
        const cardOffSets = data.cardOffsets as CardOffset[];
        // 创建偏移量映射
        const offsetMap: Map<string, number> = new Map();
        // 遍历卡片偏移信息
        cardOffSets.forEach((value) => {
            // 将偏移量存入映射
            offsetMap.set(value.cardId, value.offset);
        });
        runInAction(() => {
            if (!session) {
                return;
            }
            if (data.isAllPageParsed as boolean) {
                session.isPending = false;
                session.isParserLoading = false;
                session.startTime = data.minTime as string;
            }
            session.units.forEach((unit) => {
                unit.alignStartTimestamp = offsetMap.get((unit.metadata as CardMetaData).cardId);
                const prevObj = session.unitsConfig.offsetConfig.timestampOffset;
                if (unit.alignStartTimestamp !== undefined) {
                    setTimeOffsetForUnitTree(session, unit, unit.alignStartTimestamp, prevObj);
                    if (unit.children !== undefined && unit.children.length > 0) {
                        for (const item of unit.children) {
                            const key = getTimeOffsetKey(session, item.metadata as unknown as ThreadTraceRequest);
                            item.alignStartTimestamp = unit.alignStartTimestamp;
                            session.unitsConfig.offsetConfig.timestampOffset[key] = unit.alignStartTimestamp;
                        }
                    }
                    session.unitsConfig.offsetConfig.timestampOffset = { ...prevObj, [(unit.metadata as CardMetaData).cardId]: (unit.alignStartTimestamp) };
                }
            });
            session.updateEndTimeAll();
        });
    } catch (error) {
        console.error(error);
    }
};

/**
 * 更新项目名称处理函数。
 * @param data - 包含旧项目名称和新项目名称的对象。
 * @returns 无返回值。
 */
export const updateProjectNameHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    if (session === undefined) {
        return;
    }
    const { oldProjectName, newProjectName } = (data ?? {}) as Record<string, string>;
    if (session.projectName === oldProjectName) {
        session.projectName = newProjectName;
    }
    updatePageSetting({ type: 'updateProjectName', data });
};

/**
 * 获取timeline模块的rangeFlagList，通过触发事件返回。
 * @returns 无返回值。
 */
export const getTimelineRangeFlagListHandler: NotificationHandler = (): void => {
    const session = store.sessionStore.activeSession;
    if (session === undefined) {
        return;
    }
    connector.send({
        event: 'updateRangeFlagList',
        to: 'Memory',
        body: {
            timelineFlagList: session.timelineMaker.timelineFlagList.filter(item => item.type === 'range'),
        },
    });
};

/**
 * 通过key获取timeline模块的rank偏移量，通过触发事件返回。
 * @param data.offsetKey 需要获取的偏移量的key。
 * @returns 无返回值。
 */
export const getTimelineOffsetByKeyHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    if (session === undefined) {
        return;
    }

    connector.send({
        event: 'updateTimelineOffset',
        to: 'Memory',
        body: {
            offset: session.unitsConfig.offsetConfig.timestampOffset[(data as any).offsetKey],
        },
    });
};
