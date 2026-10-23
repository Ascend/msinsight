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
import React, { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { COMMAND_ERROR_CODES, isCommandErrorShape } from '@insight/lib/FrontendAgentCommand';
import { executeFrontendCommand } from '../bridge/frontendAgentCommandTransport';
import type { ActionItem } from '../types';
import { ActionApprovalCard, type ActionCommandDefinition } from './ActionApprovalCard';

const COMMAND_TIMEOUT_MS = 30000;

type ActionState = 'idle' | 'loading' | 'approval' | 'executing' | 'succeeded' | 'needsChoice' | 'failed';

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

// Content, not the streamed block ID, defines the lifetime of local approval.
export const ActionBlock = ({ action }: { action: ActionItem }): JSX.Element => (
    <ActionContent key={actionFingerprint(action)} action={action} />
);

const ActionContent = ({ action }: { action: ActionItem }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const busy = useRef(false);
    const mounted = useRef(true);
    const approvedDefinition = useRef<string>();
    useEffect(() => {
        mounted.current = true;
        return () => { mounted.current = false; };
    }, []);
    const [state, setState] = useState<ActionState>('idle');
    const [definition, setDefinition] = useState<ActionCommandDefinition>();
    const [error, setError] = useState<string>();
    const [targetModule, setTargetModule] = useState<string>();
    const reportError = (nextError: unknown): void => {
        approvedDefinition.current = undefined;
        const module = isCommandErrorShape(nextError) && nextError.code === COMMAND_ERROR_CODES.UNAVAILABLE &&
            nextError.details?.reason === 'module_not_active' ? nextError.details.targetModule : undefined;
        setTargetModule(typeof module === 'string' && module.trim() ? module.trim() : undefined);
        setError(errorMessage(nextError));
        setState('failed');
    };
    const [frozenAction, setFrozenAction] = useState<ActionItem>();

    const execute = async (snapshot: ActionItem, fingerprint: string): Promise<void> => {
        setState('executing');
        setError(undefined);
        try {
            const commandResult = await executeFrontendCommand(
                snapshot.command,
                snapshot.args,
                crypto.randomUUID(),
                Date.now() + COMMAND_TIMEOUT_MS,
            );
            if (!mounted.current) return;
            const needsChoice = commandResult !== null && typeof commandResult === 'object' &&
                'needsChoice' in commandResult && commandResult.needsChoice === true;
            // 仅成功执行才允许重复免确认；未完成的目标选择不能视为成功。
            approvedDefinition.current = needsChoice ? undefined : fingerprint;
            setState(needsChoice ? 'needsChoice' : 'succeeded');
        } catch (nextError) {
            if (mounted.current) reportError(nextError);
        } finally {
            if (mounted.current) {
                setDefinition(undefined);
                setFrozenAction(undefined);
            }
        }
    };

    const openApproval = async (): Promise<void> => {
        // React state alone cannot exclude two clicks in the same render tick.
        if (busy.current || state === 'approval') return;
        busy.current = true;
        setState('loading');
        setDefinition(undefined);
        setFrozenAction(undefined);
        setError(undefined);
        try {
            const snapshot = cloneAction(action);
            const result = await executeFrontendCommand(
                'help',
                { command: snapshot.command },
                crypto.randomUUID(),
                Date.now() + COMMAND_TIMEOUT_MS,
            );
            // A replaced/unmounted action must never dispatch a delayed preflight result.
            if (!mounted.current) return;
            const commandDefinition = commandDefinitionFromHelp(result, snapshot.command);
            const fingerprint = JSON.stringify(commandDefinition);
            if (approvedDefinition.current === fingerprint) {
                await execute(snapshot, fingerprint);
            } else {
                approvedDefinition.current = undefined;
                setDefinition(commandDefinition);
                setFrozenAction(snapshot);
                setState('approval');
            }
        } catch (nextError) {
            if (mounted.current) reportError(nextError);
        } finally {
            busy.current = false;
        }
    };

    const approve = async (): Promise<void> => {
        if (busy.current || state !== 'approval' || !frozenAction || !definition) return;
        busy.current = true;
        try {
            await execute(frozenAction, JSON.stringify(definition));
        } finally {
            busy.current = false;
        }
    };

    const cancel = (): void => {
        if (busy.current) return;
        setDefinition(undefined);
        setFrozenAction(undefined);
        setError(undefined);
        setState('idle');
    };

    return (
        <Container>
            <button
                className="action-trigger"
                disabled={state === 'loading' || state === 'executing' || state === 'approval'}
                onClick={() => {
                    openApproval();
                }}
                type="button"
            >
                {state === 'loading' ? t('actionLoading') : state === 'executing' ? t('actionExecuting') : action.label}
            </button>
            {state === 'approval' && definition && frozenAction ? (
                <ActionApprovalCard
                    action={frozenAction}
                    definition={definition}
                    executing={false}
                    onApprove={() => {
                        approve();
                    }}
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
            {state === 'succeeded' ? <div className="action-result succeeded">{t('actionSucceeded')}</div> : null}
            {state === 'needsChoice' ? <div className="action-result">{t('actionNeedsChoice')}</div> : null}
            {state === 'failed' ? <div className="action-result failed">{targetModule
                ? t('actionModuleNotActive', { module: targetModule })
                : t('actionFailed', { error })}</div> : null}
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

const actionFingerprint = (action: ActionItem): string => JSON.stringify([
    action.command, action.args, action.description, action.label,
]);

const errorMessage = (error: unknown): string => error instanceof Error ? error.message : String(error);
