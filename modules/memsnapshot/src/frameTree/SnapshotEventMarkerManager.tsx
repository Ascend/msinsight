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
import React, { useEffect, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { useTheme } from '@emotion/react';
import { CloseOutlined, DeleteOutlined, EyeInvisibleOutlined, EyeOutlined } from '@ant-design/icons';
import { Modal } from 'antd';
import { useTranslation } from 'react-i18next';
import {
    MarkerManagerPanel, MarkerManagerHeader, MarkerManagerBody, MarkerManagerFooter,
    MarkerManagerButton, MarkerManagerClearButton, MarkerManagerColorInput,
} from '../common/components/leaks/LifecycleMemoryMarkerManager';
import { TimelineFlagIcon } from '../common/components/leaks/TimelineFlagIcon';
import type { SnapshotEventMarker } from './timeline';

const Panel = styled(MarkerManagerPanel)`
    @media (max-width: 600px) { right: 0; }
`;
const Row = styled.div`
    display: grid; grid-template-columns: 24px minmax(0, 1fr) 24px 26px 26px; gap: 8px;
    align-items: center; margin-bottom: 8px; padding: 10px;
    border: 1px solid ${(p): string => p.theme.borderColorLight}; border-radius: 5px;
    color: ${(p): string => p.theme.primaryColor};
    &[data-hidden="true"] { opacity: .62; }
    > svg { width: 20px; }
    input:not([type="color"]) { width: 100%; min-width: 0; padding: 2px 0; background: transparent; border: 0;
        border-bottom: 1px solid transparent; color: inherit; font: inherit; font-weight: 600; }
    input:not([type="color"]):focus { outline: none; border-bottom-color: ${(p): string => p.theme.primaryColor}; }
    p { margin: 4px 0 0; font-size: 11px; color: ${(p): string => p.theme.textColorSecondary};
        overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
`;

const MarkerName = ({ name, onCommit }: { name: string; onCommit: (name: string) => void }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [draft, setDraft] = useState<string | null>(null);
    return <input value={draft ?? name} maxLength={80} aria-label={t('frameTree.renameMarker', { name })}
        title={t('frameTree.renameMarker', { name })} placeholder={t('frameTree.renameHint')}
        onChange={(event): void => setDraft(event.target.value)}
        onBlur={(): void => {
            if (draft?.trim()) onCommit(draft.trim());
            setDraft(null);
        }}
        onKeyDown={(event): void => {
            if (event.key === 'Escape') {
                event.stopPropagation();
                setDraft(null);
            } else if (event.key === 'Enter' && !event.nativeEvent.isComposing) {
                event.currentTarget.blur();
            }
        }} />;
};

export function SnapshotEventMarkerManager({ markers, onClose, onUpdate, onRemove, onClear }: {
    markers: readonly SnapshotEventMarker[];
    onClose: () => void;
    onUpdate: (id: number, updates: Partial<Pick<SnapshotEventMarker, 'label' | 'color' | 'hidden'>>) => void;
    onRemove: (id: number) => void;
    onClear: () => void;
}): JSX.Element {
    const { t } = useTranslation('leaks');
    const theme = useTheme();
    const panel = useRef<HTMLDivElement>(null);
    const confirmation = useRef<ReturnType<typeof Modal.confirm>>();
    useEffect(() => {
        confirmation.current?.update({
            title: t('frameTree.clearMarkers'),
            content: t('frameTree.clearMarkersConfirm'),
            okText: t('clearAllMemoryMarkersConfirmButton'),
            cancelText: t('clearAllMemoryMarkersCancelButton'),
        });
    }, [t]);
    useEffect(() => {
        const outside = (event: MouseEvent): void => {
            if (!panel.current?.contains(event.target as Node) &&
                !(event.target instanceof Element && event.target.closest('.ant-modal-root'))) {
                // Commit the focused draft before mousedown unmounts the manager.
                const focused = document.activeElement;
                if (focused instanceof HTMLElement && panel.current?.contains(focused)) focused.blur();
                onClose();
            }
        };
        const escape = (event: KeyboardEvent): void => { if (event.key === 'Escape') onClose(); };
        document.addEventListener('mousedown', outside);
        document.addEventListener('keydown', escape);
        return () => {
            document.removeEventListener('mousedown', outside);
            document.removeEventListener('keydown', escape);
            confirmation.current?.destroy();
        };
    }, [onClose]);
    return <Panel ref={panel} role="dialog" aria-label={t('frameTree.manageMarkers')}
        data-testid="snapshot-event-marker-manager" data-lifecycle-floating-panel
        onMouseDown={(event): void => event.stopPropagation()} onClick={(event): void => event.stopPropagation()}>
        <MarkerManagerHeader>
            <span>{t('frameTree.manageMarkers')}</span>
            <MarkerManagerButton type="button" aria-label={t('frameTree.closeMarkerManager')} onClick={onClose}><CloseOutlined /></MarkerManagerButton>
        </MarkerManagerHeader>
        <MarkerManagerBody>
            {markers.length === 0
                ? <p>{t('frameTree.noMarkers')}</p>
                : markers.map(marker =>
                    <Row key={marker.id} data-testid="snapshot-event-marker-row" data-hidden={marker.hidden === true}
                        style={{ color: marker.color }}>
                        <TimelineFlagIcon />
                        <div style={{ minWidth: 0 }}>
                            <MarkerName name={marker.label} onCommit={(label): void => onUpdate(marker.id, { label })} />
                            <p title={marker.error}>{marker.resolving
                                ? t('frameTree.resolving')
                                : marker.error ?? `${t('frameTree.event')} ${marker.eventId}`}</p>
                        </div>
                        <MarkerManagerColorInput type="color" value={marker.color ?? theme.primaryColor}
                            aria-label={`${t('markerColor')}: ${marker.label}`} title={`${t('markerColor')}: ${marker.label}`}
                            onChange={(event): void => onUpdate(marker.id, { color: event.target.value })} />
                        <MarkerManagerButton type="button" aria-pressed={marker.hidden === true}
                            aria-label={t(marker.hidden ? 'showMemoryMarker' : 'hideMemoryMarker', { marker: marker.label })}
                            title={t(marker.hidden ? 'showMemoryMarker' : 'hideMemoryMarker', { marker: marker.label })}
                            onClick={(): void => onUpdate(marker.id, { hidden: marker.hidden !== true })}>
                            {marker.hidden ? <EyeInvisibleOutlined /> : <EyeOutlined />}
                        </MarkerManagerButton>
                        <MarkerManagerButton type="button" aria-label={t('frameTree.removeMarker', { name: marker.label })}
                            title={t('frameTree.removeMarker', { name: marker.label })} onClick={(): void => onRemove(marker.id)}><DeleteOutlined /></MarkerManagerButton>
                    </Row>)}
        </MarkerManagerBody>
        <MarkerManagerFooter>
            {markers.length > 0
                ? <MarkerManagerClearButton type="button" aria-label={t('frameTree.clearMarkers')} onClick={(): void => {
                    if (confirmation.current) return;
                    confirmation.current = Modal.confirm({
                        title: t('frameTree.clearMarkers'),
                        content: t('frameTree.clearMarkersConfirm'),
                        okText: t('clearAllMemoryMarkersConfirmButton'),
                        cancelText: t('clearAllMemoryMarkersCancelButton'),
                        okButtonProps: { danger: true },
                        onOk: onClear,
                        afterClose: (): void => { confirmation.current = undefined; },
                    });
                }}><DeleteOutlined />{t('frameTree.clearMarkers')}</MarkerManagerClearButton>
                : null}
            <span style={{ marginLeft: 'auto' }}>{t('frameTree.markerCount', { count: markers.length })}</span>
        </MarkerManagerFooter>
    </Panel>;
}
