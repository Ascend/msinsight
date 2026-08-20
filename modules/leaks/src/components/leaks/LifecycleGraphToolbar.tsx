/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useEffect, useRef, useState } from 'react';
import styled from '@emotion/styled';
import {
    ColumnWidthOutlined,
    ExpandAltOutlined,
    FlagOutlined,
    GroupOutlined,
    OneToOneOutlined,
    QuestionCircleOutlined,
} from '@ant-design/icons';
import { Tooltip } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import {
    graphToolbarTooltipClassName,
    GraphKeycap,
    GraphShortcutActions,
    GraphShortcutRow,
    GraphShortcutTip,
    GraphShortcutTitle,
    GraphToolbarTooltipStyle,
    GraphWheelCombo,
    GraphWheelIcon,
} from './tools';
import {
    LifecycleGraphLayerPanel,
    type LifecycleGraphLayer,
    type LifecycleGraphLayerVisibility,
} from './LifecycleGraphLayerPanel';
import { LifecycleGraphInteractionGuide } from './LifecycleGraphInteractionGuide';

export type LifecycleZoomMode = 'proportional' | 'horizontal';

export const LifecycleZoomModeIcon = ({ zoomMode }: { zoomMode: LifecycleZoomMode }): JSX.Element =>
    zoomMode === 'proportional'
        ? <ExpandAltOutlined data-testid="proportionalZoomIcon" />
        : <ColumnWidthOutlined data-testid="horizontalZoomIcon" />;

export const LifecycleZoomModeTooltip = ({ zoomMode }: { zoomMode: LifecycleZoomMode }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const switchLabel = zoomMode === 'proportional'
        ? t('switchToHorizontalZoom')
        : t('switchToProportionalZoom');
    const currentModeLabel = zoomMode === 'proportional' ? t('equalZoomHelp') : t('xZoomWheelHelp');

    return <GraphShortcutTip>
        <GraphShortcutTitle>
            {switchLabel}
            <GraphShortcutActions><GraphKeycap>H</GraphKeycap></GraphShortcutActions>
        </GraphShortcutTitle>
        <GraphShortcutRow>
            <span>{t('currentZoomMode')}</span>
            <GraphWheelCombo><GraphWheelIcon /><span>{currentModeLabel}</span></GraphWheelCombo>
        </GraphShortcutRow>
        <GraphShortcutRow>
            <span>{t('xZoomWheelHelp')}</span>
            <GraphShortcutActions><GraphKeycap>W</GraphKeycap><span>/</span><GraphKeycap>S</GraphKeycap></GraphShortcutActions>
        </GraphShortcutRow>
        <GraphShortcutRow>
            <span>{t('equalZoomHelp')}</span>
            <GraphShortcutActions>
                <GraphKeycap>Ctrl</GraphKeycap><span>+</span><GraphKeycap>W</GraphKeycap><span>/</span><GraphKeycap>S</GraphKeycap>
            </GraphShortcutActions>
        </GraphShortcutRow>
    </GraphShortcutTip>;
};

interface LifecycleGraphToolbarProps {
    zoomMode: LifecycleZoomMode;
    layerVisibility?: LifecycleGraphLayerVisibility;
    onZoomModeChange: (mode: LifecycleZoomMode) => void;
    onReset: () => void;
    onLayerVisibilityChange?: (layer: LifecycleGraphLayer) => void;
    markerManagerOpen?: boolean;
    onMarkerManagementOpen?: () => void;
    onMarkerManagementClose?: () => void;
}

const Container = styled.div`
    position: absolute;
    top: 8px;
    right: -98px;
    z-index: 12;
    display: flex;
    align-items: flex-start;
`;

const Panel = styled.div`
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    width: 32px;
    padding: 4px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 6px;
    background: ${(props): string => props.theme.contentBackgroundColor};
    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.08), 0 4px 10px rgba(0, 0, 0, 0.06);
`;

const ToolButton = styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 24px;
    height: 24px;
    padding: 0;
    color: ${(props): string => props.theme.iconColor};
    background: transparent;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    transition: color 160ms ease-out, background-color 160ms ease-out;

    &:hover {
        color: ${(props): string => props.theme.primaryColor};
        background: ${(props): string => props.theme.bgColorLight};
    }

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }

    &:disabled {
        color: ${(props): string => props.theme.textColorSecondary};
        opacity: 0.45;
        cursor: default;
    }

    svg {
        width: 16px;
        height: 16px;
    }

`;

const Divider = styled.div`
    width: 20px;
    border-top: 1px solid ${(props): string => props.theme.borderColorLighter};
`;

export const LifecycleGraphToolbar = ({
    zoomMode,
    layerVisibility = { blocks: true, overview: true, markers: true },
    onZoomModeChange,
    onReset,
    onLayerVisibilityChange = (): void => undefined,
    markerManagerOpen = false,
    onMarkerManagementOpen = (): void => undefined,
    onMarkerManagementClose = (): void => undefined,
}: LifecycleGraphToolbarProps): JSX.Element => {
    const { t } = useTranslation('leaks');
    const containerRef = useRef<HTMLDivElement>(null);
    const [activePanel, setActivePanel] = useState<'layers' | 'guide' | null>(null);
    const nextZoomMode: LifecycleZoomMode = zoomMode === 'proportional' ? 'horizontal' : 'proportional';
    const zoomToggleLabel = zoomMode === 'proportional'
        ? t('switchToHorizontalZoom')
        : t('switchToProportionalZoom');

    useEffect(() => {
        if (activePanel === null) return undefined;
        const closeOutside = (event: MouseEvent): void => {
            const target = event.target;
            const insideFloatingPanel = target instanceof Element &&
                target.closest('[data-lifecycle-floating-panel]') !== null;
            if (!containerRef.current?.contains(target as Node) && !insideFloatingPanel) {
                setActivePanel(null);
            }
        };
        const closeOnEscape = (event: KeyboardEvent): void => {
            if (event.key === 'Escape') setActivePanel(null);
        };
        document.addEventListener('mousedown', closeOutside);
        document.addEventListener('keydown', closeOnEscape);
        return () => {
            document.removeEventListener('mousedown', closeOutside);
            document.removeEventListener('keydown', closeOnEscape);
        };
    }, [activePanel]);

    return <Container
        ref={containerRef}
        data-testid="lifecycleGraphToolbar"
        onMouseDown={(event): void => event.stopPropagation()}
        onClick={(event): void => event.stopPropagation()}
    >
        <GraphToolbarTooltipStyle />
        {activePanel === 'layers'
            ? <LifecycleGraphLayerPanel
                visibility={layerVisibility}
                onChange={onLayerVisibilityChange}
                onClose={(): void => setActivePanel(null)}
            />
            : <></>}
        {activePanel === 'guide'
            ? <LifecycleGraphInteractionGuide
                anchorElement={containerRef.current}
                onClose={(): void => setActivePanel(null)}
            />
            : <></>}
        <Panel>
            <Tooltip
                title={<LifecycleZoomModeTooltip zoomMode={zoomMode} />}
                placement="left"
                overlayClassName={graphToolbarTooltipClassName}
                mouseEnterDelay={0}
                mouseLeaveDelay={0}
            >
                <ToolButton
                    type="button"
                    aria-label={zoomToggleLabel}
                    data-current-zoom-mode={zoomMode}
                    onClick={(): void => onZoomModeChange(nextZoomMode)}
                ><LifecycleZoomModeIcon zoomMode={zoomMode} /></ToolButton>
            </Tooltip>
            <Tooltip title={t('resetView')} placement="left">
                <ToolButton type="button" aria-label={t('resetView')} onClick={onReset}>
                    <OneToOneOutlined />
                </ToolButton>
            </Tooltip>
            <Divider />
            <Tooltip title={t('graphLayers')} placement="left">
                <ToolButton
                    type="button"
                    aria-label={t('graphLayers')}
                    aria-expanded={activePanel === 'layers'}
                    onClick={(): void => {
                        setActivePanel(activePanel === 'layers' ? null : 'layers');
                        onMarkerManagementClose();
                    }}
                ><GroupOutlined /></ToolButton>
            </Tooltip>
            <Tooltip title={t('manageMemoryMarkers')} placement="left">
                <ToolButton
                    type="button"
                    aria-label={t('manageMemoryMarkers')}
                    aria-expanded={markerManagerOpen}
                    onClick={(): void => {
                        setActivePanel(null);
                        if (markerManagerOpen) onMarkerManagementClose();
                        else onMarkerManagementOpen();
                    }}
                ><FlagOutlined /></ToolButton>
            </Tooltip>
            <Divider />
            <Tooltip title={t('lifecycleGraphGuideTitle')} placement="left">
                <ToolButton
                    type="button"
                    aria-label={t('lifecycleGraphGuideTitle')}
                    aria-expanded={activePanel === 'guide'}
                    onClick={(): void => {
                        setActivePanel(activePanel === 'guide' ? null : 'guide');
                        onMarkerManagementClose();
                    }}
                ><QuestionCircleOutlined /></ToolButton>
            </Tooltip>
        </Panel>
    </Container>;
};
