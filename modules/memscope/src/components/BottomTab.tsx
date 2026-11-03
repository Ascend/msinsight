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
import { NoData, DetailTabsWrapper, DetailContextMenu, DetailContextMenuItem, DetailTabLabel, DetailPanel, DetailCard, DetailFieldList, DetailField, DetailFieldName, DetailFieldValue } from '../common/shared/components/DetailStyles';
import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { Tabs } from 'antd';
import { DrawerButton, Resizer } from '@insight/lib';
import { type Theme, useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import MemoryTable from './MemoryTable';
import type { MemScopeContext as Session } from '../state/MemScopeContext';
import { observer } from 'mobx-react';

import { runInAction } from 'mobx';
import { getBlockRenderer } from '../common/leaksWorker/blockWorker/worker';

const MARGIN = 38;
const HEIGHT_DEFAULT = 420;
export const BottomTab = observer(({ session }: {
    session: Session;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [isExpand, setIsExpand] = useState(false);
    const [containerHeight, setContainerHeight] = useState(Math.min(HEIGHT_DEFAULT, window.innerHeight - 70));
    const [activeTab, setActiveTab] = useState('sliceDetail');
    const theme: Theme = useTheme();
    const autoExpandedKeysRef = useRef(new Set<string>());
    const detailContextKey = `${session.module}_${session.deviceId}_${session.eventType}_${-1}`;
    useEffect(() => {
        if (session.leaksWorkerInfo.clickItem !== null) {
            if (!autoExpandedKeysRef.current.has(detailContextKey)) {
                autoExpandedKeysRef.current.add(detailContextKey);
                setIsExpand(true);
                setActiveTab('sliceDetail');
            }
        }
    }, [detailContextKey, session.leaksWorkerInfo.clickItem]);
    useEffect(() => {
        if (session.deviceId !== '' || Object.keys(session.deviceIds).length > 0) {
            return;
        }
        autoExpandedKeysRef.current.clear();
        setActiveTab('sliceDetail');
    }, [session.deviceId, session.deviceIds]);
    const changeHeight = (_: number, moveY: number): void => {
        if (!isExpand) {
            return;
        }
        setContainerHeight(oVal => {
            const newHeight = oVal - moveY;
            if (newHeight < MARGIN) {
                return oVal;
            }
            if (newHeight > window.innerHeight - 70) {
                return oVal;
            }
            return newHeight;
        });
    };
    const TabItems = [
        {
            label: t('sliceDetail'),
            key: 'sliceDetail',
            children: <TabContentWrapper height={containerHeight}><SliceDetail session={session} detailContextKey={detailContextKey}/></TabContentWrapper>,
        },
        {
            label: t('systemView'),
            key: 'systemView',
            children: <TabContentWrapper height={containerHeight}><MemoryTable session={session} height={containerHeight - 50}/></TabContentWrapper>,
        },
    ];
    return <div style={{
        position: 'relative',
        overflow: 'hidden',
        background: theme.backgroundColor,
        height: isExpand ? containerHeight : MARGIN,
    }}>
        <Resizer style={{ width: '100%', height: 3, cursor: isExpand ? 'n-resize' : 'auto', zIndex: 1 }} callback={changeHeight}/>
        <DrawerButton isExpand={isExpand} onClick={() => setIsExpand(oVal => !oVal)} style={{ position: 'absolute', top: 0, left: '50%', transform: 'translateX(-50%)', zIndex: 1 }}/>
        <Tabs activeKey={activeTab} onChange={setActiveTab} size="small" items={TabItems} tabBarStyle={{ padding: '0 25px' }}/>
    </div>;
});
const TabContentWrapper = ({ children, height }: {
    children: React.ReactNode;
    height: number;
}): JSX.Element => {
    return <div style={{
        boxSizing: 'border-box',
        display: 'flex',
        flexDirection: 'column',
        minHeight: 0,
        overflow: 'hidden',
        height: height - 50,
        padding: '0 24px 8px',
    }}>
        {children}
    </div>;
};

const LEGACY_LOCAL_BLOCK_HIDDEN_KEYS = new Set([
    '_firstAccessTimestamp',
    '_lastAccessTimestamp',
    '_startTimestamp',
    '_endTimestamp',
    'maxAccessInterval',
    'lazyUsed',
    'delayedFree',
    'longIdle',
    'path',
]);
type SliceDetailKind = 'localBlock';
type LocalDetailData = {
    [key: string]: any;
};
interface DetailFieldEntry {
    key: string;
    value: React.ReactNode;
    label?: string;
}

interface SliceDetailTab {
    key: string;
    label: string;
    titleId?: number;
    kind: SliceDetailKind;
    loading?: boolean;
    detailData?: LocalDetailData;
    block?: Block;
    selection?: {
        type: 'block';
        block?: Block;
    };
}
interface SliceDetailState {
    tabs: SliceDetailTab[];
    activeKey: string;
}

const SliceDetail = observer(({ session, detailContextKey }: {
    session: Session;
    detailContextKey: string;
}): JSX.Element => {
    const { selectItem: workerSelectBlockItem } = getBlockRenderer(session);

    const { t } = useTranslation('leaks', { keyPrefix: 'slice' });
    const [noData, setNoData] = useState(false);
    const [detailTabs, setDetailTabs] = useState<SliceDetailTab[]>([]);
    const [activeDetailTabKey, setActiveDetailTabKey] = useState('');
    const [dragDetailTabKey, setDragDetailTabKey] = useState('');
    const [dragOverDetailTabKey, setDragOverDetailTabKey] = useState('');
    const detailTabLabelRefs = useRef(new Map<string, HTMLSpanElement>());
    const detailTabPositionsRef = useRef(new Map<string, DOMRect>());
    const detailStateCacheRef = useRef(new Map<string, SliceDetailState>());
    const detailContextKeyRef = useRef(detailContextKey);
    const detailTabsRef = useRef<SliceDetailTab[]>([]);
    const activeDetailTabKeyRef = useRef('');
    const deviceIdRef = useRef(session.deviceId);
    const [contextMenu, setContextMenu] = useState<{
        visible: boolean;
        x: number;
        y: number;
        key: string;
    }>({
        visible: false,
        x: 0,
        y: 0,
        key: '',
    });
    const getDetailTabTitle = (tab: SliceDetailTab): string => {
        if (tab.titleId !== undefined) {
            const labelKey = 'block';
            return `${t(labelKey)} #${tab.titleId}`;
        }
        return tab.label;
    };
    const getDetailTabLabel = (tab: SliceDetailTab): string => {
        return tab.titleId !== undefined ? `#${tab.titleId}` : getDetailTabTitle(tab);
    };
    const translateDetailKey = (key: string): string => {
        return t(key, { defaultValue: key });
    };

    const cloneBlock = (block: Block | null | undefined): Block | null => {
        if (block === null || block === undefined) {
            return null;
        }
        return {
            ...block,
            path: block.path.map((point): [
                number,
                number,
            ] => [point[0], point[1]]),
        };
    };

    useEffect(() => {
        detailTabsRef.current = detailTabs;
        detailStateCacheRef.current.set(detailContextKeyRef.current, {
            tabs: detailTabs,
            activeKey: activeDetailTabKeyRef.current,
        });
    }, [detailTabs]);
    useEffect(() => {
        activeDetailTabKeyRef.current = activeDetailTabKey;
        detailStateCacheRef.current.set(detailContextKeyRef.current, {
            tabs: detailTabsRef.current,
            activeKey: activeDetailTabKey,
        });
    }, [activeDetailTabKey]);
    const upsertDetailTab = (tab: SliceDetailTab, options?: {
        activate?: boolean;
    }): void => {
        setDetailTabs(tabs => {
            if (tabs.some(item => item.key === tab.key)) {
                return tabs.map(item => item.key === tab.key ? tab : item);
            }
            return [...tabs, tab];
        });
        if (options?.activate === true) {
            setActiveDetailTabKey(tab.key);
        }
    };

    const applyTabSelection = (tab: SliceDetailTab | undefined): void => {
        const block = cloneBlock(tab?.selection?.block);
        const selectionVersion = session.selectionVersion + 1;
        workerSelectBlockItem({ item: block, selectionVersion });
        runInAction(() => { session.selectionVersion = selectionVersion; session.leaksWorkerInfo.clickItem = block; });
    };
    const selectDetailTab = (targetKey: string): void => {
        setActiveDetailTabKey(targetKey);
        applyTabSelection(detailTabs.find(item => item.key === targetKey));
    };
    const clearDetailState = (): void => {
        detailStateCacheRef.current.clear();
        detailTabLabelRefs.current.clear();
        detailTabPositionsRef.current.clear();
        detailTabsRef.current = [];
        activeDetailTabKeyRef.current = '';
        setNoData(false);
        setContextMenu(menu => ({ ...menu, visible: false, key: '' }));
        setDragDetailTabKey('');
        setDragOverDetailTabKey('');
        setDetailTabs([]);
        setActiveDetailTabKey('');
        applyTabSelection(undefined);
    };
    useLayoutEffect(() => {
        const previousContextKey = detailContextKeyRef.current;
        if (previousContextKey === detailContextKey) {
            return;
        }
        detailStateCacheRef.current.set(previousContextKey, {
            tabs: detailTabsRef.current,
            activeKey: activeDetailTabKeyRef.current,
        });
        const cachedState = detailStateCacheRef.current.get(detailContextKey) ?? { tabs: [], activeKey: '' };
        const nextActiveKey = cachedState.tabs.some(tab => tab.key === cachedState.activeKey)
            ? cachedState.activeKey
            : (cachedState.tabs[0]?.key ?? '');
        detailContextKeyRef.current = detailContextKey;
        setNoData(false);
        setContextMenu(menu => ({ ...menu, visible: false }));
        setDetailTabs(cachedState.tabs);
        setActiveDetailTabKey(nextActiveKey);
        applyTabSelection(cachedState.tabs.find(tab => tab.key === nextActiveKey));
    }, [detailContextKey]);
    useEffect(() => {
        if (session.loadingBlocks) {
            return;
        }
        const activeTab = detailTabs.find(tab => tab.key === activeDetailTabKey);
        if (activeTab !== undefined) {
            applyTabSelection(activeTab);
        }
    }, [detailContextKey, session.loadingBlocks]);
    const getDetailTabElement = (tabKey: string): HTMLElement | null => {
        return (detailTabLabelRefs.current.get(tabKey)?.closest('.ant-tabs-tab') as HTMLElement | null) ?? null;
    };
    const captureDetailTabPositions = (): void => {
        const positions = new Map<string, DOMRect>();
        detailTabLabelRefs.current.forEach((_, key) => {
            const tabElement = getDetailTabElement(key);
            if (tabElement !== null) {
                positions.set(key, tabElement.getBoundingClientRect());
            }
        });
        detailTabPositionsRef.current = positions;
    };
    const reorderDetailTabs = (sourceKey: string, targetKey: string): void => {
        if (sourceKey === targetKey) {
            return;
        }
        captureDetailTabPositions();
        setDetailTabs(tabs => {
            const sourceIndex = tabs.findIndex(tab => tab.key === sourceKey);
            const targetIndex = tabs.findIndex(tab => tab.key === targetKey);
            if (sourceIndex < 0 || targetIndex < 0) {
                return tabs;
            }
            const nextTabs = [...tabs];
            const [sourceTab] = nextTabs.splice(sourceIndex, 1);
            nextTabs.splice(targetIndex, 0, sourceTab);
            return nextTabs;
        });
    };
    useLayoutEffect(() => {
        const previousPositions = detailTabPositionsRef.current;
        if (previousPositions.size < 1) {
            return;
        }
        detailTabLabelRefs.current.forEach((_, key) => {
            const tabElement = getDetailTabElement(key);
            const previousPosition = previousPositions.get(key);
            if (tabElement === null || previousPosition === undefined) {
                return;
            }
            const nextPosition = tabElement.getBoundingClientRect();
            const deltaX = previousPosition.left - nextPosition.left;
            const deltaY = previousPosition.top - nextPosition.top;
            if (Math.abs(deltaX) < 0.5 && Math.abs(deltaY) < 0.5) {
                return;
            }
            tabElement.animate([
                { transform: `translate(${deltaX}px, ${deltaY}px)` },
                { transform: 'translate(0, 0)' },
            ], {
                duration: 160,
                easing: 'ease',
            });
        });
        detailTabPositionsRef.current = new Map();
    }, [detailTabs]);
    const dragDetailTabStart = (ev: React.DragEvent<HTMLSpanElement>, tabKey: string): void => {
        ev.dataTransfer.effectAllowed = 'move';
        ev.dataTransfer.setData('text/plain', tabKey);
        setDragDetailTabKey(tabKey);
        setDragOverDetailTabKey('');
    };
    const dragDetailTabOver = (ev: React.DragEvent<HTMLSpanElement>): void => {
        ev.preventDefault();
        ev.dataTransfer.dropEffect = 'move';
    };
    const dragDetailTabEnter = (ev: React.DragEvent<HTMLSpanElement>, targetKey: string): void => {
        ev.preventDefault();
        const sourceKey = ev.dataTransfer.getData('text/plain') || dragDetailTabKey;
        if (sourceKey !== '' && sourceKey !== targetKey) {
            reorderDetailTabs(sourceKey, targetKey);
            setDragOverDetailTabKey(targetKey);
        }
    };
    const dropDetailTab = (ev: React.DragEvent<HTMLSpanElement>, targetKey: string): void => {
        ev.preventDefault();
        const sourceKey = ev.dataTransfer.getData('text/plain') || dragDetailTabKey;
        reorderDetailTabs(sourceKey, targetKey);
        setDragDetailTabKey('');
        setDragOverDetailTabKey('');
    };
    const removeDetailTab = (targetKey: string): void => {
        const targetIndex = detailTabs.findIndex(tab => tab.key === targetKey);
        const nextTabs = detailTabs.filter(tab => tab.key !== targetKey);
        if (activeDetailTabKey === targetKey) {
            const nextActive = nextTabs[Math.max(0, targetIndex - 1)]?.key ?? nextTabs[0]?.key ?? '';
            setActiveDetailTabKey(nextActive);
            applyTabSelection(nextTabs.find(tab => tab.key === nextActive));
        }
        setDetailTabs(nextTabs);
        setContextMenu(menu => ({ ...menu, visible: false }));
    };
    const closeOtherDetailTabs = (targetKey: string): void => {
        const targetTab = detailTabs.find(tab => tab.key === targetKey);
        const nextTabs = targetTab === undefined ? detailTabs : [targetTab];
        setDetailTabs(nextTabs);
        setActiveDetailTabKey(targetKey);
        applyTabSelection(targetTab);
        setContextMenu(menu => ({ ...menu, visible: false }));
    };
    const closeRightDetailTabs = (targetKey: string): void => {
        const targetIndex = detailTabs.findIndex(tab => tab.key === targetKey);
        const nextTabs = targetIndex < 0 ? detailTabs : detailTabs.slice(0, targetIndex + 1);
        if (!nextTabs.some(tab => tab.key === activeDetailTabKey)) {
            setActiveDetailTabKey(targetKey);
            applyTabSelection(nextTabs.find(tab => tab.key === targetKey));
        }
        setDetailTabs(nextTabs);
        setContextMenu(menu => ({ ...menu, visible: false }));
    };
    const closeAllDetailTabs = (): void => {
        setDetailTabs([]);
        setActiveDetailTabKey('');
        applyTabSelection(undefined);
        setContextMenu(menu => ({ ...menu, visible: false }));
    };
    const isRecordValue = (value: any): value is LocalDetailData => {
        return value !== null && typeof value === 'object' && !Array.isArray(value);
    };
    const hasVisibleValue = (value: any): boolean => {
        if (value === null || value === undefined || value === '') {
            return false;
        }
        if (Array.isArray(value)) {
            return value.length > 0;
        }
        if (isRecordValue(value)) {
            return Object.keys(value).length > 0;
        }
        return true;
    };
    const isStructuredValue = (value: any): boolean => {
        return Array.isArray(value) || isRecordValue(value);
    };
    const formatDetailValue = (value: any): string => {
        if (value === null || value === undefined || value === '') {
            return '-';
        }
        if (isStructuredValue(value)) {
            return JSON.stringify(value, null, 2);
        }
        return String(value);
    };
    const renderFieldList = (entries: DetailFieldEntry[]): JSX.Element => {
        if (entries.length < 1) {
            return <NoData>{t('empty')}</NoData>;
        }
        return <DetailFieldList>
            {entries.map((item, index) => <DetailField key={`${item.key}_${index}`}>
                <DetailFieldName title={item.label ?? translateDetailKey(item.key)}>{item.label ?? translateDetailKey(item.key)}</DetailFieldName>
                <DetailFieldValue>{item.value}</DetailFieldValue>
            </DetailField>)}
        </DetailFieldList>;
    };

    const getLocalBlockEntries = (block: Block): DetailFieldEntry[] => {
        return Object.entries(block)
            .filter(([key, value]) => !LEGACY_LOCAL_BLOCK_HIDDEN_KEYS.has(key) && hasVisibleValue(value))
            .map(([key, value]) => ({ key, value: formatDetailValue(value) }));
    };

    const renderDetailContent = (tab: SliceDetailTab): JSX.Element => tab.block === undefined
        ? <></>
        : <DetailPanel><DetailCard>{renderFieldList(getLocalBlockEntries(tab.block))}</DetailCard></DetailPanel>;
    useEffect(() => {
        setNoData(false);
        const block = cloneBlock(session.leaksWorkerInfo.clickItem);
        if (block === null) {
            return;
        }

        upsertDetailTab({
            key: `block_${block.id}`,
            label: `${t('block')} #${block.id}`,
            titleId: block.id,
            kind: 'localBlock',
            block,
            selection: { type: 'block', block },
        }, { activate: true });
    }, [session.leaksWorkerInfo.clickItem]);
    useEffect(() => {
        if (deviceIdRef.current !== session.deviceId) {
            detailStateCacheRef.current.clear();
            deviceIdRef.current = session.deviceId;
        }
        clearDetailState();
    }, [session.deviceId]);
    const tabItems = detailTabs.map(tab => ({
        key: tab.key,
        label: <DetailTabLabel ref={(node): void => {
            if (node === null) {
                detailTabLabelRefs.current.delete(tab.key);
                return;
            }
            detailTabLabelRefs.current.set(tab.key, node);
        }} draggable data-dragging={dragDetailTabKey === tab.key} data-drag-over={dragOverDetailTabKey === tab.key} title={getDetailTabTitle(tab)} onDragStart={(ev): void => dragDetailTabStart(ev, tab.key)} onDragEnter={(ev): void => dragDetailTabEnter(ev, tab.key)} onDragOver={dragDetailTabOver} onDrop={(ev): void => dropDetailTab(ev, tab.key)} onDragEnd={(): void => {
            setDragDetailTabKey('');
            setDragOverDetailTabKey('');
        }} onContextMenu={(ev): void => {
            ev.preventDefault();
            setContextMenu({ visible: true, x: ev.clientX, y: ev.clientY, key: tab.key });
        }}>{getDetailTabLabel(tab)}</DetailTabLabel>,
        children: tab.key === activeDetailTabKey ? renderDetailContent(tab) : null,
    }));
    if (detailTabs.length > 0) {
        return <DetailTabsWrapper onClick={(): void => setContextMenu(menu => ({ ...menu, visible: false }))}>
            <Tabs type="editable-card" hideAdd size="small" activeKey={activeDetailTabKey} items={tabItems} destroyInactiveTabPane onChange={selectDetailTab} onEdit={(targetKey, action): void => {
                if (action === 'remove') {
                    removeDetailTab(String(targetKey));
                }
            }}/>
            {contextMenu.visible
                ? <DetailContextMenu style={{ left: contextMenu.x, top: contextMenu.y }}>
                    <DetailContextMenuItem onClick={(): void => removeDetailTab(contextMenu.key)}>{t('close')}</DetailContextMenuItem>
                    <DetailContextMenuItem onClick={(): void => closeOtherDetailTabs(contextMenu.key)}>{t('closeOthers')}</DetailContextMenuItem>
                    <DetailContextMenuItem onClick={(): void => closeRightDetailTabs(contextMenu.key)}>{t('closeToRight')}</DetailContextMenuItem>
                    <DetailContextMenuItem onClick={closeAllDetailTabs}>{t('closeAll')}</DetailContextMenuItem>
                </DetailContextMenu>
                : <></>}
        </DetailTabsWrapper>;
    }
    return <>
        {noData
            ? <NoData>{(t('noData', { returnObjects: true }) as string[]).map((item, index) => <div key={index}>{item}</div>)}</NoData>
            : <NoData>{t('selectDetailTip')}</NoData>}
    </>;
});
