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
import React, { useState } from 'react';
import { createRoot } from 'react-dom/client';
import { ThemeProvider } from '@emotion/react';
import { observer } from 'mobx-react';
import { observable, runInAction } from 'mobx';
import { themeInstance, GlobalStyles } from '@insight/lib/theme';
import { SharedConfigProvider } from '@insight/lib';
import '@insight/lib/style';
import '@insight/lib/i18n';
import i18n from 'i18next';
import { FrameTreePanel } from '../../src/components/FrameTreePanel';
import type { FrameTreeNodeDTO, FrameTreePageRequest } from '../../src/frameTree/types';
import { SnapshotEventFlag } from '../../src/frameTree/SnapshotEventFlag';
import type { SnapshotEventMarker } from '../../src/frameTree/timeline';
import { SnapshotEventMarkerManager } from '../../src/frameTree/SnapshotEventMarkerManager';
import { FrameTreeTabs } from '../../src/frameTree/FrameTreeTabs';
import type { MemoryContext } from '../../src/common/shared/state/MemoryContext';
import { createMockFrameTreePage } from '../../src/__tests__/fixtures/frameTreeMock';

const parameters = new URLSearchParams(location.search);
const language = parameters.get('lang') || 'enUS';
const deepNodes: FrameTreeNodeDTO[] = parameters.has('deep')
    ? Array.from({ length: 201 }, (_, depth) => ({
        nodeId: `deep-${depth}`,
        parentId: depth === 0 ? null : `deep-${depth - 1}`,
        depth,
        kind: depth === 0 ? 'root' : 'frame',
        frame: depth === 0 ? null : { name: `call_${depth}`, filename: 'deep_stack.cpp', line: depth },
        sizeBytes: '1',
        requestedBytes: '1',
        blockCount: '1',
        selfBytes: depth === 200 ? '1' : '0',
        selfRequestedBytes: depth === 200 ? '1' : '0',
        selfBlockCount: depth === 200 ? '1' : '0',
        childCount: depth === 200 ? 0 : 1,
    }))
    : [];
const deepRequest: FrameTreePageRequest = async params => ({
    total: deepNodes.length,
    nodes: deepNodes.slice((params.currentPage - 1) * params.pageSize, params.currentPage * params.pageSize),
});
const presentationNode = (id: string, parentId: string | null, depth: number, bytes: string, self: string,
    childCount: number, filename = `${id}.py`): FrameTreeNodeDTO => ({
    nodeId: id,
    parentId,
    depth,
    childCount,
    kind: parentId === null ? 'root' : 'frame',
    frame: parentId === null ? null : { name: id, filename, line: 1 },
    sizeBytes: bytes,
    selfBytes: self,
    requestedBytes: bytes,
    selfRequestedBytes: self,
    blockCount: bytes,
    selfBlockCount: self,
});
const presentationNodes: FrameTreeNodeDTO[] = [
    presentationNode('root', null, 0, '10000000000000107', '0', 4),
    presentationNode('python-outer', 'root', 1, '103', '0', 1),
    presentationNode('native', 'python-outer', 2, '103', '1', 101, 'native.cpp'),
    ...Array.from({ length: 100 }, (_, index) => presentationNode(`tiny-${index}`, 'native', 3, '1', '1', 0)),
    { ...presentationNode('static', 'root', 1, '10000000000000000', '10000000000000000', 0), kind: 'static', frame: null },
    { ...presentationNode('empty', 'root', 1, '1', '1', 0), kind: 'emptyStack', frame: null },
    { ...presentationNode('preexisting', 'root', 1, '3', '3', 0), kind: 'preexisting', frame: null },
    { ...presentationNode('missing', 'native', 3, '2', '2', 0), kind: 'missingFrame', frame: null },
];
const presentationRequest: FrameTreePageRequest = async params => ({
    total: presentationNodes.length,
    nodes: presentationNodes.slice((params.currentPage - 1) * params.pageSize, params.currentPage * params.pageSize),
});
const EventAxisPreview = observer((): JSX.Element => {
    const width = Math.max(1, window.innerWidth - 24);
    // This fixture has no renderer, API or backend connection.
    const [session] = useState(() => observable({
        deviceId: '0',
        leaksWorkerInfo: {
            sizeInfo: { minTimestamp: 0, maxTimestamp: 100 },
            renderOptions: {
                viewport: { width, height: 240 },
                zoom: { x: width / 100, y: 1 },
                transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
            },
        },
    }) as unknown as MemoryContext);
    const [eventId, setEventId] = useState<number>();
    const [markers, setMarkers] = useState<SnapshotEventMarker[]>([]);
    const [manager, setManager] = useState(false);
    const [commits, setCommits] = useState(0);
    const colors = themeInstance.getThemeType();
    return <div style={{ marginTop: 12 }}>
        <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'center', gap: 12 }}>
            <button data-testid="snapshot-axis-pan" onClick={(): void => runInAction(() => {
                session.leaksWorkerInfo.renderOptions.transform.x = -width / 4;
            })}>Pan</button>
            <button data-testid="snapshot-axis-zoom" onClick={(): void => runInAction(() => {
                session.leaksWorkerInfo.renderOptions.transform.scaleX = 2;
            })}>Zoom</button>
            <button data-testid="snapshot-axis-reset" onClick={(): void => runInAction(() => {
                session.leaksWorkerInfo.renderOptions.transform.x = 0;
                session.leaksWorkerInfo.renderOptions.transform.scaleX = 1;
            })}>Reset</button>
            <span>Fixture event: <output data-testid="snapshot-axis-event">{eventId ?? '-'}</output></span>
            <span>Commits: <output data-testid="snapshot-axis-commits">{commits}</output></span>
            <button data-testid="snapshot-axis-manager" onClick={(): void => setManager(true)}>{i18n.t('leaks:frameTree.manageMarkers')}</button>
        </div>
        <div data-testid="snapshot-axis-plot" data-width={width}
            style={{ position: 'relative', width, height: 240, marginTop: 48, background: colors.bgColorLight, boxShadow: `inset 0 0 0 1px ${colors.borderColor}` }}>
            <SnapshotEventFlag session={session} markers={markers}
                onRemove={(id): void => setMarkers(current => current.filter(marker => marker.id !== id))}
                onSelect={(coordinate): void => {
                setEventId(Math.floor(coordinate));
                setMarkers(current => [...current, { id: current.length + 1, coordinate, eventId: Math.floor(coordinate),
                    label: i18n.t('leaks:frameTree.marker', { event: Math.floor(coordinate) }) }]);
                setCommits(count => count + 1);
            }} />
            {manager ? <SnapshotEventMarkerManager markers={markers} onClose={(): void => setManager(false)}
                onUpdate={(id, updates): void => setMarkers(current => current.map(marker => marker.id === id ? { ...marker, ...updates } : marker))}
                onRemove={(id): void => setMarkers(current => current.filter(marker => marker.id !== id))}
                onClear={(): void => setMarkers([])} /> : null}
        </div>
    </div>;
});
window.setTheme(parameters.get('theme') === 'dark');
await i18n.changeLanguage(language);
const Preview = observer((): JSX.Element =>
    <ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <SharedConfigProvider locale={language as 'enUS' | 'zhCN'}>
            <div style={{ padding: 12, minWidth: 0 }}>
                {parameters.has('tabs')
                    ? <FrameTreeTabs type="editable-card" hideAdd items={[
                        { key: '1', label: '事件 1982', children: <p>First breakdown</p> },
                        { key: '2', label: '事件 2452', children: <p>Second breakdown</p> },
                    ]} />
                    : parameters.has('event-axis')
                    ? <EventAxisPreview />
                    : <FrameTreePanel context={{ datasetKey: 'test-fixture', deviceId: '0', eventId: 204412 }}
                        request={parameters.has('deep') ? deepRequest : parameters.has('presentation') ? presentationRequest : createMockFrameTreePage} />}
            </div>
        </SharedConfigProvider>
    </ThemeProvider>,
);
createRoot(document.getElementById('root')!).render(<Preview />);
