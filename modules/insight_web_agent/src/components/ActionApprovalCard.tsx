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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import type { ActionItem } from '../types';

export interface ActionCommandDefinition {
    name: string;
    title: string;
    description: string;
    inputSchema: Record<string, unknown>;
}

const Card = styled.div`
    display: grid;
    gap: 10px;
    margin-top: 8px;
    border: 1px solid ${(props): string => props.theme.primaryColor};
    border-left-width: 3px;
    border-radius: ${(props): string => props.theme.borderRadiusSmall};
    padding: 10px;
    background: ${(props): string => props.theme.bgColorLight};

    .action-approval-title {
        font-weight: 700;
    }

    .action-approval-section {
        display: grid;
        gap: 3px;
    }

    .action-approval-label {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }

    .action-approval-value {
        overflow-wrap: anywhere;
    }

    .action-approval-code {
        max-height: 160px;
        margin: 0;
        overflow: auto;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 7px;
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorPrimary};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 11px;
        overflow-wrap: anywhere;
        white-space: pre-wrap;
    }

    .action-approval-buttons {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
    }

    button {
        cursor: pointer;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 5px 10px;
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
    }

    button.primary {
        border-color: ${(props): string => props.theme.primaryColor};
        background: ${(props): string => props.theme.primaryColor};
        color: #fff;
    }

    button:disabled {
        cursor: not-allowed;
        opacity: 0.6;
    }
`;

export const ActionApprovalCard = ({
    action,
    definition,
    executing,
    onApprove,
    onCancel,
}: {
    action: ActionItem;
    definition: ActionCommandDefinition;
    executing: boolean;
    onApprove: () => void;
    onCancel: () => void;
}): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    return (
        <Card>
            <div className="action-approval-title">{t('actionApprovalTitle')}</div>
            <section className="action-approval-section">
                <span className="action-approval-label">{t('actionTrustedCapability')}</span>
                <strong className="action-approval-value">{definition.title}</strong>
                <span className="action-approval-value">{definition.description}</span>
            </section>
            <section className="action-approval-section">
                <span className="action-approval-label">{t('actionRequestedBehavior')}</span>
                <span className="action-approval-value">{action.description}</span>
            </section>
            <section className="action-approval-section">
                <span className="action-approval-label">{t('actionCommand')}</span>
                <pre className="action-approval-code">{action.command}</pre>
            </section>
            <section className="action-approval-section">
                <span className="action-approval-label">{t('actionArguments')}</span>
                <pre className="action-approval-code">{JSON.stringify(action.args, null, 2)}</pre>
            </section>
            <div className="action-approval-buttons">
                <button className="primary" disabled={executing} onClick={onApprove} type="button">
                    {executing ? t('actionExecuting') : t('actionApprove')}
                </button>
                <button disabled={executing} onClick={onCancel} type="button">{t('cancel')}</button>
            </div>
        </Card>
    );
};
