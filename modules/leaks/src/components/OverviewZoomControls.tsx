/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React, { useEffect, useState } from 'react';
import styled from '@emotion/styled';
import { Button } from 'antd';
import { useTranslation } from 'react-i18next';

const Controls = styled.div`
    position: absolute;
    top: 6px;
    right: 10px;
    z-index: 5;
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 4px 6px 4px 12px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 18px;
    background: ${(props): string => props.theme.bgColorCommon};
    box-shadow: 0 1px 4px rgba(0, 0, 0, 0.1);
    cursor: default;
    .zoom-percentage { display: flex; align-items: center; height: 32px; min-width: 52px; font-size: 12px; color: ${(props): string => props.theme.textColorSecondary}; }
    button { display: inline-flex; align-items: center; justify-content: center; vertical-align: middle; border-radius: 12px; }
    button > span { display: inline-flex; align-items: center; justify-content: center; line-height: 1; }
    button svg { display: block; flex-shrink: 0; }
    button.zoom-step { width: 32px; height: 32px; padding: 0; line-height: 1; }
    button.zoom-reset { height: 22px; padding: 0 8px; font-size: 11px; line-height: 1; }
`;

interface Props {
    zoom: number;
    activity: number;
    onZoom: (direction: number) => void;
    onReset: () => void;
}

export const OverviewZoomControls = ({ zoom, activity, onZoom, onReset }: Props): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const [visible, setVisible] = useState(false);
    const [focused, setFocused] = useState(false);
    useEffect(() => {
        if (!activity) return;
        setVisible(true);
        if (focused) return;
        const timer = window.setTimeout(() => {
            setVisible(false);
        }, 2000);
        return () => window.clearTimeout(timer);
    }, [activity, focused]);
    return <Controls data-testid="overviewZoomControls" style={{ display: visible ? undefined : 'none' }} onPointerDown={(event): void => event.stopPropagation()}
        onFocus={(): void => setFocused(true)} onBlur={(event): void => {
            if (!event.currentTarget.contains(event.relatedTarget)) setFocused(false);
        }}
        onKeyDown={(event): void => event.stopPropagation()} onWheel={(event): void => event.stopPropagation()}>
        <span className="zoom-percentage">{`${Math.round(zoom * 100)}%`}</span>
        <Button className="zoom-step" type="text" size="small" aria-label={t('overviewZoomOut')} title={t('overviewZoomOut')} onClick={(): void => onZoom(-1)}>
            <svg width="18" height="18" viewBox="0 0 20 20" fill="none" stroke="currentColor" strokeWidth="1.6" aria-hidden="true"><path d="M4 10h12" /></svg>
        </Button>
        <Button className="zoom-step" type="text" size="small" aria-label={t('overviewZoomIn')} title={t('overviewZoomIn')} onClick={(): void => onZoom(1)}>
            <svg width="18" height="18" viewBox="0 0 20 20" fill="none" stroke="currentColor" strokeWidth="1.6" aria-hidden="true"><path d="M4 10h12M10 4v12" /></svg>
        </Button>
        <Button className="zoom-reset" size="small" onClick={onReset}>{t('overviewReset')}</Button>
    </Controls>;
};
