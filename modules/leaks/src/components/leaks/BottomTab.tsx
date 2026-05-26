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
import { DrawerButton, Resizer } from '@insight/lib';
import { type Theme, useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import MemoryTable from '../MemoryTable';
import { Session } from '../../entity/session';
import { observer } from 'mobx-react';
import styled from '@emotion/styled/macro';
import { getSnapshotDetail, type EvenItem } from '@/utils/RequestUtils';
import { runInAction } from 'mobx';
import { workerSelectItem as workerSelectBlockItem } from '@/leaksWorker/blockWorker/worker';
import { workerSelectItem as workerSelectStateItem } from '@/leaksWorker/stateWorker/worker';
import { addAddressOffset, convertBytesToMBytes } from '@/utils/utils';

const MARGIN = 38;
const HEIGHT_DEFAULT = 420;

export const BottomTab = observer(({ session }: { session: Session }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [isExpand, setIsExpand] = useState(false);
    const [containerHeight, setContainerHeight] = useState(Math.min(HEIGHT_DEFAULT, window.innerHeight - 70));
    const [activeTab, setActiveTab] = useState('sliceDetail');
    const theme: Theme = useTheme();
    const autoExpandedKeysRef = useRef(new Set<string>());
    const detailContextKey = `${session.module}_${session.deviceId}_${session.eventType}`;

    useEffect(() => {
        if (session.leaksWorkerInfo.clickItem !== null || session.stateWorkerInfo.clickItem !== null || session.clickEventItem !== null) {
            if (!autoExpandedKeysRef.current.has(detailContextKey)) {
                autoExpandedKeysRef.current.add(detailContextKey);
                setIsExpand(true);
                setActiveTab('sliceDetail');
            }
        }
    }, [detailContextKey, session.leaksWorkerInfo.clickItem, session.stateWorkerInfo.clickItem, session.clickEventItem]);

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
            children: <TabContentWrapper height={containerHeight}><SliceDetail session={session} detailContextKey={detailContextKey} /></TabContentWrapper>,
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

    > .ant-tabs > .ant-tabs-nav {
        margin-bottom: 6px;
    }

    > .ant-tabs > .ant-tabs-content-holder {
        flex: 1;
        min-height: 0;
        overflow: hidden;
    }

    > .ant-tabs > .ant-tabs-content-holder > .ant-tabs-content {
        height: 100%;
    }

    > .ant-tabs > .ant-tabs-content-holder > .ant-tabs-content > .ant-tabs-tabpane {
        height: 100%;
        overflow: hidden;
    }

    .ant-tabs-card > .ant-tabs-nav .ant-tabs-tab {
        padding: 2px 8px;
        border-color: ${(props): string => props.theme.borderColorLight};
        background: ${(props): string => props.theme.bgColor};
        font-size: 12px;
        line-height: 20px;
        transition: transform 160ms ease, background-color 160ms ease, opacity 160ms ease;
    }

    .ant-tabs-card > .ant-tabs-nav .ant-tabs-tab-active {
        background: ${(props): string => props.theme.bgColorLight};
    }

    .ant-tabs-tab-remove {
        margin-left: 4px;
        font-size: 10px;
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

const DetailTabLabel = styled.span`
    position: relative;
    display: inline-flex;
    align-items: center;
    max-width: 120px;
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

const DetailPanel = styled.div`
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    min-height: 0;
    height: 100%;
    gap: 0;
    padding: 2px 0 8px;
    color: ${(props): string => props.theme.tableTextColor};
`;

const DetailLoading = styled.div`
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 120px;
`;

const DetailSection = styled.section`
    min-height: 0;
    padding: 8px 0;
    border-top: 1px solid ${(props): string => props.theme.borderColorLight};

    &:first-of-type {
        padding-top: 0;
        border-top: 0;
    }
`;

const DetailMainLayout = styled.div`
    display: grid;
    grid-template-columns: minmax(280px, 360px) minmax(0, 1fr);
    flex: 1;
    min-height: 0;
    gap: 14px;
    align-items: stretch;

    @media (max-width: 920px) {
        grid-template-columns: 1fr;
    }
`;

const DetailCard = styled.div`
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    padding: 10px 12px;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColorLight};
`;

const DetailColumn = styled.div`
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    overflow: auto;
`;

const DetailSectionTitle = styled.div`
    margin-bottom: 8px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    font-weight: 600;
`;

const NoFramesNotice = styled.div`
    box-sizing: border-box;
    min-width: 0;
    max-width: 720px;
    color: ${(props): string => props.theme.tableTextColor};
    font-size: 13px;
    line-height: 20px;
`;

const NoFramesNoticeLine = styled.div`
    &:not(:first-of-type) {
        margin-top: 4px;
    }
`;

const DetailFieldList = styled.div`
    display: flex;
    flex-direction: column;
    gap: 4px;
    --detail-label-width: minmax(168px, 48%);
`;

const DetailField = styled.div`
    display: grid;
    grid-template-columns: var(--detail-label-width) minmax(0, 1fr);
    column-gap: 12px;
    min-width: 0;
    align-items: start;
    font-size: 13px;
    line-height: 20px;

    @media (max-width: 720px) {
        --detail-label-width: 1fr;
        grid-template-columns: 1fr;
        row-gap: 2px;
    }
`;

const DetailFieldName = styled.div`
    min-width: 0;
    overflow: hidden;
    color: ${(props): string => props.theme.textColorSecondary};
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const DetailFieldValue = styled.div`
    min-width: 0;
    overflow-wrap: anywhere;
    white-space: pre-wrap;
`;

const EventTabsWrapper = styled.div`
    display: grid;
    grid-template-columns: max-content minmax(0, 1fr);
    gap: 14px;
    min-height: 0;
    height: 100%;

    @media (max-width: 920px) {
        grid-template-columns: 1fr;
    }
`;

const EventTypeList = styled.div`
    display: flex;
    flex-direction: column;
    align-self: start;
    width: max-content;
    max-width: 154px;
    padding: 3px;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColor};
`;

const EventTypeButton = styled.button`
    max-width: 150px;
    min-height: 26px;
    padding: 0 10px;
    overflow: hidden;
    border: 0;
    border-radius: 0 3px 3px 0;
    background: transparent;
    color: ${(props): string => props.theme.textColorSecondary};
    cursor: pointer;
    font-size: 12px;
    line-height: 24px;
    text-align: left;
    text-overflow: ellipsis;
    white-space: nowrap;

    &[data-active='true'] {
        background: ${(props): string => props.theme.bgColorLight};
        color: ${(props): string => props.theme.textColorPrimary};
        box-shadow: inset 2px 0 0 ${(props): string => props.theme.primaryColor};
    }

    &:hover {
        color: ${(props): string => props.theme.textColorPrimary};
    }
`;

const EventTabContent = styled.div`
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    padding: 10px 12px;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColorLight};
`;

const EventDetailLayout = styled.div`
    display: grid;
    grid-template-columns: minmax(260px, 340px) minmax(0, 1fr);
    flex: 1;
    min-height: 0;
    height: 100%;
    gap: 14px;
    align-items: stretch;

    @media (max-width: 920px) {
        grid-template-columns: 1fr;
    }
`;

const EventDetailSingleColumn = styled.div`
    min-width: 0;
    min-height: 0;
    height: 100%;
    overflow: auto;
`;

const DetailPre = styled.pre`
    box-sizing: border-box;
    max-height: 100%;
    margin: 0;
    padding: 6px 8px;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColor};
    color: ${(props): string => props.theme.tableTextColor};
    font-size: 12px;
    line-height: 18px;
    white-space: pre-wrap;
    word-break: break-word;
`;

const CallStackViewer = styled.div`
    box-sizing: border-box;
    flex: 1;
    min-height: 0;
    height: 100%;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColor};
`;

const CallStackGroup = styled.div`
    &:not(:first-of-type) {
        border-top: 1px solid ${(props): string => props.theme.borderColorLight};
    }
`;

const CallStackGroupTitle = styled.div`
    padding: 5px 8px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    font-weight: 600;
`;

const CallStackLine = styled.div`
    display: grid;
    grid-template-columns: 32px minmax(0, 1fr);
    min-width: 0;
    padding: 1px 8px 1px 0;
    font-family: Consolas, "Courier New", monospace;
    font-size: 12px;
    line-height: 17px;
`;

const CallStackLineNumber = styled.div`
    color: ${(props): string => props.theme.textColorSecondary};
    text-align: right;
    user-select: none;
`;

const CallStackLineText = styled.div`
    min-width: 0;
    padding-left: 8px;
    overflow-wrap: anywhere;
    white-space: pre-wrap;
`;

const SNAPSHOT_EVENT_KEYS = ['Alloc Event', 'Free Requested Event', 'Free Completed Event'];
const CALL_STACK_KEYS = ['CallStack', 'Callstack'];
const SNAPSHOT_NESTED_KEYS = new Set([...SNAPSHOT_EVENT_KEYS, ...CALL_STACK_KEYS]);
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

type SliceDetailKind = 'localBlock' | 'snapshotBlock' | 'snapshotEvent' | 'stateBlock' | 'stateEvent';
type SnapshotDetailData = { [key: string]: any };
interface DetailFieldEntry {
    key: string;
    value: React.ReactNode;
    label?: string;
}

interface DetailEventTab {
    key: string;
    label: string;
    value: any;
}

interface SliceDetailTab {
    key: string;
    label: string;
    titleId?: number;
    kind: SliceDetailKind;
    loading?: boolean;
    detailData?: SnapshotDetailData;
    block?: Block;
    state?: StateDataHoverResult;
    event?: EvenItem;
    selection?: {
        type: 'block' | 'state' | 'event';
        block?: Block;
        state?: StateDataHoverResult;
        event?: EvenItem;
    };
}

interface SliceDetailState {
    tabs: SliceDetailTab[];
    activeKey: string;
}

const EventDetailSwitcher = ({
    events,
    renderContent,
}: {
    events: DetailEventTab[];
    renderContent: (event: DetailEventTab) => JSX.Element;
}): JSX.Element => {
    const [activeKey, setActiveKey] = useState(events[0]?.key ?? '');
    const resolvedActiveKey = events.some(item => item.key === activeKey) ? activeKey : (events[0]?.key ?? '');
    const activeEvent = events.find(item => item.key === resolvedActiveKey);

    if (activeEvent === undefined) {
        return <></>;
    }

    return <EventTabsWrapper>
        <EventTypeList role="tablist">
            {events.map(item => <EventTypeButton
                key={item.key}
                type="button"
                role="tab"
                aria-selected={item.key === resolvedActiveKey}
                data-active={item.key === resolvedActiveKey}
                title={item.label}
                onClick={(): void => setActiveKey(item.key)}
            >
                {item.label}
            </EventTypeButton>)}
        </EventTypeList>
        <EventTabContent>{renderContent(activeEvent)}</EventTabContent>
    </EventTabsWrapper>;
};

const SliceDetail = observer(({ session, detailContextKey }: { session: Session; detailContextKey: string }): JSX.Element => {
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
    const [contextMenu, setContextMenu] = useState<{ visible: boolean; x: number; y: number; key: string }>({
        visible: false,
        x: 0,
        y: 0,
        key: '',
    });

    const getDetailTabTitle = (tab: SliceDetailTab): string => {
        if (tab.titleId !== undefined) {
            const labelKey = tab.kind === 'snapshotEvent' || tab.kind === 'stateEvent' ? 'event' : 'block';
            return `${t(labelKey)} #${tab.titleId}`;
        }
        if (tab.kind === 'stateEvent') {
            return t('exceptionEvent');
        }
        if (tab.kind === 'stateBlock') {
            return t('exceptionBlock');
        }
        return tab.label;
    };

    const getDetailTabLabel = (tab: SliceDetailTab): string => {
        return tab.titleId !== undefined ? `#${tab.titleId}` : getDetailTabTitle(tab);
    };

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

    const getSnapshotDetailData = async (type: string, id: number, deviceId: string): Promise<SnapshotDetailData> => {
        return getSnapshotDetail({ type, id, deviceId });
    };

    const normalizeStateBlockDetailData = (detailData: SnapshotDetailData, stateSelection: StateDataHoverResult): SnapshotDetailData => {
        if (stateSelection.type !== 'block') {
            return detailData;
        }
        const block = stateSelection.data.blocks[0];
        if (block === undefined) {
            return detailData;
        }
        return {
            ...detailData,
            Address: addAddressOffset(stateSelection.data.address, block.offset),
            'Size(MBytes)': convertBytesToMBytes(block.size),
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

    const upsertDetailTab = (tab: SliceDetailTab, options?: { activate?: boolean }): void => {
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

    const openPendingDetailTab = (tab: SliceDetailTab): void => {
        setDetailTabs(tabs => {
            if (tabs.some(item => item.key === tab.key)) {
                return tabs;
            }
            return [...tabs, tab];
        });
        setActiveDetailTabKey(tab.key);
    };

    const upsertNoFramesStateTab = (stateSelection: StateDataHoverResult): void => {
        const { type, data } = stateSelection;
        const block = data.blocks[0];
        const tabKeyParts = [
            type,
            data.address,
            data.stream,
            data.offsetX,
            block?.offset ?? '',
            block?.size ?? data.size,
        ];
        upsertDetailTab({
            key: `snapshot_state_no_frames_${tabKeyParts.map(String).join('_')}`,
            label: type === 'segment' ? t('exceptionEvent') : t('exceptionBlock'),
            kind: type === 'segment' ? 'stateEvent' : 'stateBlock',
            detailData: {},
            state: stateSelection,
            selection: { type: 'state', state: stateSelection },
        }, { activate: true });
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
        if (tab?.selection?.type === 'event') {
            const event = tab.selection.event === undefined ? null : { ...tab.selection.event };
            workerSelectBlockItem({ item: null, selectionVersion });
            workerSelectStateItem({ item: null, selectionVersion });
            runInAction(() => {
                session.selectionVersion = selectionVersion;
                session.leaksWorkerInfo.clickItem = null;
                session.stateWorkerInfo.clickItem = null;
                session.clickEventItem = event;
            });
            return;
        }
        workerSelectBlockItem({ item: null, selectionVersion });
        workerSelectStateItem({ item: null, selectionVersion });
        runInAction(() => {
            session.selectionVersion = selectionVersion;
            session.leaksWorkerInfo.clickItem = null;
            session.stateWorkerInfo.clickItem = null;
            session.clickEventItem = null;
        });
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
        if (session.loadingBlocks || session.loadingState) {
            return;
        }
        const activeTab = detailTabs.find(tab => tab.key === activeDetailTabKey);
        if (activeTab !== undefined) {
            applyTabSelection(activeTab);
        }
    }, [detailContextKey, session.loadingBlocks, session.loadingState]);

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

    const isRecordValue = (value: any): value is SnapshotDetailData => {
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
                <DetailFieldName title={item.label ?? t(item.key)}>{item.label ?? t(item.key)}</DetailFieldName>
                <DetailFieldValue>{item.value}</DetailFieldValue>
            </DetailField>)}
        </DetailFieldList>;
    };

    const renderContentSection = (title: string, children: React.ReactNode, visible: boolean = true): JSX.Element => {
        if (!visible) {
            return <></>;
        }
        return <DetailSection>
            <DetailSectionTitle>{title}</DetailSectionTitle>
            {children}
        </DetailSection>;
    };

    const renderNoFramesNotice = (): JSX.Element => {
        return <NoFramesNotice>
            {(t('noData', { returnObjects: true }) as string[]).map((item, index) => (
                <NoFramesNoticeLine key={index}>{item}</NoFramesNoticeLine>
            ))}
        </NoFramesNotice>;
    };

    const getSnapshotOverviewEntries = (data?: SnapshotDetailData): DetailFieldEntry[] => {
        if (data === undefined) {
            return [];
        }
        return Object.entries(data)
            .filter(([key, value]) => !SNAPSHOT_NESTED_KEYS.has(key) && !isStructuredValue(value) && hasVisibleValue(value))
            .map(([key, value]) => ({ key, value: formatDetailValue(value) }));
    };

    const getSnapshotExtraEntries = (data?: SnapshotDetailData): Array<{ key: string; value: any }> => {
        if (data === undefined) {
            return [];
        }
        return Object.entries(data)
            .filter(([key, value]) => !SNAPSHOT_NESTED_KEYS.has(key) && isStructuredValue(value) && hasVisibleValue(value))
            .map(([key, value]) => ({ key, value }));
    };

    const getRecordFieldEntries = (data: SnapshotDetailData, excludedKeys: Set<string> = new Set()): DetailFieldEntry[] => {
        return Object.entries(data)
            .filter(([key, value]) => !excludedKeys.has(key) && hasVisibleValue(value))
            .map(([key, value]) => ({
                key,
                value: isStructuredValue(value) ? <DetailPre>{formatDetailValue(value)}</DetailPre> : formatDetailValue(value),
            }));
    };

    const renderObjectValue = (key: string, value: any): JSX.Element => {
        if (!isRecordValue(value)) {
            return <DetailPre>{formatDetailValue(value)}</DetailPre>;
        }
        const scalarEntries = getRecordFieldEntries(value);
        return renderFieldList(scalarEntries.length > 0 ? scalarEntries : [{ key, value: '-' }]);
    };

    const getCallStackValue = (data?: SnapshotDetailData): any => {
        return CALL_STACK_KEYS.map(key => data?.[key]).find(value => hasVisibleValue(value));
    };

    const normalizeCallStackLines = (value: any): string[] => {
        if (!hasVisibleValue(value)) {
            return [];
        }
        if (typeof value === 'string') {
            return value.split(/\r?\n/).map(item => item.trim()).filter(item => item.length > 0);
        }
        if (Array.isArray(value)) {
            return value.flatMap(item => normalizeCallStackLines(item));
        }
        if (isRecordValue(value)) {
            return [Object.entries(value)
                .map(([key, item]) => `${t(key)}: ${formatDetailValue(item)}`)
                .join('  ')];
        }
        return [formatDetailValue(value)];
    };

    const renderCallStackValue = (value: any): JSX.Element => {
        const groups = isRecordValue(value)
            ? Object.entries(value)
                .filter(([, item]) => hasVisibleValue(item))
                .map(([key, item]) => ({ key, label: t(key), lines: normalizeCallStackLines(item) }))
                .filter(group => group.lines.length > 0)
            : [{ key: 'callStack', label: t('callStack'), lines: normalizeCallStackLines(value) }];

        if (groups.length < 1) {
            return <NoData>{t('empty')}</NoData>;
        }

        return <CallStackViewer>
            {groups.map(group => <CallStackGroup key={group.key}>
                <CallStackGroupTitle>{group.label}</CallStackGroupTitle>
                {group.lines.map((line, index) => <CallStackLine key={`${group.key}_${index}`}>
                    <CallStackLineNumber>{index + 1}</CallStackLineNumber>
                    <CallStackLineText>{line}</CallStackLineText>
                </CallStackLine>)}
            </CallStackGroup>)}
        </CallStackViewer>;
    };

    const getRelatedEvents = (data?: SnapshotDetailData): Array<{ key: string; value: any }> => {
        return SNAPSHOT_EVENT_KEYS
            .map(key => ({ key, value: data?.[key] }))
            .filter(item => hasVisibleValue(item.value));
    };

    const renderEventDetailContent = (value: any): JSX.Element => {
        if (!isRecordValue(value)) {
            return <DetailPre>{formatDetailValue(value)}</DetailPre>;
        }
        const callStackValue = getCallStackValue(value);
        const fieldEntries = getRecordFieldEntries(value, new Set(CALL_STACK_KEYS));
        const fieldsContent = fieldEntries.length > 0 ? renderFieldList(fieldEntries) : <></>;
        if (!hasVisibleValue(callStackValue)) {
            return <EventDetailSingleColumn>{fieldsContent}</EventDetailSingleColumn>;
        }
        return <EventDetailLayout>
            <DetailColumn>{fieldsContent}</DetailColumn>
            <DetailColumn>{renderCallStackValue(callStackValue)}</DetailColumn>
        </EventDetailLayout>;
    };

    const getRelatedEventTabs = (data?: SnapshotDetailData): DetailEventTab[] => {
        return getRelatedEvents(data).map(item => ({
            key: item.key,
            label: t(item.key),
            value: item.value,
        }));
    };

    const renderDirectEventContent = (data?: SnapshotDetailData): JSX.Element => {
        const callStackValue = getCallStackValue(data);
        if (!hasVisibleValue(callStackValue)) {
            return renderNoFramesNotice();
        }
        return renderCallStackValue(callStackValue);
    };

    const renderSnapshotEventObject = (item: { key: string; value: any }): JSX.Element => {
        if (!isRecordValue(item.value)) {
            return renderObjectValue(item.key, item.value);
        }
        const callStackValue = getCallStackValue(item.value);
        const fieldEntries = getRecordFieldEntries(item.value, new Set(CALL_STACK_KEYS));
        if (!hasVisibleValue(callStackValue)) {
            return <EventDetailSingleColumn>{fieldEntries.length > 0 ? renderFieldList(fieldEntries) : <></>}</EventDetailSingleColumn>;
        }
        return <EventDetailLayout>
            <DetailColumn>{renderFieldList(fieldEntries)}</DetailColumn>
            <DetailColumn>{renderCallStackValue(callStackValue)}</DetailColumn>
        </EventDetailLayout>;
    };

    const renderExtraSnapshotData = (extraEntries: Array<{ key: string; value: any }>): JSX.Element => {
        if (extraEntries.length < 1) {
            return <></>;
        }
        return <>
            {extraEntries.map(item => renderContentSection(t(item.key), renderSnapshotEventObject(item)))}
        </>;
    };

    const getLocalBlockEntries = (block: Block): DetailFieldEntry[] => {
        return Object.entries(block)
            .filter(([key, value]) => !LEGACY_LOCAL_BLOCK_HIDDEN_KEYS.has(key) && hasVisibleValue(value))
            .map(([key, value]) => ({ key, value: formatDetailValue(value) }));
    };

    const getBasicInfoEntries = (tab: SliceDetailTab): DetailFieldEntry[] => {
        return getSnapshotOverviewEntries(tab.detailData);
    };

    const renderEventInfo = (tab: SliceDetailTab, type: 'block' | 'event', extraEntries: Array<{ key: string; value: any }>): JSX.Element => {
        if (type === 'block') {
            return <>
                {renderDirectEventContent(tab.detailData)}
                {renderExtraSnapshotData(extraEntries)}
            </>;
        }
        return <>
            {renderDirectEventContent(tab.detailData)}
            {renderExtraSnapshotData(extraEntries)}
        </>;
    };

    const renderSnapshotDetail = (tab: SliceDetailTab, type: 'block' | 'event'): JSX.Element => {
        if (tab.loading === true) {
            return <DetailPanel>
                <DetailCard>
                    <DetailLoading><Spin size="small" /></DetailLoading>
                </DetailCard>
            </DetailPanel>;
        }
        const basicEntries = getBasicInfoEntries(tab);
        const extraEntries = getSnapshotExtraEntries(tab.detailData);
        const hasCallStack = hasVisibleValue(getCallStackValue(tab.detailData));
        const relatedEventTabs = type === 'block' ? getRelatedEventTabs(tab.detailData) : [];
        const hasRelatedEvents = relatedEventTabs.length > 0;
        const hasEventInfo = hasRelatedEvents || hasCallStack || extraEntries.length > 0;
        const hasAnyDetail = basicEntries.length > 0 || hasEventInfo;
        if (!hasAnyDetail) {
            return <DetailPanel><DetailCard>{renderNoFramesNotice()}</DetailCard></DetailPanel>;
        }
        if (hasRelatedEvents) {
            return <DetailPanel>
                <DetailMainLayout>
                    <DetailCard>{renderFieldList(basicEntries)}</DetailCard>
                    <DetailColumn>
                        <EventDetailSwitcher
                            events={relatedEventTabs}
                            renderContent={(item): JSX.Element => <>
                                {renderEventDetailContent(item.value)}
                                {renderExtraSnapshotData(extraEntries)}
                            </>}
                        />
                    </DetailColumn>
                </DetailMainLayout>
            </DetailPanel>;
        }
        if (!hasEventInfo && basicEntries.length > 0) {
            return <DetailPanel>
                <DetailMainLayout>
                    <DetailCard>{renderFieldList(basicEntries)}</DetailCard>
                    <DetailCard>{renderNoFramesNotice()}</DetailCard>
                </DetailMainLayout>
            </DetailPanel>;
        }
        return <DetailPanel>
            {hasEventInfo
                ? <DetailMainLayout>
                    <DetailCard>{renderFieldList(basicEntries)}</DetailCard>
                    <DetailCard>{renderEventInfo(tab, type, extraEntries)}</DetailCard>
                </DetailMainLayout>
                : renderFieldList(basicEntries)}
        </DetailPanel>;
    };

    const renderDetailContent = (tab: SliceDetailTab): JSX.Element => {
        if (tab.kind === 'localBlock' && tab.block !== undefined) {
            return <DetailPanel><DetailCard>{renderFieldList(getLocalBlockEntries(tab.block))}</DetailCard></DetailPanel>;
        }
        if (tab.kind === 'snapshotBlock' || tab.kind === 'stateBlock') {
            return renderSnapshotDetail(tab, 'block');
        }
        return renderSnapshotDetail(tab, 'event');
    };

    useEffect(() => {
        setNoData(false);
        const block = cloneBlock(session.leaksWorkerInfo.clickItem);
        if (block === null) {
            return;
        }
        if (session.module === 'leaks') {
            upsertDetailTab({
                key: `block_${block.id}`,
                label: `${t('block')} #${block.id}`,
                titleId: block.id,
                kind: 'localBlock',
                block,
                selection: { type: 'block', block },
            }, { activate: true });
            return;
        }
        let cancelled = false;
        const requestDeviceId = session.deviceId;
        const requestDetailContextKey = detailContextKeyRef.current;
        const tabKey = `snapshot_block_${block.id}`;
        openPendingDetailTab({
            key: tabKey,
            label: `${t('block')} #${block.id}`,
            titleId: block.id,
            kind: 'snapshotBlock',
            loading: true,
            block,
            selection: { type: 'block', block },
        });
        getSnapshotDetailData('block', block.id, session.deviceId).then(result => {
            if (cancelled || session.deviceId !== requestDeviceId || detailContextKeyRef.current !== requestDetailContextKey) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('block')} #${block.id}`,
                titleId: block.id,
                kind: 'snapshotBlock',
                loading: false,
                detailData: result,
                block,
                selection: { type: 'block', block },
            });
        }).catch(() => {
            if (cancelled || session.deviceId !== requestDeviceId || detailContextKeyRef.current !== requestDetailContextKey) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('block')} #${block.id}`,
                titleId: block.id,
                kind: 'snapshotBlock',
                loading: false,
                detailData: {},
                block,
                selection: { type: 'block', block },
            });
        });
        return () => {
            cancelled = true;
        };
    }, [session.leaksWorkerInfo.clickItem]);

    useEffect(() => {
        setNoData(false);
        const stateSelection = cloneStateSelection(session.stateWorkerInfo.clickItem);
        if (stateSelection === null) {
            return;
        }
        const { type, data } = stateSelection;
        const id = type === 'segment' ? data.allocOrMapEventId : (data.blocks[0]?.id ?? -1);
        if (id < 0) {
            upsertNoFramesStateTab(stateSelection);
            return;
        }
        let cancelled = false;
        const requestDeviceId = session.deviceId;
        const requestDetailContextKey = detailContextKeyRef.current;
        const detailType = type === 'segment' ? 'event' : 'block';
        const tabKey = `snapshot_${detailType}_${id}`;
        openPendingDetailTab({
            key: tabKey,
            label: `${detailType === 'event' ? t('event') : t('block')} #${id}`,
            titleId: id,
            kind: type === 'segment' ? 'stateEvent' : 'stateBlock',
            loading: true,
            state: stateSelection,
            selection: { type: 'state', state: stateSelection },
        });
        getSnapshotDetailData(detailType, id, session.deviceId).then(result => {
            if (cancelled || session.deviceId !== requestDeviceId || detailContextKeyRef.current !== requestDetailContextKey) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${detailType === 'event' ? t('event') : t('block')} #${id}`,
                titleId: id,
                kind: type === 'segment' ? 'stateEvent' : 'stateBlock',
                loading: false,
                detailData: normalizeStateBlockDetailData(result, stateSelection),
                state: stateSelection,
                selection: { type: 'state', state: stateSelection },
            });
        }).catch(() => {
            if (cancelled || session.deviceId !== requestDeviceId || detailContextKeyRef.current !== requestDetailContextKey) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${detailType === 'event' ? t('event') : t('block')} #${id}`,
                titleId: id,
                kind: type === 'segment' ? 'stateEvent' : 'stateBlock',
                loading: false,
                detailData: normalizeStateBlockDetailData({}, stateSelection),
                state: stateSelection,
                selection: { type: 'state', state: stateSelection },
            });
        });
        return () => {
            cancelled = true;
        };
    }, [session.stateWorkerInfo.clickItem]);

    useEffect(() => {
        setNoData(false);
        if (session.clickEventItem === null) {
            return;
        }
        const event = { ...session.clickEventItem };
        let cancelled = false;
        const requestDeviceId = session.deviceId;
        const requestDetailContextKey = detailContextKeyRef.current;
        const tabKey = `snapshot_event_${event.id}`;
        openPendingDetailTab({
            key: tabKey,
            label: `${t('event')} #${event.id}`,
            titleId: event.id,
            kind: 'snapshotEvent',
            loading: true,
            event,
            selection: { type: 'event', event },
        });
        getSnapshotDetailData('event', event.id, session.deviceId).then(result => {
            if (cancelled || session.deviceId !== requestDeviceId || detailContextKeyRef.current !== requestDetailContextKey) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('event')} #${event.id}`,
                titleId: event.id,
                kind: 'snapshotEvent',
                loading: false,
                detailData: result,
                event,
                selection: { type: 'event', event },
            });
        }).catch(() => {
            if (cancelled || session.deviceId !== requestDeviceId || detailContextKeyRef.current !== requestDetailContextKey) {
                return;
            }
            upsertDetailTab({
                key: tabKey,
                label: `${t('event')} #${event.id}`,
                titleId: event.id,
                kind: 'snapshotEvent',
                loading: false,
                detailData: {},
                event,
                selection: { type: 'event', event },
            });
        });
        return () => {
            cancelled = true;
        };
    }, [session.clickEventItem]);

    useEffect(() => {
        if (deviceIdRef.current !== session.deviceId) {
            detailStateCacheRef.current.clear();
            deviceIdRef.current = session.deviceId;
        }
        clearDetailState();
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
            title={getDetailTabTitle(tab)}
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
            }}>{getDetailTabLabel(tab)}</DetailTabLabel>,
        children: tab.key === activeDetailTabKey ? renderDetailContent(tab) : null,
    }));

    if (detailTabs.length > 0) {
        return <DetailTabsWrapper onClick={(): void => setContextMenu(menu => ({ ...menu, visible: false }))}>
            <Tabs
                type="editable-card"
                hideAdd
                size="small"
                activeKey={activeDetailTabKey}
                items={tabItems}
                destroyInactiveTabPane
                onChange={selectDetailTab}
                onEdit={(targetKey, action): void => {
                    if (action === 'remove') {
                        removeDetailTab(String(targetKey));
                    }
                }}
            />
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
        {
            noData
                ? <NoData>{(t('noData', { returnObjects: true }) as string[]).map((item, index) => <div key={index}>{item}</div>)}</NoData>
                : <NoData>{t('selectDetailTip')}</NoData>
        }
    </>;
});
