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
// #region cputime
export interface ThreadInfo {
    tid: number;
    name: string;
};

export interface FrameProcessInfo {
    processId: number;
    processName: string;
    tag: boolean;
};

export interface UserTraceSliceList {
    name: string;
    timestamp: number;
    duration: number;
    cookie: number;
    depth: number;
    taskName: string;
};

export interface UserTraceMetadata {
    taskNameList: string[];
    maxDepthList: number[];
    pidList: number[];
};

export interface UserTraceList {
    id: number;
    task: string;
    startTime: number;
    endTime: number;
    duration: number;
};

export interface UserTraceStatistics {
    taskName: string;
    occurrences: number;
    wallDuration: number;
    avgDuration: number;
    maxDuration: number;
    minDuration: number;
};

export interface ProcessLaneCpuUsage {
    duration: number;
    startTime: number;
};

export interface CpuMultiSliceProcessDetail {
    name: string;
    id: number;
    type: 'PROCESS' | 'THREAD';
    duration: number;
    percentage: number;
    avgDuration: number;
    occurrences: number;
    children: CpuMultiSliceProcessDetail[];
};

export interface CpuStateProcessThreadDetail {
    avgDuration: number;
    type: 'PROCESS' | 'THREAD' | 'STATE';
    children?: CpuStateProcessThreadDetail[];
    duration: number;
    id: number;
    maxDuration: number;
    minDuration: number;
    name: string;
    occurrences: number;
};

export interface CpuProcessThreadStateDetail {
    avgDuration: number;
    type: 'PROCESS' | 'THREAD' | 'STATE';
    children?: CpuProcessThreadStateDetail[];
    duration: number;
    id: number;
    maxDuration: number;
    minDuration: number;
    name: string;
    occurrences: number;
};

export interface CpuMultiSliceList {
    cpu: number;
    duration: number;
    endState: string;
    hasLatency: boolean;
    itid: number;
    priority: number;
    processCmdLine?: string;
    processId: number;
    processName?: string;
    startTime: number;
    threadId: number;
    threadName: string;
};

export interface ThreadTraceMetadata {
    threadNameList: string[];
    tidList: number[];
    maxDepthList: number[];
};

export interface ThreadStateList {
    cpu: number;
    duration: number;
    endTime: number;
    process: string;
    processId: number;
    startTime: number;
    state: string;
    thread: string;
    threadId: number;
};

export interface ThreadWakeUpFrom {
    threadId: number;
    cpu: number;
    startTime: number;
    processId: number;
};

export interface ThreadWakeUpList {
    threadId: number;
    startTime: number;
    processId: number;
};

export interface ThreadStateDetail {
    cpu: number;
    duration: number;
    endTime: number;
    process: string;
    processId: number;
    startTime: number;
    state: string;
    thread: string;
    threadId: number;
    threadWakeUpFrom?: ThreadWakeUpFrom;
    threadWakeUpList?: ThreadWakeUpList[];
};

export interface ThreadTraceList {
    name: string;
    duration: number;
    startTime: number;
    endTime: number;
    depth: number;
    threadId: string;
};

export type ThreadTraceDetail = ThreadTraceList & { relateTraceList?: RelateTrace[] } & { relateFrameDetail?: RelateFrameDetail };

export interface RelateTrace {
    relateKey: string;
    name: string;
    startTime: number;
    duration: number;
    depth: number;
    threadId: number;
};

export interface RelateFrameDetail {
    processId: number;
    startTime: number;
    duration: number;
    depth: number;
    isJank: boolean;
    jankType: string;
    frameNo: number;
};

export interface ThreadStateStatistics {
    avgDuration: number;
    occurrences: number;
    processId: number;
    process: string;
    state: string;
    thread: string;
    threadId: number;
    duration: number;
    maxDuration: number;
    minDuration: number;
};

export interface ThreadTraceStatistics {
    name: string;
    wallDuration: number;
    avgDuration: number;
    occurrences: number;
};

export interface ProfileNode extends NativeCall {
    name: string;
    globalTotal: number;
    self: number;
    globalChildrenTotal: number;
    selfCostPercent: number;
    totalCostPercent: number;
    posTicks: Array<{
        ticks: number;
        line: number;
    }>;
    url: string;
    lineNumber?: number;
    columnNumber?: number;
    children?: ProfileNode[];
    category?: 'js' | 'system' | 'napi' | 'native';
} ; // #endregion

interface SysEventBase {
    id: number;
    name: string;
    time: number;
    tz: string;
    pid: number;
    tid: number;
    timestamp: number;
}

export interface KeyEventInfo extends SysEventBase {
    happenTime: number;
    upTime?: string;
    duration: number;
}

export interface EnergyAnomalyEventInfo {
    name: string;
    happenTime: number;
    endTime: number;
    duration: number;
}

export interface EnergyMonitorDetails {
    realTimeEnergy: number;
    totalEnergy: number;
}

export interface AbilityEvent extends SysEventBase {
    userId: number;
    appId: number;
    bundleName: string;
    moduleName: string;
    abilityName: string;
    processName: string;
}

export interface NativeCall {
    functionName?: string;
    threadName?: string;
    tid: number;
    depth: number;
    totalTime: number;
    selfTime: number;
    startTs: number;
    endTs: number;
    source?: string;
    url?: string;
    sourceAddr: number;
    children?: NativeCall[];
};

export interface JsDetailMemory {
    id?: number;
    className: string;
    childrenCount: number;
    distance: number;
    retainedSize: number;
    retainedSizePercent: number;
    shallowSize: number;
    shallowSizePercent: number;
    name: string;
    reachableFromWindow?: boolean;
    parentId?: number;
    pathNode?: number[];
    retainerNodes: JsDetailMemory[];
    children?: JsDetailMemory[];
};

export interface AssignStack {
    column: number;
    functionInfoIndex: number;
    line: number;
    name: string;
    nodeId: number;
    scriptName: string;
    traceNodeId: number;
};

export interface StatisticMemory {
    total: number;
    js: number;
    native: number;
    stack: number;
    code: number;
    others: number;
    category: string;
};

export interface NativeThread {
    threadUsage: number;
    state: number;
    timestamp: number;
};

// Native Memory实时主泳道区域数据
export interface NativeMemory {
    timestamp: number;
    total: number;
};

// Native Memory详情数据，三个tab下的每条数据定义
export interface NativeMemoryStatistics {
    idList: number;
    laneType: number;
    eventType: string;
    total: number; // 总计大小
    totalNumber: number; // 总计涉及次数
    malloc: number; // 创建的内存大小
    mallocNumber: number; // 创建的内存次数
    free: number; // 释放的内存大小
    freeNumber: number; // 释放的内存次数
};

export interface NativeMemoryCallInfo {
    laneType: number;
    symbolName: string; // 内存分配的调用栈
    size: number; // 分配的大小
    sizePercent: number;
    count: number;
    category?: 'js' | 'system' | 'napi' | 'native'; // 函数类型
    filePath?: string;
    offset: number;
};

export interface NativeMemoryObj {
    index: number;
    id: number;
    laneType: number;
    address: string;
    memoryType: string;
    timestamp: number; // 申请时间
    state: string;
    size: number;
    lib: string;
    caller: string;
    callStack: NativeMemoryCallStack[];
};

export interface NativeMemoryCallStack {
    frameSeqId: number;
    lib: string;
    caller: string;
    filePath: string;
    offset: number;
};

export interface JSCpuState {
    timestamp: number;
    duration: number;
    name: string;
    type: 'js' | 'system' | 'napi' | 'native';
    dataKey: 'jsCpuState';
};

export interface JsHeapMemory {
    timestamp: number;
    usedSize: number;
    totalSize: number;
};

export interface AppMemory {
    timestamp: number;
    total: number;
    native: number;
    graphics: number;
    stack: number;
    code: number;
    others: number;
};

export interface HeapSnapshotInfo {
    id: number;
    rawId: number;
    timestamp: number;
    duration: number;
    fileSize: number;
};

export type TotalHeapSnapshot = HeapSnapshotInfo;

export interface HeapDiffNode {
    name: string;
    addedCount: number;
    removedCount: number;
    addedSize: number;
    childrenCount: number;
    countDelta: number;
    removedSize: number;
    sizeDelta: number;
    distance: number;
    retainedSize: number;
    retainedSizePercent: number;
    shallowSize: number;
    shallowSizePercent: number;
    reachableFromWindow?: boolean;
    parentId?: number;
    pathNode?: number[];
    retainerNodes: JsDetailMemory[];
    className: string;
    id?: number;
    children?: HeapDiffNode[];
};

export interface CpuInsightMetadata {
    processTraceMetadata: {
        nameList: string[];
        idList: number[];
    };
    cpuSliceMetadata: number;
    cpuFreqMetadata: {
        freqLaneNumbers: number;
        maxFrequency: number;
    };
    dictData: {
        idList: number[];
        textList: string[];
    };
};

export interface CpuUsageLaneData {
    timestamp: number;
    coreUsage: number;
};

export interface CpuSingleSliceDetail {
    processName: string;
    processId: number;
    threadName: string;
    threadId: number;
    cmdLine: string;
    startTs: number;
    duration: number;
    priority: number;
    endState: string;
    hasLatency: boolean;
    schedulingData?: CpuSingleSliceSchedulingData;
};

export interface FrameSliceDetail {
    frameNo: number;
    vsync: number;
    gpuDuration: number;
    actualStartTime: number;
    actualDuration: number;
    expectStartTime: number;
    expectDuration: number;
    processId: number;
    processName: string;
    jankType: string;
    frameTrace?: FrameRelateTrace;
    appFrameList?: RelateFrame[]; // 为RS帧时 appFrameList字段存在  (appFrameList/rsFrameList不同时存在，但肯定有一个字段存在（可能length===0），标识为app侧帧/RS侧帧）
    rsFrameList?: RelateFrame[]; // 为app帧时 rsFrameList字段存在
    otherAppFrameList?: RelateFrame[]; // 为app帧时 存放可能存在的其余app侧的数据（多对一的情况)
};

export interface FrameRelateTrace {
    name: string;
    threadId: number;
    threadName: string;
    startTime: number;
    duration: number;
    depth: number;
};

export interface RelateFrame {
    frameNo: number;
    vsync: number;
    processId: number;
    processName: string;
    startTime: number;
    duration: number;
    depth: number;
    isJank: boolean;
};

export interface CpuSingleSliceSchedulingData {
    wakeUpTime: number;
    cpu: number;
    processName: string;
    processId: number;
    threadName: string;
    threadId: number;
    schedulingLatency: number;
};

export interface FrameTraceMetadata {
    processId: number;
    processName: string;
    depth: number;
    isRenderService: boolean;
};

export interface FrameLaneData {
    frameNo: string;
    vsync: number;
    startTime: number;
    endTime: number;
    jankTime: number;
    depth: number;
    jankType: string;
    expectStartTime: number;
    isJank: boolean;
};

export interface FrameAppStatistics {
    frameNo: number;
    vsync: number;
    startTime: number;
    appDur: number;
    renderServiceDur: number;
    gpuDur: number;
    totalDur: number;
    jankType: string;
};

export interface FrameRenderServiceStatistics {
    frameNo: number;
    vsync: number;
    startTime: number;
    duration: number;
    gpuDur: number;
    jankType: string;
};

export interface FrameTotalStatistics {
    processId: number;
    processName: string;
    jankRate: number;
    jankNum: number;
    maxConsecutiveJankNum: number;
    maxDuration: number;
    avgJankDuration: number;
    avgNormalDuration: number;
};

export interface FrameSliceList {
    processId: number;
    processName: string;
    startTime: number;
    jankType: string;
    frameNo: number;
    duration: number;
    gpuDuration: number;
    vsync: number;
};

export interface FrameMetadata {
    hasRenderService: boolean;
    idList: number[];
};

export interface HiperfStopResult {
    result: boolean;
    body: string;
};

export interface EnergyDetailsMap {
    cpu: EnergyMonitorDetails;
    display: EnergyMonitorDetails;
    gpu: EnergyMonitorDetails;
    location: EnergyMonitorDetails;
    camera: EnergyMonitorDetails;
    bluetooth: EnergyMonitorDetails;
    flashlight: EnergyMonitorDetails;
    audio: EnergyMonitorDetails;
    wifiscan: EnergyMonitorDetails;
};

export interface ProcessSearchResultType {
    type: string;
    startTime: number;
    duration: number;
    depth: 0;
    pid: number;
    tid: number;
    taskName: string;
    cookie: number;
};

export interface FrameSearchResultType {
    processId: number;
    startTime: number;
    endTime: number;
    depth: number;
    isJank: boolean;
};

export interface CPUCoreReqParams {
    unit?: string;
    index?: number;
    endTimeAll?: number;
    processId?: number;
    threadId?: number;
    processName?: string;
    threadName?: string;
};

export interface ProcessReqParams {
    unit?: string;
    index?: number;
    functionName?: string;
    endTimeAll?: number;
};

export interface FrameReqParams {
    unit?: string;
    index?: number;
    startTimeAll?: number;
    endTimeAll?: number;
    processId?: number;
    processName?: string;
    jankType?: string;
};

export interface ArkTSReqParams {
    unit?: string;
    index: number;
    startTimeAll: number;
    endTimeAll: number;
    startRecordTime: number;
    functionName?: string;
};

export interface NativeReqParams {
    unit?: string;
    index: number;
    threadId?: number;
    startTimeAll: number;
    endTimeAll: number;
    startRecordTime: number;
    functionName?: string;
};

export interface CpuSliceWakeType {
    currentCpu: number;
    wakeUpCpu: number;
    wakeUpTime: number;
    startTs: number;
    schedulingLatency: number;
};

export interface MetaDataEnumType {
    card: CardMetaData;
    process: ProcessMetaData;
    thread: ThreadMetaData;
    counter: CounterMetaData;
    label: LabelMetaData;
}

export interface InsightMetaData <T extends keyof MetaDataEnumType> {
    type: T;
    metadata: MetaDataEnumType[T];
    children?: Array<InsightMetaData<keyof MetaDataEnumType>>;
    dataSource: DataSource;
}

export type MetaDataBase = { dataSource: DataSource } & Record<string, unknown>;

export interface MetaDataInnerBase extends MetaDataBase {
    cardId: string;
    dbPath: string; // 卡对应信息所在 DB 的文件路径
    metaType: string; // db 格式数据下有效, text 格式数据下始终是 TEXT
    processId?: string;
    processName?: string;
    threadName?: string;
    threadId?: string;
    headerTooltip?: string;
    maxValue?: number;
}

export interface LabelMetaData extends MetaDataInnerBase {
    label: string;
}

export interface CounterMetaData extends MetaDataInnerBase {
    processId: string;
    processName?: string;
    threadName: string;
    threadId?: string;
    dataType: string[];
}

export interface ThreadMetaData extends MetaDataInnerBase {
    processId?: string;
    processName?: string;
    threadId: string;
    threadIdList?: string[];
    threadName: string;
    groupNameValue: string;
    maxDepth?: number;
    rankList: string[];
}

export interface ProcessMetaData extends MetaDataInnerBase {
    processId: string;
    processName: string;
    label?: string;
}

export interface CardMetaData extends MetaDataBase {
    cardId: string;
    dbPath: string;
    cluster: string;
    cardName: string;
    cardPath: string;
    label?: string;
    isFtrace?: boolean;
}

export interface HostMetaData extends MetaDataBase {
    host: string;
}

export interface EmptyMetaData extends MetaDataBase {
    count: number;
}

export interface ThreadTrace extends Omit<SliceData, 'rankId' | 'pid' | 'tid'> {
    [x: string]: unknown;
    name: string;
    duration: number;
    startTime: number;
    endTime: number;
    depth: number;
    threadId: string;
    id?: string;
    cname: string;
};

export interface SliceMeta extends MetaDataBase {
    [x: string]: unknown;
    cardId: string;
    processId: string;
    startTime: number;
    endTime: number;
    duration: number;
    threadId: string;
    label: string;
}

export interface SliceData {
    rankId: string;
    dbPath: string;
    pid: string;
    tid: string;
    id?: string;
    name: string;
    startTime: number;
    duration: number;
    depth: number;
}

export interface ProcessData {
    duration: number;
    startTime: number;
};

export interface CounterData {
    timestamp: number;
    value: any;
};

export interface ThreadTraceRequest {
    cardId: string;
    processId: string;
    threadId: string;
    startTime: number;
    endTime: number;
    metaType: string;
    unitType: string;
};

export interface CounterRequest {
    rankId: string;
    pid: string;
    threadName: number;
    startTime: number;
    endTime: number;
};

export interface ProcessRequest {
    cardId: string;
    processId: string;
    startTime: number;
    endTime: number;
};

export interface AscendSliceDetail extends Record<string, unknown> {
    pid?: string;
    tid?: string;
    title?: string;
    startTime?: number;
    rawStartTime?: string;
    rawEndTime?: string;
    depth?: number;
    duration?: number;
    selfTime?: number;
    args?: string;
    inputShapes?: string;
    inputDataTypes?: string;
    inputFormats?: string;
    outputShapes?: string;
    outputDataTypes?: string;
    outputFormats?: string;
    attrInfo?: string;
};

export interface SimpleProcessItem {
    pid: string;
    tidList: string[];
}

export interface AscendMultiSliceList extends Record<string, unknown> {
    title?: string;
    wallDuration?: number;
    selfTime?: number;
    avgWallDuration?: number;
    maxWallDuration?: number;
    minWallDuration?: number;
    occurrences?: number;
    rankId?: string;
    dbPath?: string;
    processes?: SimpleProcessItem[];
    startTime?: number;
    endTime?: number;
    metaTypeList?: string[];
}
