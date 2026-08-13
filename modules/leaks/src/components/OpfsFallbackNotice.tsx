/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the License at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import React, { useSyncExternalStore } from 'react';
import { Button } from 'antd';
import { WarningOutlined } from '@ant-design/icons';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { opfsFallbackPrompt } from './opfsFallback';

const FallbackMask = styled.div`
    position: absolute;
    inset: 16px;
    z-index: 10;
    display: flex;
    align-items: center;
    justify-content: center;
    background: var(--mi-bg-color);
`;

const FallbackPanel = styled.div`
    width: min(520px, calc(100% - 48px));
    padding: 24px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 12px;
    background: ${(props): string => props.theme.bgColorCommon};
    box-shadow: ${(props): string => props.theme.boxShadow};
    color: ${(props): string => props.theme.textColorPrimary};
`;

const FallbackTitle = styled.div`
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 12px;
    font-size: 14px;
    font-weight: 500;

    svg {
        color: ${(props): string => props.theme.warningColor};
        font-size: 16px;
    }
`;

const FallbackDescription = styled.div`
    color: ${(props): string => props.theme.textColorSecondary};
    line-height: 1.6;
    white-space: pre-line;
`;

const FallbackActions = styled.div`
    display: flex;
    justify-content: flex-end;
    margin-top: 20px;
`;

const OpfsFallbackNotice = (): React.ReactElement | null => {
    const { t } = useTranslation('leaks');
    const visible = useSyncExternalStore(
        opfsFallbackPrompt.subscribe,
        opfsFallbackPrompt.getSnapshot,
        opfsFallbackPrompt.getSnapshot,
    );

    if (!visible) {
        return null;
    }

    return (
        <FallbackMask
            data-testid="opfsFallbackNotice"
            role="dialog"
            aria-modal="true"
            aria-labelledby="opfsFallbackTitle"
            aria-describedby="opfsFallbackDescription"
        >
            <FallbackPanel>
                <FallbackTitle id="opfsFallbackTitle">
                    <WarningOutlined />
                    {t('opfsUnavailableTitle')}
                </FallbackTitle>
                <FallbackDescription id="opfsFallbackDescription">
                    {t('opfsUnavailableContent')}
                </FallbackDescription>
                <FallbackActions>
                    <Button type="primary" autoFocus onClick={opfsFallbackPrompt.approve}>
                        {t('opfsFallbackConfirm')}
                    </Button>
                </FallbackActions>
            </FallbackPanel>
        </FallbackMask>
    );
};

export default OpfsFallbackNotice;
