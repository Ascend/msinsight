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
import {
    BorderOutlined,
    CloseOutlined,
    EyeInvisibleOutlined,
    EyeOutlined,
    FlagOutlined,
    LineChartOutlined,
} from '@ant-design/icons';
import { useTranslation } from 'react-i18next';

export interface LifecycleGraphLayerVisibility {
    blocks: boolean;
    overview: boolean;
    markers: boolean;
}

export type LifecycleGraphLayer = keyof LifecycleGraphLayerVisibility;

const Panel = styled.div`
    position: absolute;
    top: 68px;
    right: 42px;
    width: 224px;
    padding: 8px;
    color: ${(props): string => props.theme.textColorPrimary};
    background: ${(props): string => props.theme.contentBackgroundColor};
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 6px;
    box-shadow: ${(props): string => props.theme.boxShadow};
`;

const Header = styled.div`
    display: flex;
    align-items: center;
    justify-content: space-between;
    min-height: 28px;
    padding: 0 2px 6px 6px;
    font-size: 12px;
    font-weight: 600;
`;

const IconButton = styled.button`
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

    &:hover { color: ${(props): string => props.theme.primaryColor}; background: ${(props): string => props.theme.bgColorLight}; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; outline-offset: 1px; }
`;

const LayerButton = styled.button`
    display: grid;
    grid-template-columns: 20px minmax(0, 1fr) auto;
    align-items: center;
    gap: 8px;
    width: 100%;
    min-height: 34px;
    padding: 0 8px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;
    text-align: left;
    background: transparent;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    transition: background-color 160ms ease-out;

    &:hover { background: ${(props): string => props.theme.bgColorLight}; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; outline-offset: -2px; }
    > svg { justify-self: center; width: 15px; height: 15px; color: ${(props): string => props.theme.iconColor}; }
`;

const Label = styled.span`
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const State = styled.span<{ visible: boolean }>`
    display: inline-flex;
    align-items: center;
    gap: 4px;
    color: ${(props): string => props.visible ? props.theme.primaryColor : props.theme.textColorSecondary};
    font-size: 11px;
    white-space: nowrap;
`;

const LAYERS: Array<{ id: LifecycleGraphLayer; label: string; icon: JSX.Element }> = [
    { id: 'blocks', label: 'memoryBlockLayer', icon: <BorderOutlined /> },
    { id: 'overview', label: 'memoryOverviewLayer', icon: <LineChartOutlined /> },
    { id: 'markers', label: 'memoryMarkerLayer', icon: <FlagOutlined /> },
];

export const LifecycleGraphLayerPanel = ({
    visibility,
    onChange,
    onClose,
}: {
    visibility: LifecycleGraphLayerVisibility;
    onChange: (layer: LifecycleGraphLayer) => void;
    onClose: () => void;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    return <Panel role="group" aria-label={t('graphLayers')} data-testid="lifecycleGraphLayerPanel">
        <Header>
            <span>{t('layerVisibilityTitle')}</span>
            <IconButton type="button" aria-label={t('closeLayerVisibility')} onClick={onClose}>
                <CloseOutlined />
            </IconButton>
        </Header>
        {LAYERS.map(layer => <LayerButton
            key={layer.id}
            type="button"
            aria-pressed={visibility[layer.id]}
            onClick={(): void => onChange(layer.id)}
        >
            {layer.icon}
            <Label>{t(layer.label)}</Label>
            <State visible={visibility[layer.id]}>
                {visibility[layer.id] ? <EyeOutlined /> : <EyeInvisibleOutlined />}
                <span>{t(visibility[layer.id] ? 'layerVisible' : 'layerHidden')}</span>
            </State>
        </LayerButton>)}
    </Panel>;
};
