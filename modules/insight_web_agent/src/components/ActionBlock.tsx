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
import { cancelFrontendCommand, executeFrontendCommand } from '../bridge/frontendAgentCommandTransport';
import type { ActionItem } from '../types';
import { ActionApprovalCard, type ActionCommandDefinition } from './ActionApprovalCard';

const COMMAND_TIMEOUT_MS = 30000;

type ActionState = 'idle' | 'loading' | 'approval' | 'executing' | 'succeeded' | 'needsChoice' | 'failed';

interface PendingApproval {
    action: ActionItem;
    definition: ActionCommandDefinition;
    targetModule?: string;
}

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
    const pendingSwitch = useRef<string>();
    const approvedDefinition = useRef<string>();
    useEffect(() => {
        mounted.current = true;
        return () => {
            mounted.current = false;
            if (pendingSwitch.current) cancelFrontendCommand(pendingSwitch.current);
        };
    }, []);
    const [state, setState] = useState<ActionState>('idle');
    const [pendingApproval, setPendingApproval] = useState<PendingApproval>();
    const [error, setError] = useState<string>();
    const reportError = (nextError: unknown): void => {
        approvedDefinition.current = undefined;
        setPendingApproval(undefined);
        setError(errorMessage(nextError));
        setState('failed');
    };
    const showApproval = (approval: PendingApproval): void => {
        approvedDefinition.current = undefined;
        setPendingApproval(approval);
        setState('approval');
    };

    const execute = async (approval: PendingApproval): Promise<void> => {
        const { action: snapshot, definition, targetModule } = approval;
        setState('executing');
        setError(undefined);
        try {
            if (targetModule) {
                pendingSwitch.current = crypto.randomUUID();
                try {
                    await executeFrontendCommand(
                        'framework.switchModule',
                        { moduleId: targetModule },
                        pendingSwitch.current,
                        Date.now() + COMMAND_TIMEOUT_MS,
                    );
                } finally {
                    pendingSwitch.current = undefined;
                }
                if (!mounted.current) return;
                const currentDefinition = await loadCommandDefinition(snapshot.command);
                if (!mounted.current) return;
                // 跨页授权只适用于已确认的定义，切换后必须重新核验。
                if (JSON.stringify(currentDefinition) !== JSON.stringify(definition)) {
                    showApproval({ action: snapshot, definition: currentDefinition });
                    return;
                }
            }
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
            approvedDefinition.current = needsChoice ? undefined : JSON.stringify(definition);
            setPendingApproval(undefined);
            setState(needsChoice ? 'needsChoice' : 'succeeded');
        } catch (nextError) {
            if (!mounted.current) return;
            // 已同意的操作可恢复一次跨页拒绝，恢复链再次失败时直接终止。
            const target = !targetModule ? inactiveTargetModule(nextError, snapshot.command) : undefined;
            if (target) await execute({ action: snapshot, definition, targetModule: target });
            else reportError(nextError);
        }
    };

    const openApproval = async (): Promise<void> => {
        // React state alone cannot exclude two clicks in the same render tick.
        if (busy.current || state === 'approval') return;
        busy.current = true;
        setState('loading');
        setPendingApproval(undefined);
        setError(undefined);
        try {
            const snapshot = cloneAction(action);
            let approval: PendingApproval;
            try {
                approval = { action: snapshot, definition: await loadCommandDefinition(snapshot.command) };
            } catch (nextError) {
                const targetModule = inactiveTargetModule(nextError, snapshot.command);
                if (!targetModule || !isCommandErrorShape(nextError)) throw nextError;
                const definition = commandDefinitionFromHelp({ command: nextError.details?.commandDefinition }, snapshot.command);
                approval = { action: snapshot, definition, targetModule };
            }
            // A replaced/unmounted action must never dispatch a delayed preflight result.
            if (!mounted.current) return;
            if (approvedDefinition.current === JSON.stringify(approval.definition)) {
                await execute(approval);
            } else {
                showApproval(approval);
            }
        } catch (nextError) {
            if (mounted.current) reportError(nextError);
        } finally {
            busy.current = false;
        }
    };

    const approve = async (): Promise<void> => {
        if (busy.current || state !== 'approval' || !pendingApproval) return;
        busy.current = true;
        try {
            await execute(pendingApproval);
        } finally {
            busy.current = false;
        }
    };

    const cancel = (): void => {
        if (busy.current) return;
        setPendingApproval(undefined);
        setError(undefined);
        setState('idle');
    };

    const approvalCard = pendingApproval && (state === 'approval' || state === 'executing') ? (
        <ActionApprovalCard
            action={pendingApproval.action}
            definition={pendingApproval.definition}
            executing={state === 'executing'}
            onApprove={() => { void approve(); }}
            onCancel={cancel}
        />
    ) : null;

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
            {approvalCard}
            {state === 'succeeded' ? <div className="action-result succeeded">{t('actionSucceeded')}</div> : null}
            {state === 'needsChoice' ? <div className="action-result">{t('actionNeedsChoice')}</div> : null}
            {state === 'failed' ? <div className="action-result failed">{t('actionFailed', { error })}</div> : null}
        </Container>
    );
};

const loadCommandDefinition = async (command: string): Promise<ActionCommandDefinition> => {
    const result = await executeFrontendCommand('help', { command }, crypto.randomUUID(), Date.now() + COMMAND_TIMEOUT_MS);
    return commandDefinitionFromHelp(result, command);
};

const inactiveTargetModule = (error: unknown, command: string): string | undefined => {
    if (!isCommandErrorShape(error) || error.code !== COMMAND_ERROR_CODES.UNAVAILABLE ||
        error.details?.reason !== 'module_not_active' || error.details.command !== command) return undefined;
    const target = error.details.targetModule;
    return typeof target === 'string' && target.trim() ? target.trim() : undefined;
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
