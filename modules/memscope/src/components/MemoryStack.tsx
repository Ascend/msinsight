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
import type { MemScopeContext } from '../state/MemScopeContext';
import { useLifecycleRange } from '../common/shared/hooks/useLifecycleRange';
import React, { useEffect, useState } from 'react';
import { Select, Checkbox, CollapsiblePanel } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import { runInAction } from 'mobx';
import type { CheckboxChangeEvent } from 'antd/lib/checkbox';

import MemorySliceChart from './MemorySliceChart';
import MemoryFunctionCall from './MemoryFunctionCall';
import { Label } from '../common/components/Common';
import { getFuncNewData, getBarNewData } from '../loaders/data';
import { convertNanoseconds, isHostMemoryEventType } from '../common/utils/utils';
import { MemoryBlockDiagram } from '../common/components/leaks/MemoryBlockDiagram';
import { getInitialZoomDomain } from '../common/components/leaks/zoomDomain';
import MemoryDataZoom from '../common/components/MemoryDataZoom';

const isValidRange = (range: [number, number]): boolean => Number.isFinite(range[0]) && Number.isFinite(range[1]) && range[0] < range[1];
const MemoryStack = observer(({ session }: {
    session: MemScopeContext;
}): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const [zoomData, setZoomData] = useState<Array<[number, number]>>([]);
    const [zoomMinTime, setZoomMinTime] = useState<number>(Number.MAX_SAFE_INTEGER);
    const [zoomMaxTime, setZoomMaxTime] = useState<number>(Number.MIN_SAFE_INTEGER);
    const {
        selectedRange, setSelectedRange, selectedRangeRef, zoomRangeRef, debouncedRangeRequestRef,
        debouncedCommitRangeRef, rangeRequestSeqRef, commitRangeSideEffects, getCurrentRange,
        requestRangeData, selectedZoomChange, syncDataZoomRange,
    } = useLifecycleRange(session, { onRangeRequest: (range, shouldApply): void => { void getFuncNewData(session, range[0], range[1], shouldApply); } });

    useEffect(() => {
        const newIdOpts = Object.keys(session.deviceIds).map((id: string) => ({ label: id, value: id }));
        if (newIdOpts.length > 0) {
            const nextDeviceId = newIdOpts.some((option: {
                value: string;
            }) => option.value === session.deviceId)
                ? session.deviceId
                : newIdOpts[0].value;
            const newTypeOpts = session.deviceIds[nextDeviceId].map((type: string) => ({ label: type, value: type }));
            const newThreadOpts = session.threadIds.map((thread: number) => ({ label: thread, value: thread }));
            const nextEventType = newTypeOpts.some((option: {
                value: string;
            }) => option.value === session.eventType)
                ? session.eventType
                : newTypeOpts[0].value;
            const nextThreadId = newThreadOpts.some((option: {
                value: number;
            }) => option.value === session.threadId)
                ? session.threadId
                : newThreadOpts[0]?.value ?? '';
            runInAction(() => {
                session.deviceIdOpts = newIdOpts;
                session.typeOpts = newTypeOpts;
                session.threadOps = newThreadOpts;
                session.deviceId = nextDeviceId;
                session.eventType = nextEventType;
                session.threadId = nextThreadId;
            });
        }
        return () => {
        };
    }, [session.deviceIds, session.threadIds]);
    useEffect(() => {
        debouncedRangeRequestRef.current?.cancel();
        debouncedCommitRangeRef.current?.cancel();
        rangeRequestSeqRef.current++;
        if (session.threadFlag) { return; }
        selectedRangeRef.current = undefined;
        setSelectedRange(undefined);
        if (session.deviceId === '') { return; }
        // Replace invalidated trace requests after cancelling the previous context's zoom work.
        void getFuncNewData(session);
        void getBarNewData(session);
    }, [session, session.fileHash, session.module, session.deviceIds, session.deviceId,
        session.eventType, session.threadId, -1]);
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
        if (selectedRangeRef.current === undefined) {
            commitRangeSideEffects([minTime, maxTime]);
        }
    }, [
        session.module,
        session.allocationData.allocations,
        session.leaksWorkerInfo.sizeInfo.minTimestamp,
        session.leaksWorkerInfo.sizeInfo.maxTimestamp,
        session.funcData.minTimestamp,
        session.funcData.maxTimestamp,
    ]);

    return (<>
        <CollapsiblePanel title={t('FlameGraph')} style={{ minWidth: 1000, display: session.threadOps.length > 0 && session.threadId !== '' ? 'block' : 'none' }}>
            <Label name={t('ThreadID')}/>
            <Select id={'select-threadId'} value={session.threadId} size="middle" onChange={(value): void => {
                runInAction(() => {
                    session.threadId = value;
                    session.threadFlag = false;
                    session.searchFunc = [];
                });
            }} options={session.threadOps}/>
            <Label name={t('Search')} style={{ marginLeft: 24 }}/>
            <Select id={'select-funcName'} mode="multiple" value={session.searchFunc} style={{ width: 550, marginRight: 20 }} onChange={(val: string[]): void => {
                runInAction(() => { session.searchFunc = val; });
            }} options={session.funcOptions} showSearch={true} maxTagTextLength={10} maxTagCount={4}/>
            <Checkbox checked={session.allowTrim} onChange={(event: CheckboxChangeEvent): void => {
                runInAction(() => { session.allowTrim = event.target.checked; });
                requestRangeData(getCurrentRange());
            }}>{t('allowTrim')}</Checkbox>
            <div id="funcContent" style={{ overflow: 'hidden', padding: 0, position: 'relative' }}>
                <MemoryFunctionCall session={session}/>
            </div>
        </CollapsiblePanel>
        <div data-testid="blockDiagramPanel">
            <CollapsiblePanel title={t('BlockGraph')} style={{ minWidth: 1000 }}>
                <Label name={t('DeviceID')}/>
                <Select id={'select-deviceId'} style={{ marginRight: 20 }} value={session.deviceId} size="middle" onChange={(value): void => {
                    runInAction(() => {
                        session.threadFlag = false;
                        session.typeOpts = session.deviceIds[value].map((type: string) => ({ label: type, value: type }));
                        session.deviceId = value;
                        session.eventType = session.deviceIds[value][0];
                    });
                }} options={session.deviceIdOpts}/>
                <Label name={t('Type')}/>
                <Select id={'select-type'} value={session.eventType} size="middle" onChange={(value): void => {
                    runInAction(() => {
                        session.threadFlag = false;
                        session.eventType = value;
                    });
                }} options={session.typeOpts.map((option: {
                    label: string | number;
                    value: string | number;
                }) => ({
                    ...option,
                    label: option.value === 'BLOCK' ? 'PTA_BLOCK' : option.label,
                }))}/>
                {<></>}
                <div id="barContent" style={{ overflow: 'hidden', padding: 0, position: 'relative' }}>
                    <MemoryBlockDiagram session={session} onSelectTimestamp={(timestamp): void => { session.memoryStamp = timestamp; }} onResetTransform={() => {
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
                    {<MemoryDataZoom session={session} module={session.module} offsetLeft={95} offsetRight={105} dataSource={zoomData} minTime={zoomMinTime} maxTime={zoomMaxTime} selectedRange={selectedRange} selectedZoomChange={selectedZoomChange}/>}
                </div>
            </CollapsiblePanel>
        </div>
        {session.memoryStamp && !isHostMemoryEventType(session.eventType)
            ? (<CollapsiblePanel title={t('DetailsDiagram')} collapsible style={{ minWidth: 1000 }}>
                <div id="detailsContent" style={{ position: 'relative' }}>
                    <div style={{ position: 'absolute', left: '42%' }}>{`${t('Current Time')}: ${convertNanoseconds(session.memoryStamp)}`}</div>
                    <MemorySliceChart session={session}/>
                </div>
            </CollapsiblePanel>)
            : (<></>)}
        {false}
    </>);
});
export default MemoryStack;
