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
import styled from '@emotion/styled';
import { useState } from 'react';
import { useTranslation } from 'react-i18next';
import { executeFrontendCommand } from '../bridge/frontendAgentCommandTransport';
import type { ActionItem } from '../types';
import { ActionApprovalCard, type ActionCommandDefinition } from './ActionApprovalCard';

const COMMAND_TIMEOUT_MS = 30000;

type ActionState = 'idle' | 'loading' | 'approval' | 'executing' | 'succeeded' | 'failed';

const Container = styled.div`
    display: grid;
    gap: 6px;
    margin: 8px 0;

    .action-trigger {
        width: fit-content;
        max-width: 100%;
        cursor: pointer;
        border: 1px solid ${(props): string => props.theme.primaryColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 6px 10px;
        background: transparent;
        color: ${(props): string => props.theme.primaryColor};
        font-weight: 700;
        overflow-wrap: anywhere;
        text-align: left;
    }

    .action-trigger:disabled {
        cursor: wait;
        opacity: 0.6;
    }

    .action-result {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        overflow-wrap: anywhere;
    }

    .action-result.failed {
        color: ${(props): string => props.theme.dangerColor};
    }

    .action-result.succeeded {
        color: ${(props): string => props.theme.successColor};
    }
`;

export const ActionBlock = ({ action }: { action: ActionItem }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const [state, setState] = useState<ActionState>('idle');
    const [definition, setDefinition] = useState<ActionCommandDefinition>();
    const [error, setError] = useState<string>();
    const [result, setResult] = useState<string>();
    const [frozenAction, setFrozenAction] = useState<ActionItem>();

    const openApproval = async (): Promise<void> => {
        if (state === 'loading' || state === 'executing') return;
        setState('loading');
        setDefinition(undefined);
        setFrozenAction(undefined);
        setError(undefined);
        setResult(undefined);
        try {
            const result = await executeFrontendCommand(
                'help',
                { command: action.command },
                crypto.randomUUID(),
                Date.now() + COMMAND_TIMEOUT_MS,
            );
            const commandDefinition = commandDefinitionFromHelp(result, action.command);
            setDefinition(commandDefinition);
            setFrozenAction(cloneAction(action));
            setState('approval');
        } catch (nextError) {
            setError(errorMessage(nextError));
            setState('failed');
        }
    };

    const approve = async (): Promise<void> => {
        if (state !== 'approval' || !frozenAction) return;
        setState('executing');
        setError(undefined);
        try {
            const commandResult = await executeFrontendCommand(
                frozenAction.command,
                frozenAction.args,
                crypto.randomUUID(),
                Date.now() + COMMAND_TIMEOUT_MS,
            );
            setResult(formatCommandResult(commandResult));
            setState('succeeded');
        } catch (nextError) {
            setError(errorMessage(nextError));
            setState('failed');
        } finally {
            setDefinition(undefined);
            setFrozenAction(undefined);
        }
    };

    const cancel = (): void => {
        setDefinition(undefined);
        setFrozenAction(undefined);
        setError(undefined);
        setResult(undefined);
        setState('idle');
    };

    return (
        <Container>
            <button
                className="action-trigger"
                disabled={state === 'loading' || state === 'executing'}
                onClick={() => { void openApproval(); }}
                type="button"
            >
                {state === 'loading' ? t('actionLoading') : action.label}
            </button>
            {state === 'approval' && definition && frozenAction ? (
                <ActionApprovalCard
                    action={frozenAction}
                    definition={definition}
                    executing={false}
                    onApprove={() => { void approve(); }}
                    onCancel={cancel}
                />
            ) : null}
            {state === 'executing' && definition && frozenAction ? (
                <ActionApprovalCard
                    action={frozenAction}
                    definition={definition}
                    executing
                    onApprove={() => undefined}
                    onCancel={() => undefined}
                />
            ) : null}
            {state === 'succeeded' ? <div className="action-result succeeded">{t('actionSucceeded')}{result ? ` ${result}` : ''}</div> : null}
            {state === 'failed' ? <div className="action-result failed">{t('actionFailed', { error })}</div> : null}
        </Container>
    );
};

const commandDefinitionFromHelp = (value: unknown, expectedCommand: string): ActionCommandDefinition => {
    const root = objectValue(value, 'help result');
    const command = objectValue(root.command, 'command definition');
    const name = nonEmptyString(command.name, 'command name');
    const title = nonEmptyString(command.title, 'command title');
    const description = nonEmptyString(command.description, 'command description');
    const inputSchema = objectValue(command.inputSchema, 'command input schema');
    if (name !== expectedCommand) throw new Error('The page returned a different command definition.');
    return { name, title, description, inputSchema };
};

const cloneAction = (action: ActionItem): ActionItem => ({
    ...action,
    args: structuredClone(action.args),
});

const objectValue = (value: unknown, name: string): Record<string, unknown> => {
    if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`Invalid ${name}.`);
    return value as Record<string, unknown>;
};

const nonEmptyString = (value: unknown, name: string): string => {
    if (typeof value !== 'string' || !value.trim()) throw new Error(`Invalid ${name}.`);
    return value.trim();
};

const formatCommandResult = (value: unknown): string | undefined => {
    if (value === undefined) return undefined;
    if (typeof value === 'string') return truncateResult(value);
    try {
        const serialized = JSON.stringify(value);
        return truncateResult(serialized ?? String(value));
    } catch (_error) {
        return truncateResult(String(value));
    }
};

const truncateResult = (value: string): string => value.length > 500 ? `${value.slice(0, 499)}…` : value;

const errorMessage = (error: unknown): string => error instanceof Error ? error.message : String(error);
