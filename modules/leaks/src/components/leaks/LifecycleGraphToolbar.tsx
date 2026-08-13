/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React from 'react';
import styled from '@emotion/styled';
import { ColumnWidthOutlined, ExpandAltOutlined, OneToOneOutlined } from '@ant-design/icons';
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
    onZoomModeChange: (mode: LifecycleZoomMode) => void;
    onReset: () => void;
}

const Container = styled.div`
    position: absolute;
    top: 8px;
    right: -98px;
    z-index: 5;
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

    svg {
        width: 16px;
        height: 16px;
    }
`;

export const LifecycleGraphToolbar = ({
    zoomMode,
    onZoomModeChange,
    onReset,
}: LifecycleGraphToolbarProps): JSX.Element => {
    const { t } = useTranslation('leaks');
    const nextZoomMode: LifecycleZoomMode = zoomMode === 'proportional' ? 'horizontal' : 'proportional';
    const zoomToggleLabel = zoomMode === 'proportional'
        ? t('switchToHorizontalZoom')
        : t('switchToProportionalZoom');

    return <Container
        data-testid="lifecycleGraphToolbar"
        onMouseDown={(event): void => event.stopPropagation()}
        onClick={(event): void => event.stopPropagation()}
    >
        <GraphToolbarTooltipStyle />
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
        </Panel>
    </Container>;
};
