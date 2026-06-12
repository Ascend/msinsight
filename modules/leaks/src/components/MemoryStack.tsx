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
import React, { useEffect, useRef, useState } from 'react';
import { Select, Checkbox, CollapsiblePanel } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import { runInAction } from 'mobx';
import type { CheckboxChangeEvent } from 'antd/lib/checkbox';
import MemorySliceChart from './MemorySliceChart';
import MemoryFunctionCall from './MemoryFunctionCall';
import { Label } from './Common';
import { getFuncNewData, getBarNewData, getBlockTableData, getEventTableData, getPotentialLeakStats } from './dataHandler';
import { convertNanoseconds } from '../utils/utils';
import { MemoryBlockDiagram } from './leaks/MemoryBlockDiagram';
import { getInitialZoomDomain } from './leaks/zoomDomain';
import MemoryDataZoom from './MemoryDataZoom';
import { workerTransform } from '@/leaksWorker/blockWorker/worker';
import { MemoryStateDiagram } from './leaks/MemoryStateDiagram';
import PotentialLeakStats from './PotentialLeakStats';
import { debounce, type DebouncedFunc } from 'lodash';

type TransformChangeSource = 'wheel' | 'keyboard' | 'drag';
const isValidRange = (range: [number, number]): boolean => Number.isFinite(range[0]) && Number.isFinite(range[1]) && range[0] < range[1];
const isSameRange = (prev: [number, number] | undefined, next: [number, number]): boolean => {
    if (prev === undefined) {
        return false;
    }
    return prev[0] === next[0] && prev[1] === next[1];
};
const clampRangeToBounds = (range: [number, number], min: number, max: number): [number, number] => {
    const totalRange = max - min;
    const rangeWidth = Math.min(range[1] - range[0], totalRange);
    let start = Math.max(min, Math.min(max - rangeWidth, range[0]));
    if (!Number.isFinite(start)) {
        start = min;
    }
    return [Math.floor(start), Math.floor(start + rangeWidth)];
};

const MemoryStack = observer(({ session }: { session: any }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const [zoomData, setZoomData] = useState<Array<[number, number]>>([]);
    const [zoomMinTime, setZoomMinTime] = useState<number>(Number.MAX_SAFE_INTEGER);
    const [zoomMaxTime, setZoomMaxTime] = useState<number>(Number.MIN_SAFE_INTEGER);
    const [dataZoomKey, setDataZoomKey] = useState(0);
    const [selectedRange, setSelectedRange] = useState<[number, number] | undefined>(undefined);
    const selectedRangeRef = useRef<[number, number] | undefined>(undefined);
    const zoomRangeRef = useRef({ min: Number.MAX_SAFE_INTEGER, max: Number.MIN_SAFE_INTEGER });
    const debouncedFuncRangeRef = useRef<DebouncedFunc<(range: [number, number]) => void> | null>(null);
    const debouncedCommitRangeRef = useRef<DebouncedFunc<(range: [number, number]) => void> | null>(null);
    const debouncedLeakStatsRef = useRef<DebouncedFunc<(range: [number, number]) => void> | null>(null);
    const funcRangeRequestSeqRef = useRef(0);

    const schedulePotentialLeakStats = (range: [number, number]): void => {
        if (session.module !== 'memsnapshot') {
            return;
        }
        debouncedLeakStatsRef.current?.(range);
    };

    const requestPotentialLeakStatsNow = (range: [number, number]): void => {
        if (session.module !== 'memsnapshot') {
            return;
        }
        debouncedLeakStatsRef.current?.cancel();
        getPotentialLeakStats(session, range);
    };

    const commitSessionRange = (range: [number, number]): void => {
        runInAction(() => {
            session.minTime = range[0];
            session.maxTime = range[1];
        });
    };

    const commitRangeSideEffects = (range: [number, number]): void => {
        commitSessionRange(range);
        if (session.module === 'memsnapshot') {
            if (session.autoFilterPotentialLeaks) {
                getBlockTableData(session);
                getEventTableData(session);
            }
        }
    };

    const getCurrentFuncRange = (): [number, number] => {
        return selectedRangeRef.current ?? [session.minTime, session.maxTime];
    };

    const requestFuncRangeData = (range: [number, number]): void => {
        if (!isValidRange(range)) {
            return;
        }
        const requestSeq = ++funcRangeRequestSeqRef.current;
        getFuncNewData(session, range[0], range[1], () => requestSeq === funcRangeRequestSeqRef.current);
    };

    const scheduleRangeChange = (range: [number, number]): void => {
        if (!isValidRange(range)) {
            return;
        }
        if (isSameRange(selectedRangeRef.current, range)) {
            return;
        }
        selectedRangeRef.current = range;
        setSelectedRange(range);
        schedulePotentialLeakStats(range);
        debouncedFuncRangeRef.current?.cancel();
        debouncedFuncRangeRef.current?.(range);
        debouncedCommitRangeRef.current?.cancel();
        debouncedCommitRangeRef.current?.(range);
    };

    if (debouncedFuncRangeRef.current === null) {
        debouncedFuncRangeRef.current = debounce((range: [number, number]): void => {
            requestFuncRangeData(range);
        }, 500);
    }
    if (debouncedCommitRangeRef.current === null) {
        debouncedCommitRangeRef.current = debounce((range: [number, number]): void => {
            commitRangeSideEffects(range);
        }, 300);
    }
    if (debouncedLeakStatsRef.current === null) {
        debouncedLeakStatsRef.current = debounce((range: [number, number]): void => {
            getPotentialLeakStats(session, range);
        }, 150, { maxWait: 500 });
    }

    const selectedZoomChange = (range: [number, number]): void => {
        if (!isValidRange(range)) {
            return;
        }
        scheduleRangeChange(range);

        const { sizeInfo, renderOptions } = session.leaksWorkerInfo;
        const newScale = range[1] - range[0] === 0 ? Number.MAX_SAFE_INTEGER : (sizeInfo.maxTimestamp - sizeInfo.minTimestamp) / (range[1] - range[0]);
        const newX = -(range[0] - sizeInfo.minTimestamp) * renderOptions.zoom.x * newScale;
        const transform = { x: newX, y: 0, scaleX: newScale, scaleY: 1 };

        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
    };

    const syncDataZoomRange = (transform: RenderOptions['transform'], _source: TransformChangeSource = 'wheel'): void => {
        const { renderOptions } = session.leaksWorkerInfo;
        const { scaleX } = transform;
        const { viewport, zoom } = renderOptions;
        if (scaleX <= 0 || zoom.x <= 0 || viewport.width <= 0) {
            return;
        }
        const { min, max } = zoomRangeRef.current;
        if (min >= max) {
            return;
        }
        const visibleRange = viewport.width / zoom.x / scaleX;
        const visibleMinTime = zoom.offset - transform.x / scaleX / zoom.x;
        const range = clampRangeToBounds([visibleMinTime, visibleMinTime + visibleRange], min, max);
        if (!isValidRange(range)) {
            return;
        }
        scheduleRangeChange(range);
    };

    useEffect(() => {
        const newIdOpts = Object.keys(session.deviceIds).map((id: string) => ({ label: id, value: id }));
        if (newIdOpts.length > 0) {
            const newTypeOpts = session.deviceIds[newIdOpts[0].value].map((type: string) => ({ label: type, value: type }));
            const newThreadOpts = session.threadIds.map((thread: number) => ({ label: thread, value: thread }));
            runInAction(() => {
                session.deviceIdOpts = newIdOpts;
                session.typeOpts = newTypeOpts;
                session.threadOps = newThreadOpts;
                session.deviceId = newIdOpts[0].value;
                session.eventType = newTypeOpts[0].value;
                session.threadId = newThreadOpts[0]?.value ?? '';
            });
        }
        return () => {
        };
    }, [session.deviceIds, session.threadIds]);

    useEffect(() => {
        if (session.deviceId === '' || session.threadFlag) return;
        debouncedFuncRangeRef.current?.cancel();
        selectedRangeRef.current = undefined;
        setSelectedRange(undefined);
        setDataZoomKey(key => key + 1);
        getBarNewData(session);
    }, [session.deviceId, session.eventType, session.threadId]);

    useEffect(() => {
        setZoomData(session.allocationData.allocations.map((item: any) => ([item.timestamp, item.totalSize])));
        const { minTime, maxTime } = getInitialZoomDomain({
            blockMinTimestamp: session.leaksWorkerInfo.sizeInfo.minTimestamp,
            blockMaxTimestamp: session.leaksWorkerInfo.sizeInfo.maxTimestamp,
            allocationMinTimestamp: session.allocationData.minTimestamp,
            allocationMaxTimestamp: session.allocationData.maxTimestamp,
            funcMinTimestamp: session.funcData.minTimestamp,
            funcMaxTimestamp: session.funcData.maxTimestamp,
        });
        setZoomMinTime(minTime);
        setZoomMaxTime(maxTime);
        zoomRangeRef.current = { min: minTime, max: maxTime };
        if (selectedRange === undefined) {
            commitRangeSideEffects([minTime, maxTime]);
            getPotentialLeakStats(session, [minTime, maxTime]);
        }
    }, [
        session.allocationData.allocations,
        session.leaksWorkerInfo.sizeInfo.minTimestamp,
        session.leaksWorkerInfo.sizeInfo.maxTimestamp,
        session.funcData.minTimestamp,
        session.funcData.maxTimestamp,
        selectedRange,
    ]);

    useEffect(() => {
        return () => {
            debouncedFuncRangeRef.current?.cancel();
            debouncedCommitRangeRef.current?.cancel();
            debouncedLeakStatsRef.current?.cancel();
        };
    }, []);

    return (
        <>
            <CollapsiblePanel title={t('FlameGraph')} style={{ minWidth: 1000, display: session.threadOps.length > 0 && session.threadId !== '' ? 'block' : 'none' }}>
                <Label name={t('ThreadID')} />
                <Select
                    id={'select-threadId'}
                    value={session.threadId}
                    size="middle"
                    onChange={(value): void => {
                        runInAction(() => {
                            session.threadId = value;
                            session.threadFlag = false;
                            session.searchFunc = [];
                        });
                    }}
                    options={session.threadOps}
                />
                <Label name={t('Search')} style={{ marginLeft: 24 }} />
                <Select
                    id={'select-funcName'}
                    mode="multiple"
                    value={session.searchFunc}
                    style={{ width: 550, marginRight: 20 }}
                    onChange={(val: string[]): void => {
                        runInAction(() => { session.searchFunc = val; });
                    }}
                    options={session.funcOptions}
                    showSearch={true}
                    maxTagTextLength={10}
                    maxTagCount={4}
                />
                <Checkbox
                    checked={session.allowTrim}
                    onChange={(event: CheckboxChangeEvent): void => {
                        runInAction(() => { session.allowTrim = event.target.checked; });
                        requestFuncRangeData(getCurrentFuncRange());
                    }}
                >{t('allowTrim')}</Checkbox>
                <div id="funcContent" style={{ overflow: 'hidden', padding: 0, position: 'relative' }}>
                    <MemoryFunctionCall session={session} />
                </div>
            </CollapsiblePanel >
            <div data-testid="blockDiagramPanel">
                <CollapsiblePanel title={t('BlockGraph')} style={{ minWidth: 1000 }}>
                    <Label name={t('DeviceID')} />
                    <Select
                        id={'select-deviceId'}
                        style={{ marginRight: 20 }}
                        value={session.deviceId}
                        size="middle"
                        onChange={(value): void => {
                            runInAction(() => {
                                session.threadFlag = false;
                                session.typeOpts = session.deviceIds[value].map((type: string) => ({ label: type, value: type }));
                                session.deviceId = value;
                                session.eventType = session.deviceIds[value][0];
                            });
                        }}
                        options={session.deviceIdOpts}
                    />
                    <Label name={t('Type')} />
                    <Select
                        id={'select-type'}
                        value={session.eventType}
                        size="middle"
                        onChange={(value): void => {
                            runInAction(() => {
                                session.threadFlag = false;
                                session.eventType = value;
                            });
                        }}
                        options={session.typeOpts}
                    />
                    {session.module === 'memsnapshot' ? <PotentialLeakStats session={session} /> : <></>}
                    <div id="barContent" style={{ overflow: 'hidden', padding: 0, position: 'relative' }}>
                        <MemoryBlockDiagram
                            session={session}
                            onResetTransform={() => {
                                const { min, max } = zoomRangeRef.current;
                                const fullRange: [number, number] = [min, max];
                                if (!isValidRange(fullRange)) {
                                    return;
                                }
                                selectedRangeRef.current = fullRange;
                                setSelectedRange(fullRange);
                                debouncedFuncRangeRef.current?.cancel();
                                debouncedCommitRangeRef.current?.cancel();
                                debouncedLeakStatsRef.current?.cancel();
                                commitRangeSideEffects(fullRange);
                                requestFuncRangeData(fullRange);
                                requestPotentialLeakStatsNow(fullRange);
                                setDataZoomKey(key => key + 1);
                            }}
                            onTransformChange={syncDataZoomRange}
                        />
                        <MemoryDataZoom
                            key={dataZoomKey}
                            module={session.module}
                            offsetLeft={95}
                            offsetRight={105}
                            dataSource={zoomData}
                            minTime={zoomMinTime}
                            maxTime={zoomMaxTime}
                            selectedRange={selectedRange}
                            selectedZoomChange={selectedZoomChange} />
                    </div>
                </CollapsiblePanel>
            </div>
            {session.memoryStamp && session.module === 'leaks' && session.eventType !== 'HOST_PINNED'
                ? (
                    <CollapsiblePanel title={t('DetailsDiagram')} collapsible style={{ minWidth: 1000 }}>
                        <div id="detailsContent" style={{ position: 'relative' }}>
                            <div style={{ position: 'absolute', left: '42%' }}>{`${t('Current Time')}: ${convertNanoseconds(session.memoryStamp)}`}</div>
                            <MemorySliceChart session={session} />
                        </div>
                    </CollapsiblePanel>
                )
                : (
                    <></>
                )
            }
            {session.module === 'memsnapshot' &&
                <CollapsiblePanel title={t('stateDiagram')} testId="stateDiagramPanel" style={{ minWidth: 1000 }}>
                    <MemoryStateDiagram session={session} />
                </CollapsiblePanel>
            }
        </>
    );
});

export default MemoryStack;
