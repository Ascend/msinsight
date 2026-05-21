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
import MemoryDataZoom from './MemoryDataZoom';
import { workerTransform } from '@/leaksWorker/blockWorker/worker';
import { MemoryStateDiagram } from './leaks/MemoryStateDiagram';
import PotentialLeakStats from './PotentialLeakStats';

const MemoryStack = observer(({ session }: { session: any }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const [zoomData, setZoomData] = useState<Array<[number, number]>>([]);
    const [zoomMinTime, setZoomMinTime] = useState<number>(Number.MAX_SAFE_INTEGER);
    const [zoomMaxTime, setZoomMaxTime] = useState<number>(Number.MIN_SAFE_INTEGER);
    const leakStatsTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
    const refreshPotentialLeakStats = (): void => {
        if (leakStatsTimer.current !== null) {
            clearTimeout(leakStatsTimer.current);
        }
        leakStatsTimer.current = setTimeout(() => {
            getPotentialLeakStats(session);
            leakStatsTimer.current = null;
        }, 300);
    };

    const selectedZoomChange = (range: [number, number]): void => {
        getFuncNewData(session, range[0], range[1]);
        if (session.module === 'memsnapshot') {
            runInAction(() => {
                session.minTime = range[0];
                session.maxTime = range[1];
                if (session.autoFilterPotentialLeaks) {
                    session.blocksCurrentPage = 1;
                    session.eventsCurrentPage = 1;
                }
            });
            refreshPotentialLeakStats();
            if (session.autoFilterPotentialLeaks) {
                getBlockTableData(session);
                getEventTableData(session);
            }
        }

        const { sizeInfo, renderOptions } = session.leaksWorkerInfo;
        const newScale = range[1] - range[0] === 0 ? Number.MAX_SAFE_INTEGER : (sizeInfo.maxTimestamp - sizeInfo.minTimestamp) / (range[1] - range[0]);
        const newX = -(range[0] - sizeInfo.minTimestamp) * renderOptions.zoom.x * newScale;
        const transform = { x: newX, y: 0, scale: newScale };

        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
    };

    useEffect(() => () => {
        if (leakStatsTimer.current !== null) {
            clearTimeout(leakStatsTimer.current);
        }
    }, []);

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
        getBarNewData(session);
    }, [session.deviceId, session.eventType, session.threadId]);

    useEffect(() => {
        setZoomData(session.allocationData.allocations.map((item: any) => ([item.timestamp, item.totalSize])));
        let minTime = Math.min(session.leaksWorkerInfo.sizeInfo.minTimestamp, session.allocationData.minTimestamp);
        let maxTime = Math.max(session.leaksWorkerInfo.sizeInfo.maxTimestamp, session.allocationData.maxTimestamp);
        if (session.funcData.maxTimestamp > 0) {
            minTime = Math.min(minTime, session.funcData.minTimestamp);
            maxTime = Math.max(maxTime, session.funcData.maxTimestamp);
        }
        setZoomMinTime(minTime);
        setZoomMaxTime(maxTime);
        runInAction(() => {
            session.maxTime = maxTime;
            session.minTime = minTime;
        });
        if (session.module === 'memsnapshot') {
            refreshPotentialLeakStats();
        }
    }, [session.allocationData.allocations]);

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
                        runInAction(() => { session.allowTrim = event.target.checked; getFuncNewData(session, session.minTime, session.maxTime); });
                    }}
                >{t('allowTrim')}</Checkbox>
                <div id="funcContent" style={{ overflow: 'hidden', padding: 0, position: 'relative' }}>
                    <MemoryFunctionCall session={session} />
                </div>
            </CollapsiblePanel >
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
                    <MemoryBlockDiagram session={session} />
                    <MemoryDataZoom
                        module={session.module}
                        offsetLeft={95}
                        offsetRight={105}
                        dataSource={zoomData}
                        minTime={zoomMinTime}
                        maxTime={zoomMaxTime}
                        selectedZoomChange={selectedZoomChange} />
                </div>
            </CollapsiblePanel>
            {session.memoryStamp && session.module === 'leaks'
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
                <CollapsiblePanel title={t('stateDiagram')} style={{ minWidth: 1000 }}>
                    <MemoryStateDiagram session={session} />
                </CollapsiblePanel>
            }
        </>
    );
});

export default MemoryStack;
