/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React, { useSyncExternalStore } from 'react';
import { keyframes } from '@emotion/react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { frontendAgentCommandController } from '../agent/frontendAgentCommandController';
import { CONTROL_BREATH_MS, CONTROL_FADE_MS, type WorkspaceControlSnapshot } from '../agent/WorkspaceControlState';

const appear = keyframes`
    from { opacity: 0; }
    to { opacity: 1; }
`;

const breathe = keyframes`
    0%, 100% { opacity: 0.56; }
    50% { opacity: 0.18; }
`;

const Outline = styled.div`
    position: fixed;
    inset: 0;
    z-index: 2000;
    pointer-events: none;
    color: ${({ theme }): string => theme.primaryColor};
    animation: ${appear} ${CONTROL_FADE_MS}ms ease-out;
    transition: color 120ms ease, opacity ${CONTROL_FADE_MS}ms ease-out;
    &::before {
        content: '';
        position: absolute;
        inset: 0;
        --inner-glow: color-mix(in srgb, currentColor 24%, transparent);
        --glow-stops: currentColor, var(--inner-glow) 14px, transparent 50%, var(--inner-glow) calc(100% - 14px), currentColor;
        background:
            linear-gradient(to right, var(--glow-stops)),
            linear-gradient(to bottom, var(--glow-stops));
        background-color: color-mix(in srgb, currentColor 20%, transparent);
        opacity: 0.26;
        animation: ${breathe} ${CONTROL_BREATH_MS}ms ease-in-out infinite;
    }
    &[data-color='input'] { color: ${({ theme }): string => theme.warningColor}; }
    &[data-phase='fading'] { opacity: 0; }
    @media (prefers-reduced-motion: reduce) {
        animation: none;
        transition: none;
        &::before { animation: none; }
    }
`;

const useWorkspaceControl = (): WorkspaceControlSnapshot => {
    const { workspaceControl } = frontendAgentCommandController;
    return useSyncExternalStore(workspaceControl.subscribe, workspaceControl.getSnapshot);
};

export const WorkspaceControlOverlay = (): JSX.Element | null => {
    const { phase, color } = useWorkspaceControl();
    if (phase === 'idle') return null;
    return <Outline aria-hidden="true" data-testid="workspace-control-outline" data-phase={phase} data-color={color} />;
};

export const WorkspaceControlStatus = (): JSX.Element => {
    const { t } = useTranslation('framework');
    const { phase, color } = useWorkspaceControl();
    const label = color === 'input' ? t('workspaceControlInput') : t('workspaceControlActive');
    return <span className="workspace-control-status" role="status" aria-atomic="true" data-color={color} data-phase={phase}>
        {phase !== 'idle' && <><i key={color} aria-hidden="true" /><span>{label}</span></>}
    </span>;
};
