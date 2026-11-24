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
import React, { useRef } from 'react';
import { observer } from 'mobx-react';
import styled from '@emotion/styled';
import { ReturnIcon } from '@insight/lib/icon';
import { Button, Tooltip } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import type { Session } from '@/entity/session';
import { clusterView } from '@/cluster/viewVersion';
import { isInClusterCompare } from '@/utils/Compare';

const SWITCH_COOLDOWN_MS = 500;

const ToolButton = styled(Button)`
    && {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        gap: 8px;
        width: auto;
        min-width: 0;
        height: 20px;
        padding: 0;
        border: 0;
        box-shadow: none;
        font-size: 12px;
        line-height: 20px;
        color: inherit;
        background: transparent;

        &:hover, &:focus, &:active, &:disabled {
            background: transparent;
            box-shadow: none;
            color: inherit;
        }
        &:disabled {
            color: ${({ theme }): string => theme.textColorDisabled};
        }
        > span {
            color: inherit;
        }
        &:focus-visible {
            outline: 2px solid ${({ theme }): string => theme.primaryColor};
            outline-offset: 3px;
        }
    }
`;

const SwitchClusterView = observer(({ session }: { session: Session }) => {
    const { t } = useTranslation('framework');
    const nextSwitchAt = useRef(0);
    const modern = clusterView.versionFor(session) === 'modern';
    const comparing = isInClusterCompare() || clusterView.comparisonPending;
    const label = t(modern ? 'clusterView.legacy' : 'clusterView.modern');
    const switchVersion = (): void => {
        const now = performance.now();
        if (now < nextSwitchAt.current) { return; }
        if (clusterView.select(modern ? 'legacy' : 'modern', comparing)) {
            nextSwitchAt.current = now + SWITCH_COOLDOWN_MS;
        }
    };
    return <Tooltip placement="bottom" title={modern ? label : comparing ? t('clusterView.exitCompare') : undefined}>
        <ToolButton type="text" size="small" icon={<ReturnIcon aria-hidden="true" focusable="false" />} aria-label={label} disabled={!modern && comparing}
            onClick={switchVersion}>
            {modern ? null : label}
        </ToolButton>
    </Tooltip>;
});

export default SwitchClusterView;
