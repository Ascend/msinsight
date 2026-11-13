/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import type { MemSnapshotContext } from '../state/MemSnapshotContext';
import { getStateRenderer } from '../rendering/stateWorker/worker';
import { useLifecycleRange } from '../common/shared/hooks/useLifecycleRange';
import React, { useEffect, useRef, useState } from 'react';
import { Select, CollapsiblePanel } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import { runInAction } from 'mobx';

import { message } from 'antd';

import { Label } from '../common/components/Common';
import { getBarNewData, getBlockTableData, getEventTableData, getPotentialLeakStats, preloadSnapshotSliceOverviews } from '../loaders/data';

import { MemoryBlockDiagram } from '../common/components/leaks/MemoryBlockDiagram';
import { getInitialZoomDomain } from '../common/components/leaks/zoomDomain';
import MemoryDataZoom from '../common/components/MemoryDataZoom';
import { getBlockRenderer } from '../common/leaksWorker/blockWorker/worker';
import { MemoryStateDiagram } from './MemoryStateDiagram';
import PotentialLeakStats from './PotentialLeakStats';
import MemSnapshotSliceOverview from './MemSnapshotSliceOverview';
import { FrameTreePanel } from './FrameTreePanel';
import { resolveFrameTreeEvent } from '../frameTree/timeline';
const isValidRange = (range: [number, number]): boolean => Number.isFinite(range[0]) && Number.isFinite(range[1]) && range[0] < range[1];
const MemoryStack = observer(({ session }: {
    session: MemSnapshotContext;
}): React.ReactElement => {
    const { removeMemoryBlockCaches: workerRemoveMemoryBlockCaches, setBlockGraphGlobalMaxSize: workerSetBlockGraphGlobalMaxSize } = getBlockRenderer(session);
    const { selectItem: workerSelectStateItem } = getStateRenderer(session);

    const { t } = useTranslation('leaks');
    const frameTreeDataKey = JSON.stringify([
        session.fileHash, session.dbPath, session.deviceId, session.eventType, session.selectedSliceIndex,
        session.memSnapshotParseFileId, session.memSnapshotParseLoading, session.memSnapshotCacheRefreshPending,
    ]);
    const frameTreeDataKeyRef = useRef(frameTreeDataKey);
    frameTreeDataKeyRef.current = frameTreeDataKey;
    const frameTreeSequence = useRef(0);
    const frameTreeTimer = useRef<ReturnType<typeof setTimeout>>();
    const [frameTreeSelection, setFrameTreeSelection] = useState<{
        key: string; eventId?: number; resolving?: boolean; error?: string;
    }>();
    const currentFrameTreeSelection = frameTreeSelection?.key === frameTreeDataKey ? frameTreeSelection : undefined;
    useEffect(() => {
        frameTreeSequence.current++;
        return () => {
            frameTreeSequence.current++;
            clearTimeout(frameTreeTimer.current);
        };
    }, [frameTreeDataKey]);
    const selectFrameTreeEvent = (coordinate: number): void => {
        const key = frameTreeDataKey;
        const deviceId = session.deviceId;
        const sliceIndex = session.selectedSliceIndex;
        const sequence = ++frameTreeSequence.current;
        clearTimeout(frameTreeTimer.current);
        setFrameTreeSelection({ key, resolving: true });
        if (session.memSnapshotParseLoading || session.memSnapshotCacheRefreshPending || !session.snapshotParsingComplete) {
            setFrameTreeSelection({ key, error: t('frameTree.parsing') });
            return;
        }
        frameTreeTimer.current = setTimeout(() => {
            void resolveFrameTreeEvent(coordinate, deviceId, sliceIndex).then(eventId => {
                if (sequence !== frameTreeSequence.current || frameTreeDataKeyRef.current !== key) return;
                setFrameTreeSelection(eventId === null ? { key, error: t('frameTree.noEvent') } : { key, eventId });
            }).catch(error => {
                if (sequence !== frameTreeSequence.current || frameTreeDataKeyRef.current !== key) return;
                setFrameTreeSelection({ key, error: error instanceof Error ? error.message : String(error) });
            });
        }, 150);
    };
    const [zoomData, setZoomData] = useState<Array<[number, number]>>([]);
    const [zoomMinTime, setZoomMinTime] = useState<number>(Number.MAX_SAFE_INTEGER);
    const [zoomMaxTime, setZoomMaxTime] = useState<number>(Number.MIN_SAFE_INTEGER);
    const {
        selectedRange, setSelectedRange, selectedRangeRef, zoomRangeRef, debouncedRangeRequestRef,
        debouncedCommitRangeRef, rangeRequestSeqRef, commitRangeSideEffects,
        requestRangeData, selectedZoomChange, syncDataZoomRange,
    } = useLifecycleRange(session, {
        synchronizeSlider: true,
        onRangeCommit: (): void => {
            if (session.autoFilterPotentialLeaks) { void getBlockTableData(session); void getEventTableData(session); }
        },
    });
    const deviceSlices = session.snapshotSlices[session.deviceId];
    const readySlicesSignature = JSON.stringify(deviceSlices?.readySlices ?? []);

    useEffect(() => {
        const newIdOpts = Object.keys(session.deviceIds).map((id: string) => ({ label: id, value: id }));
        if (newIdOpts.length > 0) {
            const nextDeviceId = newIdOpts.some((option: {
                value: string;
            }) => option.value === session.deviceId)
                ? session.deviceId
                : newIdOpts[0].value;
            const newTypeOpts = session.deviceIds[nextDeviceId].map((type: string) => ({ label: type, value: type }));
            const nextEventType = newTypeOpts.some((option: {
                value: string;
            }) => option.value === session.eventType)
                ? session.eventType
                : newTypeOpts[0].value;
            runInAction(() => {
                session.deviceIdOpts = newIdOpts;
                session.typeOpts = newTypeOpts;
                session.deviceId = nextDeviceId;
                session.eventType = nextEventType;
            });
        }
        return () => {
        };
    }, [session.deviceIds]);
    useEffect(() => {
        debouncedRangeRequestRef.current?.cancel();
        debouncedCommitRangeRef.current?.cancel();
        rangeRequestSeqRef.current++;
        selectedRangeRef.current = undefined;
        setSelectedRange(undefined);
        if (session.deviceId === '') { return; }
        // Replace invalidated trace requests after cancelling the previous context's zoom work.
        void getBarNewData(session);
    }, [session, session.fileHash, session.module, session.deviceIds, session.deviceId,
        session.eventType, session.selectedSliceIndex]);
    useEffect(() => {
        if (session.deviceId === '' || !deviceSlices) {
            return;
        }
        if (!deviceSlices.readySlices.includes(session.selectedSliceIndex)) {
            const nextSlice = deviceSlices.readySlices.length > 0 ? Math.max(...deviceSlices.readySlices) : -1;
            runInAction(() => {
                session.selectedSliceIndex = nextSlice;
            });
        }
        // 新分窗就绪时只补齐全局缩略趋势，不刷新当前分窗。
        void preloadSnapshotSliceOverviews(session);
    }, [session.module, session.deviceId, session.eventType, readySlicesSignature,
        session.snapshotParsingComplete, session.memSnapshotCacheRefreshPending]);
    useEffect(() => {
        if (session.memSnapshotParseLoading || !session.memSnapshotCacheRefreshPending ||
                session.deviceId === '' || session.selectedSliceIndex < 0) {
            return;
        }
        const refreshFinalizedSnapshot = async (): Promise<void> => {
            const fileHash = session.fileHash;
            let cacheRemovalSucceeded = false;
            try {
                try {
                    await workerRemoveMemoryBlockCaches({ fileHash });
                    cacheRemovalSucceeded = true;
                } catch (error) {
                    message.error(error instanceof Error ? error.message : String(error));
                }
                if (session.fileHash !== fileHash || session.module !== 'memsnapshot') {
                    return;
                }
                if (cacheRemovalSucceeded) {
                    runInAction(() => {
                        session.memSnapshotCacheRefreshPending = false;
                    });
                }
                // 所有反插和完整性校验结束后，再统一刷新一次当前分窗。
                // 删除缓存失败时先保持 pending，让本次 getBarNewData 绕过过期 OPFS 缓存。
                debouncedRangeRequestRef.current?.cancel();
                selectedRangeRef.current = undefined;
                setSelectedRange(undefined);
                await getBarNewData(session);
            } finally {
                if (session.fileHash === fileHash && session.module === 'memsnapshot') {
                    runInAction(() => {
                        session.memSnapshotCacheRefreshPending = false;
                    });
                }
            }
        };
        void refreshFinalizedSnapshot();
    }, [
        session.memSnapshotParseLoading,
        session.memSnapshotCacheRefreshPending,
        session.module,
        session.deviceId,
        session.selectedSliceIndex,
        session.fileHash,
    ]);
    const snapshotGlobalMaxSize = session.snapshotGlobalMaxSizes[session.deviceId]?.[session.eventType] ?? 0;
    useEffect(() => {
        workerSetBlockGraphGlobalMaxSize({
            maxSize: snapshotGlobalMaxSize,
        });
    }, [session.module, session.fileHash, session.deviceId, session.eventType, snapshotGlobalMaxSize]);
    useEffect(() => {
        setZoomData([]);
        const { minTime, maxTime } = getInitialZoomDomain({
            blockMinTimestamp: session.leaksWorkerInfo.sizeInfo.minTimestamp,
            blockMaxTimestamp: session.leaksWorkerInfo.sizeInfo.maxTimestamp,
            allocationMinTimestamp: session.allocationData.minTimestamp,
            allocationMaxTimestamp: session.allocationData.maxTimestamp,
            funcMinTimestamp: 0,
            funcMaxTimestamp: 0,
        });
        setZoomMinTime(minTime);
        setZoomMaxTime(maxTime);
        zoomRangeRef.current = { min: minTime, max: maxTime };
        if (selectedRangeRef.current === undefined) {
            commitRangeSideEffects([minTime, maxTime]);
        }
    }, [
        session.module,
        session.allocationData.allocations,
        session.leaksWorkerInfo.sizeInfo.minTimestamp,
        session.leaksWorkerInfo.sizeInfo.maxTimestamp,
        0,
        0,
    ]);
    useEffect(() => {
        if (session.selectedSliceIndex < 0) {
            debouncedRangeRequestRef.current?.cancel();
            debouncedCommitRangeRef.current?.cancel();
        }
    }, [session.module, session.selectedSliceIndex, session.memSnapshotParseFileId]);
    useEffect(() => {
        // Leak stats should only follow the committed session range after the selected
        // snapshot slice is ready. Importing a new pickle must not query the unfinished db.
        getPotentialLeakStats(session);
    }, [session.module, session.deviceId, session.minTime, session.maxTime, session.selectedSliceIndex]);

    return (<>
        <div data-testid="blockDiagramPanel">
            <CollapsiblePanel title={t('BlockGraph')} style={{ minWidth: 1000 }}>
                <Label name={t('DeviceID')}/>
                <Select id={'select-deviceId'} style={{ marginRight: 20 }} value={session.deviceId} size="middle" onChange={(value): void => {
                    runInAction(() => {
                        session.typeOpts = session.deviceIds[value].map((type: string) => ({ label: type, value: type }));
                        session.deviceId = value;
                        session.eventType = session.deviceIds[value][0];
                    });
                }} options={session.deviceIdOpts}/>
                <Label name={t('Type')}/>
                <Select id={'select-type'} value={session.eventType} size="middle" onChange={(value): void => {
                    runInAction(() => {
                        session.eventType = value;
                    });
                }} options={session.typeOpts.map((option: {
                    label: string | number;
                    value: string | number;
                }) => ({
                    ...option,
                    label: option.value === 'BLOCK' ? 'PTA_BLOCK' : option.label,
                }))}/>
                {<PotentialLeakStats session={session}/>}
                <div id="barContent" style={{ overflow: 'hidden', padding: 0, position: 'relative' }}>
                    <MemoryBlockDiagram session={session} enhancedControls windowKey={session.selectedSliceIndex}
                        selectedEventId={currentFrameTreeSelection?.eventId} onSelectTimestamp={selectFrameTreeEvent}
                        onSelectBlock={(selectionVersion): void => {
                            workerSelectStateItem({ item: null, selectionVersion });
                            runInAction(() => { session.stateWorkerInfo.clickItem = null; session.clickEventItem = null; });
                        }} onResetTransform={() => {
                            const { min, max } = zoomRangeRef.current;
                            const fullRange: [number, number] = [min, max];
                            if (!isValidRange(fullRange)) {
                                return;
                            }
                            selectedRangeRef.current = fullRange;
                            setSelectedRange(fullRange);
                            debouncedRangeRequestRef.current?.cancel();
                            debouncedCommitRangeRef.current?.cancel();
                            commitRangeSideEffects(fullRange);
                            requestRangeData(fullRange);
                        }} onTransformChange={syncDataZoomRange}/>
                    {deviceSlices
                        ? <MemSnapshotSliceOverview deviceSlices={deviceSlices} overviewData={session.sliceOverviewData[session.deviceId] ?? {}} selectedSliceIndex={session.selectedSliceIndex} selectedRange={selectedRange} onSelectSlice={(sliceIndex): void => {
                            if (!(deviceSlices.slices[sliceIndex]?.ready)) {
                                return;
                            }
                            runInAction(() => {
                                session.selectedSliceIndex = sliceIndex;
                            });
                        }} onRangeChange={selectedZoomChange}/>
                        : <MemoryDataZoom session={session} module={session.module} offsetLeft={95} offsetRight={105} dataSource={zoomData} minTime={zoomMinTime} maxTime={zoomMaxTime} selectedRange={selectedRange} selectedZoomChange={selectedZoomChange}/>}
                </div>
            </CollapsiblePanel>
        </div>
        <FrameTreePanel context={currentFrameTreeSelection?.eventId === undefined
            ? null
            : {
                datasetKey: frameTreeDataKey, deviceId: session.deviceId, eventId: currentFrameTreeSelection.eventId,
            }} resolving={currentFrameTreeSelection?.resolving} selectionError={currentFrameTreeSelection?.error} />
        {<CollapsiblePanel title={t('stateDiagram')} testId="stateDiagramPanel" style={{ minWidth: 1000 }}>
            <MemoryStateDiagram session={session}/>
        </CollapsiblePanel>}
    </>);
});
export default MemoryStack;
