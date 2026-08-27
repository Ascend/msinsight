/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useEffect, useRef } from 'react';
import styled from '@emotion/styled';
import { CloseOutlined, DeleteOutlined, EyeInvisibleOutlined, EyeOutlined, LinkOutlined } from '@ant-design/icons';
import { Modal } from 'antd';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import { type Session } from '../../entity/session';
import {
    getLifecycleMemoryMarkerColor,
    getLifecycleMemoryMarkerOrdinal,
    getLifecycleMemoryMarkerSource,
    sortLifecycleMemoryMarkers,
} from '../../entity/lifecycleMemoryMarkers';
import { formatBytes } from '../../utils/utils';
import { TimelineFlagIcon } from './TimelineFlagIcon';
const MARKER_LAYOUT = { panelWidth: 480, relationshipWidth: 130, nodeWidth: 28, rowHeight: 76, stride: 108 };

const Panel = styled.div`
    position: absolute;
    top: 8px;
    right: -56px;
    z-index: 12;
    display: flex;
    flex-direction: column;
    width: min(${MARKER_LAYOUT.panelWidth}px, calc(100% - 24px));
    max-height: calc(100% - 16px);
    box-sizing: border-box;
    color: ${(props): string => props.theme.textColorPrimary};
    background: ${(props): string => props.theme.contentBackgroundColor};
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 6px;
    box-shadow: ${(props): string => props.theme.boxShadow};
    pointer-events: auto;
`;
const Header = styled.div`
    display: flex;
    align-items: center;
    justify-content: space-between;
    min-height: 44px;
    padding: 0 10px 0 14px;
    background: ${(props): string => props.theme.contentBackgroundColor};
    border-bottom: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 6px 6px 0 0;
    font-size: 14px;
    font-weight: 600;
`;
const IconButton = styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 26px;
    height: 26px;
    padding: 0;
    color: ${(props): string => props.theme.iconColor};
    background: transparent;
    border: none;
    border-radius: 4px;
    cursor: pointer;

    &:hover {
        color: ${(props): string => props.theme.primaryColor};
        background: ${(props): string => props.theme.bgColorLight};
    }

    &:focus {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }
`;
const Body = styled.div`
    flex: 1 1 auto; min-height: 0;
    padding: 12px 14px 14px;
    overflow-y: auto;
`;
const Footer = styled.div`
    flex: 0 0 auto; display: flex; align-items: center; justify-content: space-between;
    gap: 12px; min-height: 42px; padding: 7px 14px; box-sizing: border-box;
    color: ${(props): string => props.theme.textColorSecondary}; font-size: 12px;
    background: ${(props): string => props.theme.contentBackgroundColor};
    border-top: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 0 0 6px 6px;
`;
const ClearButton = styled.button`
    display: inline-flex;
    align-items: center;
    gap: 4px;
    padding: 3px 8px;
    color: #d4380d;
    font-size: 12px;
    background: transparent;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    cursor: pointer;

    &:hover {
        background: ${(props): string => props.theme.bgColorLight};
        border-color: #d4380d;
    }
`;
const Timeline = styled.div`
    position: relative;
`;
const Rail = styled.div`
    position: absolute;
    top: ${MARKER_LAYOUT.rowHeight / 2}px;
    bottom: ${MARKER_LAYOUT.rowHeight / 2}px;
    left: ${MARKER_LAYOUT.relationshipWidth + MARKER_LAYOUT.nodeWidth / 2}px;
    width: 2px;
    background: ${(props): string => props.theme.textColorSecondary}; opacity: 0.82;
    transform: translateX(-50%);
`;
const Row = styled.div`
    position: relative;
    display: grid;
    grid-template-columns: ${MARKER_LAYOUT.relationshipWidth}px ${MARKER_LAYOUT.nodeWidth}px minmax(0, 1fr);
    align-items: center;
    height: ${MARKER_LAYOUT.rowHeight}px;
`;
const Node = styled.span<{ markerColor: string; $hidden: boolean }>`
    z-index: 1;
    position: relative; display: inline-flex;
    width: 20px;
    height: 20px;
    align-items: center;
    justify-content: center;
    justify-self: center;
    color: ${(props): string => props.markerColor};
    background: ${(props): string => props.theme.contentBackgroundColor};
    visibility: ${(props): string => props.$hidden ? 'hidden' : 'visible'};

    &::before { content: ''; position: absolute; top: 50%; right: calc(50% + 12px); width: ${MARKER_LAYOUT.relationshipWidth + MARKER_LAYOUT.nodeWidth / 2 - 36}px;
        border-top: 1px dashed ${(props): string => props.theme.textColorSecondary}; opacity: 0.82; transform: translateY(-50%); }

    svg {
        width: 18px;
        height: auto;
    }
`;
const Card = styled.div`
    display: grid;
    grid-template-columns: minmax(0, 1fr) repeat(3, 26px);
    gap: 4px;
    align-items: center;
    height: ${MARKER_LAYOUT.rowHeight}px;
    padding: 7px 8px 7px 10px;
    box-sizing: border-box;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;
    font-variant-numeric: tabular-nums;
    background: ${(props): string => props.theme.contentBackgroundColor};
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 5px;

    &[data-hidden='true'] {
        opacity: 0.62;
    }
`;
const CardMain = styled.div`
    display: flex; flex-direction: column;
    align-items: stretch; gap: 2px;
    min-width: 0;
`;
const EditableMarkerLabel = styled.input<{ markerColor: string }>`
    width: 100%;
    min-width: 0;
    height: 22px;
    padding: 0;
    color: ${(props): string => props.markerColor};
    font-weight: 600;
    background: transparent;
    border: 0;
    border-bottom: 1px solid transparent;
    cursor: text;

    &:hover {
        border-bottom-color: ${(props): string => props.theme.borderColor};
    }

    &:focus-visible {
        border-color: ${(props): string => props.theme.primaryColor};
        outline: none;
    }
`;
const SourceBadge = styled.span`
    display: inline-flex; align-items: center; align-self: flex-start;
    gap: 3px; max-width: 100%; box-sizing: border-box;
    padding: 1px 5px;
    color: ${(props): string => props.theme.textColorSecondary};
    background: ${(props): string => props.theme.bgColorLight};
    border-radius: 3px;
    span { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
`;
const Baseline = styled.span`
    overflow: hidden;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 11px;
    opacity: 0.72; text-overflow: ellipsis; white-space: nowrap;
`;
const ColorInput = styled.input`
    width: 24px;
    height: 22px;
    padding: 1px;
    background: transparent;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 3px;
    cursor: pointer;
`;
const Spacer = styled.div`
    height: ${MARKER_LAYOUT.stride - MARKER_LAYOUT.rowHeight}px;
`;
const Connection = styled.div`
    position: absolute;
    left: 0;
    width: ${MARKER_LAYOUT.relationshipWidth + MARKER_LAYOUT.nodeWidth / 2}px;
    z-index: 1;
    pointer-events: none;
`;
const GapLabel = styled.span`
    position: absolute; inset: 0 18px 0 24px;
    display: flex; align-items: center; justify-content: center;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px; font-variant-numeric: tabular-nums; font-weight: 500;

    > span:last-child {
        z-index: 1; padding: 1px 7px; pointer-events: auto; user-select: text; cursor: text;
        background: ${(props): string => props.theme.contentBackgroundColor};
        white-space: nowrap;
    }
`;
const GapDirection = styled.span`
    position: absolute; top: 7px; bottom: 7px; left: 50%;
    width: 1px;
    color: ${(props): string => props.theme.textColorSecondary}; opacity: 0.82;
    background: linear-gradient(currentColor, currentColor) center top / 1px calc(50% - 14px) no-repeat, linear-gradient(currentColor, currentColor) center bottom / 1px calc(50% - 14px) no-repeat;
    transform: translateX(-50%);

    &::before, &::after { content: ''; position: absolute; left: -3px; width: 6px; height: 6px; }
    &::before { top: 0; border-top: 1px solid; border-left: 1px solid; transform: rotate(45deg); }
    &::after { bottom: 0; border-right: 1px solid; border-bottom: 1px solid; transform: rotate(45deg); }
`;
const Empty = styled.div`
    padding: 28px 12px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    text-align: center;
`;
export const LifecycleMemoryMarkerManager = observer(({
    session,
    onClose,
}: {
    session: Session;
    onClose: () => void;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    const panelRef = useRef<HTMLDivElement>(null);
    const clearAllModalRef = useRef<ReturnType<typeof Modal.confirm> | null>(null);
    const markers = sortLifecycleMemoryMarkers(session.getLifecycleMemoryMarkers());
    const displayedMarkers = [...markers].reverse();
    const visibleMarkers = displayedMarkers.map((marker, index) => ({ marker, index }))
        .filter(item => item.marker.hidden !== true);
    const clearAllModalText = {
        title: t('clearAllMemoryMarkersConfirm'),
        content: t('clearAllMemoryMarkersDescription'),
        okText: t('clearAllMemoryMarkersConfirmButton'),
        cancelText: t('clearAllMemoryMarkersCancelButton'),
    };

    useEffect(() => {
        const closeOutside = (event: MouseEvent): void => {
            if (!panelRef.current?.contains(event.target as Node)) onClose();
        };
        const closeOnEscape = (event: KeyboardEvent): void => {
            if (event.key === 'Escape') onClose();
        };
        document.addEventListener('mousedown', closeOutside);
        document.addEventListener('keydown', closeOnEscape);
        return () => {
            document.removeEventListener('mousedown', closeOutside);
            document.removeEventListener('keydown', closeOnEscape);
        };
    }, [onClose]);
    useEffect(() => {
        clearAllModalRef.current?.update(clearAllModalText);
    }, [session.language, clearAllModalText.title, clearAllModalText.content, clearAllModalText.okText, clearAllModalText.cancelText]);
    const clearAll = (): void => {
        if (clearAllModalRef.current) return;
        const clearAllModal = Modal.confirm({
            ...clearAllModalText,
            okButtonProps: { danger: true },
            onOk: (): void => {
                session.clearCurrentLifecycleMemoryMarkers();
                onClose();
            },
            afterClose: (): void => {
                if (clearAllModalRef.current === clearAllModal) clearAllModalRef.current = null;
            },
        });
        clearAllModalRef.current = clearAllModal;
    };
    return <Panel
        ref={panelRef}
        role="dialog"
        aria-label={t('manageMemoryMarkers')}
        data-testid="memoryMarkerManagerFloatingPanel"
        onMouseDown={(event): void => event.stopPropagation()}
        onClick={(event): void => event.stopPropagation()}
    >
        <Header>
            <span>{t('manageMemoryMarkers')}</span>
            <IconButton type="button" aria-label={t('closeMemoryMarkerManagement')} onClick={onClose}>
                <CloseOutlined />
            </IconButton>
        </Header>
        <Body>
            {markers.length > 0
                ? <Timeline data-testid="memoryMarkerManagerTimeline">
                    <Rail />
                    {visibleMarkers.slice(0, -1).map((item, index) => {
                        const lower = visibleMarkers[index + 1];
                        return <Connection
                            key={`${item.marker.id}-${lower.marker.id}`}
                            data-testid="memoryMarkerManagerGap"
                            style={{
                                top: MARKER_LAYOUT.rowHeight / 2 + item.index * MARKER_LAYOUT.stride,
                                height: (lower.index - item.index) * MARKER_LAYOUT.stride,
                            }}
                        >
                            <GapLabel><GapDirection aria-hidden="true" data-testid="memoryMarkerManagerGapDirection" />
                                <span>{formatBytes(item.marker.memoryBytes - lower.marker.memoryBytes)}</span>
                            </GapLabel>
                        </Connection>;
                    })}
                    {displayedMarkers.map((marker, index) => {
                        const sourceIndex = markers.findIndex(item => item.id === marker.id);
                        const ordinal = getLifecycleMemoryMarkerOrdinal(marker, sourceIndex);
                        const color = getLifecycleMemoryMarkerColor(marker, sourceIndex);
                        const defaultLabel = `Flag ${ordinal}`;
                        return <React.Fragment key={marker.id}>
                            <Row data-testid="memoryMarkerManagerRow">
                                <span />
                                <Node markerColor={color} $hidden={marker.hidden === true} data-hidden={String(marker.hidden === true)} data-testid="memoryMarkerManagerNode">
                                    <TimelineFlagIcon aria-hidden="true" />
                                </Node>
                                <Card data-hidden={String(marker.hidden === true)}>
                                    <CardMain>
                                        <EditableMarkerLabel
                                            markerColor={color}
                                            defaultValue={marker.name ?? defaultLabel}
                                            maxLength={40}
                                            aria-label={t('renameMemoryMarker', { marker: defaultLabel })}
                                            onBlur={(event): void => {
                                                const name = event.target.value.trim();
                                                event.currentTarget.value = name || defaultLabel;
                                                session.updateLifecycleMemoryMarkerPresentation(marker.id, {
                                                    name: name === defaultLabel ? '' : name,
                                                });
                                            }}
                                            onKeyDown={(event): void => {
                                                event.stopPropagation();
                                                if (event.key === 'Enter') event.currentTarget.blur();
                                                if (event.key === 'Escape') {
                                                    event.currentTarget.value = marker.name ?? defaultLabel;
                                                    event.currentTarget.blur();
                                                }
                                            }}
                                        />
                                        {getLifecycleMemoryMarkerSource(marker) === 'block'
                                            ? <SourceBadge><LinkOutlined />
                                                <span>{t('linkedBlockId', { id: marker.blockId ?? '--' })}</span>
                                            </SourceBadge>
                                            : <></>}
                                        <Baseline>{`${t('memoryMarkerBaseline')}: ${formatBytes(marker.memoryBytes)}`}</Baseline>
                                    </CardMain>
                                    <ColorInput
                                        type="color"
                                        value={color}
                                        aria-label={`${t('markerColor')}: Flag ${ordinal}`}
                                        onChange={(event): void => {
                                            session.updateLifecycleMemoryMarkerColor(marker.id, event.target.value);
                                        }}
                                    />
                                    <IconButton
                                        type="button"
                                        aria-pressed={marker.hidden === true}
                                        aria-label={t(marker.hidden === true ? 'showMemoryMarker' : 'hideMemoryMarker', {
                                            marker: `Flag ${ordinal}`,
                                        })}
                                        title={t(marker.hidden === true ? 'showMemoryMarker' : 'hideMemoryMarker', {
                                            marker: `Flag ${ordinal}`,
                                        })}
                                        onClick={(): void => {
                                            session.updateLifecycleMemoryMarkerPresentation(marker.id, {
                                                hidden: marker.hidden !== true,
                                            });
                                        }}
                                    >{marker.hidden === true ? <EyeInvisibleOutlined /> : <EyeOutlined />}</IconButton>
                                    <IconButton
                                        type="button"
                                        aria-label={`${t('deleteMemoryMarker')}: Flag ${ordinal}`}
                                        onClick={(): void => session.deleteLifecycleMemoryMarker(marker.id)}
                                    ><DeleteOutlined /></IconButton>
                                </Card>
                            </Row>
                            {index < displayedMarkers.length - 1
                                ? <Spacer data-testid="memoryMarkerManagerSpacing" />
                                : <></>}
                        </React.Fragment>;
                    })}
                </Timeline>
                : <Empty>{t('noMemoryMarkers')}</Empty>}
        </Body>
        <Footer data-testid="memoryMarkerManagerFooter">
            {markers.length > 0
                ? <ClearButton type="button" aria-label={t('clearAllMemoryMarkers')} onClick={clearAll}>
                    <DeleteOutlined /><span>{t('clearAllMemoryMarkers')}</span>
                </ClearButton>
                : <></>}
            <span style={{ marginLeft: 'auto' }}>{t('memoryMarkerCount', { count: markers.length })}</span>
        </Footer>
    </Panel>;
});
