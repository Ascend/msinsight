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

import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { Spin, Tabs } from 'antd';
import { DrawerButton, Resizer, Button } from '@insight/lib';
import { type Theme, useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import MemoryTable from '../MemoryTable';
import { Session } from '../../entity/session';
import { observer } from 'mobx-react';
import styled from '@emotion/styled/macro';
import { getSnapshotDetail } from '@/utils/RequestUtils';
import { runInAction } from 'mobx';
import { workerSelectItem as workerSelectBlockItem } from '@/leaksWorker/blockWorker/worker';
import { workerSelectItem as workerSelectStateItem } from '@/leaksWorker/stateWorker/worker';

const MARGIN = 38;
const HEIGHT_DEFAULT = 300;

export const BottomTab = observer(({ session }: { session: Session }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [isExpand, setIsExpand] = useState(false);
    const [containerHeight, setContainerHeight] = useState(HEIGHT_DEFAULT);
    const [activeTab, setActiveTab] = useState('sliceDetail');
    const theme: Theme = useTheme();

    useEffect(() => {
        if (session.leaksWorkerInfo.clickItem !== null || session.stateWorkerInfo.clickItem !== null || session.clickEventItem !== null) {
            setIsExpand(true);
            setActiveTab('sliceDetail');
        }
    }, [session.leaksWorkerInfo.clickItem, session.stateWorkerInfo.clickItem, session.clickEventItem]);

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
            children: <TabContentWrapper height={containerHeight}><SliceDetail session={session} /></TabContentWrapper>,
        },
        {
            label: t('systemView'),
            key: 'systemView',
            children: <TabContentWrapper height={containerHeight}><MemoryTable session={session} /></TabContentWrapper>,
        },
    ];

    return <div
        style={{
            position: 'relative',
            overflow: 'hidden',
            background: theme.backgroundColor,
            height: isExpand ? containerHeight : MARGIN,
        }}>
        <Resizer
            style={{ width: '100%', height: 3, cursor: isExpand ? 'n-resize' : 'auto', zIndex: 1 }}
            callback={changeHeight}
        />
        <DrawerButton
            isExpand={isExpand} onClick={() => setIsExpand(oVal => !oVal)}
            style={{ position: 'absolute', top: 0, left: '50%', transform: 'translateX(-50%)', zIndex: 1 }}
        />
        <Tabs activeKey={activeTab} onChange={setActiveTab} size="small" items={TabItems} tabBarStyle={{ padding: '0 25px' }} />
    </div>;
});

const TabContentWrapper = ({ children, height }: { children: React.ReactNode; height: number }): JSX.Element => {
    return <div style={{ overflowY: 'auto', height: height - 50, padding: '0 25px' }}>
        {children}
    </div>;
};

const SliceDetailItem = styled.div`
    font-size: 14px;
    display: flex;
    color: ${(props): string => props.theme.tableTextColor};
    .sliceDetailName {
        width: 220px;
        font-weight: bold;
    }
    .sliceDetailValue {
        white-space: pre-wrap;
        flex: 1;
    }
`;

const NoData = styled.div`
    font-size: 14px;
    color: ${(props): string => props.theme.tableTextColor};
`;
const DetailTabsWrapper = styled.div`
    position: relative;
    display: flex;
    flex-direction: column;
    min-height: 0;
    height: 100%;
    > .ant-tabs {
        display: flex;
        flex: 1;
        flex-direction: column;
        min-height: 0;
    }
    > .ant-tabs > .ant-tabs-content-holder {
        flex: 1;
        min-height: 0;
        overflow: auto;
    }
    .ant-tabs-card > .ant-tabs-nav .ant-tabs-tab {
        padding: 2px 8px;
        font-size: 12px;
        line-height: 20px;
    }
`;
const DetailContextMenu = styled.div`
    position: fixed;
    z-index: 1000;
    min-width: 140px;
    padding: 4px 0;
    border: 1px solid ${(props): string => props.theme.textColorDisabled};
    border-radius: ${(props): string => props.theme.borderRadiusBase};
    background: ${(props): string => props.theme.contextMenuBgColor};
    box-shadow: ${(props): string => props.theme.boxShadowLight};
`;
const DetailContextMenuItem = styled.div`
    padding: 5px 12px;
    color: ${(props): string => props.theme.textColorPrimary};
    cursor: pointer;
    white-space: nowrap;
    &:hover {
        color: #ffffff;
        background: ${(props): string => props.theme.primaryColorHover};
    }
`;
const DetailLoading = styled.div`
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 120px;
`;

const DetailTabLabel = styled.span`
    display: inline-flex;
    align-items: center;
    overflow: hidden;
    cursor: default;
    text-overflow: ellipsis;
    transition: transform 160ms ease, color 160ms ease, opacity 160ms ease;
    user-select: none;
    white-space: nowrap;

    &[data-dragging='true'] {
        cursor: grabbing;
        opacity: 0.45;
    }

    &[data-dragging='true']::before {
        flex: 0 0 auto;
        width: 8px;
        height: 12px;
        margin-right: 4px;
        background-image: radial-gradient(currentColor 1px, transparent 1px);
        background-position: 0 0, 4px 0;
        background-size: 4px 4px;
        content: '';
        opacity: 0.65;
    }

    &[data-drag-over='true'] {
        color: ${(props): string => props.theme.primaryColor};
        transform: translateX(3px);
    }
`;

const hiddenList = ['_firstAccessTimestamp', '_lastAccessTimestamp', '_startTimestamp', '_endTimestamp',
    'maxAccessInterval', 'lazyUsed', 'delayedFree', 'longIdle', 'path'];

interface DetailEntry {
    key: string;
    value: any;
}

interface SliceDetailTab {
    key: string;
    label: string;
    titleId?: number;
    detailList: DetailEntry[];
    noData?: boolean;
    loading?: boolean;
    selection?: {
        type: 'block' | 'state' | 'event';
        block?: Block | null;
        state?: StateDataHoverResult | null;
        event?: any;
    };
}

const SliceDetail = observer(({ session }: { session: Session }): JSX.Element => {
    const { t } = useTranslation('leaks', { keyPrefix: 'slice' });
    const [detailTabs, setDetailTabs] = useState<SliceDetailTab[]>([]);
    const [activeDetailTabKey, setActiveDetailTabKey] = useState('');
    const [dragDetailTabKey, setDragDetailTabKey] = useState('');
    const [dragOverDetailTabKey, setDragOverDetailTabKey] = useState('');
    const detailTabLabelRefs = useRef(new Map<string, HTMLSpanElement>());
    const detailTabPositionsRef = useRef(new Map<string, DOMRect>());
    const [contextMenu, setContextMenu] = useState<{ visible: boolean; x: number; y: number; key: string }>({
        visible: false,
        x: 0,
        y: 0,
        key: '',
    });

    const cloneBlock = (block: Block | null | undefined): Block | null => {
        if (block === null || block === undefined) {
            return null;
        }
        return {
            ...block,
            path: block.path.map((point): [number, number] => [point[0], point[1]]),
        };
    };

    const cloneStateSelection = (item: StateDataHoverResult | null | undefined): StateDataHoverResult | null => {
        if (item === null || item === undefined) {
            return null;
        }
        return {
            type: item.type,
            data: {
                ...item.data,
                blocks: item.data.blocks.map(block => ({ ...block })),
            },
        };
    };

    const getSnapshotDetailInfo = async (type: string, id: number, deviceId: string): Promise<DetailEntry[]> => {
        const data = await getSnapshotDetail({ type, id, deviceId });
        const result: DetailEntry[] = [];
        Object.entries(data).forEach(([key, value]) => {
            if (typeof value === 'object') {
                if (Object.keys(value).length < 1) {
                    return;
                }
                result.push({ key, value: <SliceDetailObjectItem data={value} /> });
            } else {
                result.push({ key, value });
            }
        });
        return result;
    };

    const upsertDetailTab = (tab: SliceDetailTab, options?: { activate?: boolean }): void => {
        const nextTab = { ...tab, loading: tab.loading ?? false };
        setDetailTabs(tabs => {
            const index = tabs.findIndex(item => item.key === nextTab.key);
            if (index < 0) {
                return [...tabs, nextTab];
            }
            const nextTabs = [...tabs];
            nextTabs[index] = { ...nextTabs[index], ...nextTab };
            return nextTabs;
        });
        if (options?.activate === true) {
            setActiveDetailTabKey(nextTab.key);
        }
    };

    const openPendingDetailTab = (tab: SliceDetailTab): void => {
        setDetailTabs(tabs => {
            if (tabs.some(item => item.key === tab.key)) {
                return tabs;
            }
            return [...tabs, tab];
        });
        setActiveDetailTabKey(tab.key);
    };

    const applyTabSelection = (tab: SliceDetailTab | undefined): void => {
        const selectionVersion = session.selectionVersion + 1;
        if (tab?.selection?.type === 'block') {
            const block = cloneBlock(tab.selection.block);
            workerSelectBlockItem({ item: block, selectionVersion });
            workerSelectStateItem({ item: null, selectionVersion });
            runInAction(() => {
                session.selectionVersion = selectionVersion;
                session.leaksWorkerInfo.clickItem = block;
                session.stateWorkerInfo.clickItem = null;
                session.clickEventItem = null;
            });
            return;
        }
        if (tab?.selection?.type === 'state') {
            const state = cloneStateSelection(tab.selection.state);
            workerSelectBlockItem({ item: null, selectionVersion });
            workerSelectStateItem({ item: state, selectionVersion });
            runInAction(() => {
                session.selectionVersion = selectionVersion;
                session.leaksWorkerInfo.clickItem = null;
                session.stateWorkerInfo.clickItem = state;
                session.clickEventItem = null;
            });
            return;
        }
        workerSelectBlockItem({ item: null, selectionVersion });
        workerSelectStateItem({ item: null, selectionVersion });
        runInAction(() => {
            session.selectionVersion = selectionVersion;
            session.leaksWorkerInfo.clickItem = null;
            session.stateWorkerInfo.clickItem = null;
            session.clickEventItem = tab?.selection?.event ?? null;
        });
    };

    const selectDetailTab = (targetKey: string): void => {
        setActiveDetailTabKey(targetKey);
        applyTabSelection(detailTabs.find(item => item.key === targetKey));
    };

    const closeDetailTab = (targetKey: string): void => {
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

    const closeOtherDetailTabs = (targetKey: string): void => {
        const targetTab = detailTabs.find(tab => tab.key === targetKey);
        setDetailTabs(targetTab === undefined ? detailTabs : [targetTab]);
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

    const renderDetailContent = (tab: SliceDetailTab): JSX.Element => {
        if (tab.loading === true) {
            return <DetailLoading><Spin size="small" /></DetailLoading>;
        }
        if (tab.noData === true) {
            return <NoData>{(t('noData', { returnObjects: true }) as string[]).map((item, index) => <div key={index}>{item}</div>)}</NoData>;
        }
        return <>
            {tab.detailList.map(item => (<SliceDetailItem key={item.key} >
                <div className="sliceDetailName">{t(item.key)}</div>
                <div className="sliceDetailValue">{item.value}</div>
            </SliceDetailItem>))}
        </>;
    };

    useEffect(() => {
        if (session.module === 'leaks') {
            if (session.leaksWorkerInfo.clickItem === null) {
                return;
            }
            const block = cloneBlock(session.leaksWorkerInfo.clickItem);
            const result: DetailEntry[] = [];
            Object.entries(session.leaksWorkerInfo.clickItem).forEach(([key, value]) => {
                if (hiddenList.includes(key)) {
                    return;
                }
                result.push({ key, value });
            });
            upsertDetailTab({
                key: `local_block_${session.leaksWorkerInfo.clickItem.id}`,
                label: `${t('block')} #${session.leaksWorkerInfo.clickItem.id}`,
                titleId: session.leaksWorkerInfo.clickItem.id,
                detailList: result,
                selection: { type: 'block', block },
            }, { activate: true });
            return;
        }
        const block = cloneBlock(session.leaksWorkerInfo.clickItem);
        const id = block?.id;
        if (id === undefined) {
            return;
        }
        let cancelled = false;
        const requestDeviceId = session.deviceId;
        const tabKey = `snapshot_block_${id}`;
        openPendingDetailTab({
            key: tabKey,
            label: `${t('block')} #${id}`,
            titleId: id,
            detailList: [],
            loading: true,
            selection: { type: 'block', block },
        });
        getSnapshotDetailInfo('block', id, session.deviceId).then(result => {
            if (cancelled || session.deviceId !== requestDeviceId) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('block')} #${id}`,
                titleId: id,
                detailList: result,
                selection: { type: 'block', block },
            });
        }).catch(() => {
            if (cancelled || session.deviceId !== requestDeviceId) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('block')} #${id}`,
                titleId: id,
                detailList: [],
                noData: true,
                selection: { type: 'block', block },
            });
        });
        return () => {
            cancelled = true;
        };
    }, [session.leaksWorkerInfo.clickItem]);

    useEffect(() => {
        const stateSelection = cloneStateSelection(session.stateWorkerInfo.clickItem);
        if (stateSelection === null) {
            return;
        }
        const { type, data } = stateSelection;
        const id = type === 'segment' ? data.allocOrMapEventId : (data.blocks[0]?.id ?? -1);
        if (id < 0) {
            const noDataKey = `snapshot_state_no_frames_${type}_${Date.now()}`;
            upsertDetailTab({
                key: noDataKey,
                label: type === 'segment' ? t('exceptionEvent') : t('exceptionBlock'),
                detailList: [],
                noData: true,
                selection: { type: 'state', state: stateSelection },
            }, { activate: true });
            return;
        }
        let cancelled = false;
        const requestDeviceId = session.deviceId;
        const detailType = type === 'segment' ? 'event' : 'block';
        const tabKey = `snapshot_${detailType}_${id}`;
        openPendingDetailTab({
            key: tabKey,
            label: `${detailType === 'event' ? t('event') : t('block')} #${id}`,
            titleId: id,
            detailList: [],
            loading: true,
            selection: { type: 'state', state: stateSelection },
        });
        getSnapshotDetailInfo(detailType, id, session.deviceId).then(result => {
            if (cancelled || session.deviceId !== requestDeviceId) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${detailType === 'event' ? t('event') : t('block')} #${id}`,
                titleId: id,
                detailList: result,
                selection: { type: 'state', state: stateSelection },
            });
        }).catch(() => {
            if (cancelled || session.deviceId !== requestDeviceId) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${detailType === 'event' ? t('event') : t('block')} #${id}`,
                titleId: id,
                detailList: [],
                noData: true,
                selection: { type: 'state', state: stateSelection },
            });
        });
        return () => {
            cancelled = true;
        };
    }, [session.stateWorkerInfo.clickItem]);

    useEffect(() => {
        if (session.clickEventItem === null) {
            return;
        }
        const event = { ...session.clickEventItem };
        let cancelled = false;
        const requestDeviceId = session.deviceId;
        const tabKey = `snapshot_event_${event.id}`;
        openPendingDetailTab({
            key: tabKey,
            label: `${t('event')} #${event.id}`,
            titleId: event.id,
            detailList: [],
            loading: true,
            selection: { type: 'event', event },
        });
        getSnapshotDetailInfo('event', event.id, session.deviceId).then(result => {
            if (cancelled || session.deviceId !== requestDeviceId) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('event')} #${event.id}`,
                titleId: event.id,
                detailList: result,
                selection: { type: 'event', event },
            });
        }).catch(() => {
            if (cancelled || session.deviceId !== requestDeviceId) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('event')} #${event.id}`,
                titleId: event.id,
                detailList: [],
                noData: true,
                selection: { type: 'event', event },
            });
        });
        return () => {
            cancelled = true;
        };
    }, [session.clickEventItem]);

    useEffect(() => {
        setDetailTabs([]);
        setActiveDetailTabKey('');
    }, [session.deviceId]);

    const tabItems = detailTabs.map(tab => ({
        key: tab.key,
        label: <DetailTabLabel
            ref={(node): void => {
                if (node === null) {
                    detailTabLabelRefs.current.delete(tab.key);
                    return;
                }
                detailTabLabelRefs.current.set(tab.key, node);
            }}
            draggable
            data-dragging={dragDetailTabKey === tab.key}
            data-drag-over={dragOverDetailTabKey === tab.key}
            title={tab.label}
            onDragStart={(ev): void => dragDetailTabStart(ev, tab.key)}
            onDragEnter={(ev): void => dragDetailTabEnter(ev, tab.key)}
            onDragOver={dragDetailTabOver}
            onDrop={(ev): void => dropDetailTab(ev, tab.key)}
            onDragEnd={(): void => {
                setDragDetailTabKey('');
                setDragOverDetailTabKey('');
            }}
            onContextMenu={(ev): void => {
                ev.preventDefault();
                setContextMenu({ visible: true, x: ev.clientX, y: ev.clientY, key: tab.key });
            }}>{tab.titleId !== undefined ? `#${tab.titleId}` : tab.label}</DetailTabLabel>,
        children: tab.key === activeDetailTabKey ? renderDetailContent(tab) : null,
    }));

    if (detailTabs.length > 0) {
        return <DetailTabsWrapper onClick={(): void => setContextMenu(menu => ({ ...menu, visible: false }))}>
            <Tabs
                type="editable-card"
                size="small"
                hideAdd
                activeKey={activeDetailTabKey}
                items={tabItems}
                destroyInactiveTabPane
                onChange={selectDetailTab}
                onEdit={(targetKey, action): void => {
                    if (action === 'remove') {
                        closeDetailTab(String(targetKey));
                    }
                }}
            />
            {contextMenu.visible
                ? <DetailContextMenu style={{ left: contextMenu.x, top: contextMenu.y }}>
                    <DetailContextMenuItem onClick={(): void => closeDetailTab(contextMenu.key)}>{t('close')}</DetailContextMenuItem>
                    <DetailContextMenuItem onClick={(): void => closeOtherDetailTabs(contextMenu.key)}>{t('closeOthers')}</DetailContextMenuItem>
                    <DetailContextMenuItem onClick={(): void => closeRightDetailTabs(contextMenu.key)}>{t('closeToRight')}</DetailContextMenuItem>
                    <DetailContextMenuItem onClick={closeAllDetailTabs}>{t('closeAll')}</DetailContextMenuItem>
                </DetailContextMenu>
                : null}
        </DetailTabsWrapper>;
    }

    return <NoData>{t('selectDetailTip')}</NoData>;
});
const SliceDetailObjectItem = observer(({ data }: { data: { [key: string]: any } }): JSX.Element => {
    const { t } = useTranslation('leaks', { keyPrefix: 'slice' });
    const [isExpand, setIsExpand] = useState(false);
    const dataList: Array<{ key: string; value: string }> = [];
    Object.entries(data).forEach(([key, value]) => {
        if (typeof value === 'object') {
            dataList.push({ key, value: JSON.stringify(value) });
        } else {
            dataList.push({ key, value });
        }
    });

    return <>
        <Button type="link" size="small" style={{ padding: 0, minWidth: 0 }}
            onClick={() => setIsExpand(oVal => !oVal)} >
            {`${isExpand ? '-' : '+'} ${t('detail')}`}
        </Button>
        {
            isExpand && dataList.map(item => (<SliceDetailItem key={item.key} >
                <div className="sliceDetailName">{t(item.key)}</div>
                <div className="sliceDetailValue">{item.value}</div>
            </SliceDetailItem>))
        }
    </>;
});
