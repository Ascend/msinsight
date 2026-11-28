/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import React from 'react';
import { observer } from 'mobx-react';
import { keyframes } from '@emotion/react';
import styled from '@emotion/styled';
import SwitchTheme from './SwitchTheme';
import SwitchLanguage from './SwitchLanguage';
import Version from './Version';
import InsightBotEntry from './InsightBotEntry';
import { WorkspaceControlStatus } from '../WorkspaceControlOverlay';
import { CONTROL_BREATH_MS, CONTROL_FADE_MS } from '../../agent/WorkspaceControlState';

const statusPulse = keyframes`
  from { transform: scale(0.75); opacity: 0; }
  24% { opacity: 0.8; }
  to { transform: scale(2.15); opacity: 0; }
`;

const labelBreath = keyframes`
  0%, 100% { text-shadow: 0 0 5px var(--status-halo); }
  50% { text-shadow: 0 0 0 transparent; }
`;

const Container = styled.div`
  position: absolute;
  right: 0;
  margin: 6px 20px 0 0;
  display: flex;
  align-items: center;
  color: ${({ theme }): string => theme.textColorPrimary};
  > * {
    margin-left: 16px;
    cursor: pointer;
  }
  .size-20 > svg {
    width: 20px;
    height: 20px;
  }
  .workspace-control-status {
    --status-accent: ${({ theme }): string => theme.primaryColor};
    --status-halo: color-mix(in srgb, var(--status-accent) 55%, transparent);
    display: inline-flex;
    gap: 12px;
    align-items: center;
    min-width: calc(20px + var(--workspace-status-text-width));
    font-size: 12px;
    white-space: nowrap;
    pointer-events: none;
    transition: opacity ${CONTROL_FADE_MS}ms ease-out;
    &[data-phase='idle'], &[data-phase='fading'] { opacity: 0; }
    &[data-color='input'] { --status-accent: ${({ theme }): string => theme.warningColor}; }
    > i {
      position: relative;
      flex: 0 0 8px;
      height: 8px;
      border-radius: 50%;
      color: var(--status-accent);
      background: currentColor;
      &::after {
        content: '';
        position: absolute;
        inset: -1px;
        border: 1px solid currentColor;
        border-radius: inherit;
        box-shadow: inset 0 0 2px currentColor, 0 0 2px currentColor;
        animation: ${statusPulse} 1.6s ease-out infinite;
      }
    }
    &[data-color='input'] > i {
      border-radius: 1px;
      transform: rotate(45deg);
    }
    > span {
      color: color-mix(in srgb, var(--status-accent) 40%, ${({ theme }): string => theme.textColorPrimary});
      font-weight: 700;
      text-shadow: 0 0 3px var(--status-halo);
      animation: ${labelBreath} ${CONTROL_BREATH_MS}ms ease-in-out infinite;
    }
    @media (prefers-reduced-motion: reduce) {
      transition: none;
      > span, > i::after { animation: none; }
    }
  }
`;

interface ToolBoxProps {
    insightBotActive: boolean;
    onInsightBotClick: () => void;
}

const Index = observer(({ insightBotActive, onInsightBotClick }: ToolBoxProps) => {
    return <Container>
        <WorkspaceControlStatus />
        <SwitchTheme/>
        <SwitchLanguage/>
        <Version/>
        <InsightBotEntry active={insightBotActive} onClick={onInsightBotClick} />
    </Container>;
});

export default Index;
