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
import { Drawer, message } from 'antd';
import React, { lazy, Suspense, useEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Alert, Button, Input, InputNumber, PasswordInput } from '@insight/lib/components';
import { copyToClipboard } from '@insight/lib/utils';
import { fetchAgentConfig, isBackendUnavailableError, saveAgentServersConfig, saveAgentSessionConfig, saveBuiltinAgentConfig } from '../api';
import { useChatState } from '../hooks/useChatState';
import type { AgentConfigSnapshot } from '../types';
import addIcon from '../icons/add.svg';
import arrowDownIcon from '../icons/arrow-down.svg';
import backIcon from '../icons/back.svg';
import agentLogo from '../icons/logo.png';
import deleteIcon from '../icons/delete.svg';
import copyIcon from '../icons/copy.svg';

const JsonEditor = lazy(async () => {
    const module = await import('./JsonEditor');
    return { default: module.JsonEditor };
});

const Container = styled.div`
    display: inline-flex;
    align-items: center;

    .settings-trigger {
        display: inline-flex;
        align-items: center;
    }

    .settings-drawer .ant-drawer-content {
        background: ${(props): string => props.theme.bgColor};
    }

    .settings-drawer.ant-drawer-right {
        position: fixed;
        inset: 0;
    }

    .settings-drawer .ant-drawer-header {
        padding: 14px 16px;
        border-bottom: 0;
        background: ${(props): string => props.theme.bgColor};
        transition: box-shadow 0.15s ease;
    }

    .settings-drawer.content-scrolled .ant-drawer-header {
        z-index: 1;
        box-shadow: ${(props): string => props.theme.boxShadowLighter};
    }

    .settings-drawer .ant-drawer-title,
    .settings-drawer .ant-drawer-close {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .settings-header {
        display: flex;
        align-items: center;
        gap: 10px;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 16px;
        font-weight: 500;
        line-height: 32px;
    }

    .settings-back {
        width: 24px;
        height: 24px;
        flex: 0 0 24px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        padding: 0;
        background: ${(props): string => props.theme.bgColorDark};
        cursor: pointer;
    }

    .settings-back img {
        width: 16px;
        height: 16px;
        filter: ${(props): string => props.theme.mode === 'dark' ? 'invert(1)' : 'none'};
        opacity: 0.45;
    }

    .settings-drawer .ant-drawer-body {
        padding: 0;
        background: ${(props): string => props.theme.bgColor};
        overflow: hidden;
    }

    .settings-layout {
        height: 100%;
        min-height: 0;
        display: grid;
        grid-template-rows: minmax(0, 1fr) auto;
    }

    .panel {
        min-height: 0;
        display: grid;
        align-content: start;
        gap: 14px;
        padding: 16px;
        overflow: auto;
    }

    .section {
        display: grid;
        gap: 10px;
        padding: 0;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        background: ${(props): string => props.theme.bgColor};
    }

    .session-section {
        padding-top: 16px;
        border-top: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: 0;
    }

    .session-toggle {
        width: fit-content;
        display: inline-flex;
        align-items: center;
        gap: 10px;
        border: 0;
        padding: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        font-weight: 400;
        line-height: 24px;
        cursor: pointer;
    }

    .session-chevron {
        width: 16px;
        height: 16px;
        filter: ${(props): string => props.theme.mode === 'dark' ? 'invert(1)' : 'none'};
        opacity: 0.85;
        transform: rotate(-90deg);
        transition: transform 0.15s ease;
    }

    .session-toggle[aria-expanded='true'] .session-chevron {
        transform: rotate(0deg);
    }

    .session-content {
        display: grid;
        gap: 10px;
        margin-left: 8px;
        border-left: 1px solid ${(props): string => props.theme.borderColor};
        padding-left: 18px;
    }

    .section-title {
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        font-weight: 700;
        letter-spacing: 0.04em;
        text-transform: uppercase;
    }

    .agent-grid {
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
        gap: 8px;
    }

    .agent-card {
        min-width: 0;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 10px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 14px 12px;
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
        transition: border-color 0.15s ease, box-shadow 0.15s ease, background 0.15s ease;
    }

    .agent-card:hover {
        border-color: ${(props): string => props.theme.primaryColor};
    }

    .agent-card.selected {
        border-color: ${(props): string => props.theme.primaryColor};
        box-shadow: inset 0 0 0 1px ${(props): string => props.theme.primaryColor};
    }

    .agent-card-icon {
        width: 26px;
        height: 26px;
        flex: 0 0 26px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.primaryColor};
        font-size: 13px;
        font-weight: 600;
        text-transform: uppercase;
    }

    .agent-card-icon.logo {
        border-radius: 0;
        background: transparent;
    }

    .agent-card-icon img {
        width: 26px;
        height: 26px;
        object-fit: contain;
    }

    .agent-card-name {
        width: 100%;
        overflow: hidden;
        font-size: 14px;
        font-weight: 400;
        line-height: 20px;
        text-align: center;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .agent-card.add-card {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .add-icon {
        position: relative;
        width: 26px;
        height: 26px;
    }

    .add-icon::before,
    .add-icon::after {
        position: absolute;
        top: 50%;
        left: 50%;
        content: '';
        background: ${(props): string => props.theme.textColorSecondary};
        transform: translate(-50%, -50%);
    }

    .add-icon::before {
        width: 22px;
        height: 1px;
    }

    .add-icon::after {
        width: 1px;
        height: 22px;
    }

    .config-tabs {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        margin-top: 8px;
        padding: 2px;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        background: ${(props): string => props.theme.bgColorDark};
    }

    .config-tab {
        height: 28px;
        border: 0;
        border-radius: 7px;
        padding: 0 12px;
        background: transparent;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 14px;
        font-weight: 400;
        cursor: pointer;
    }

    .config-tab.active {
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.primaryColor};
        font-weight: 500;
    }

    .script-config-panel {
        display: grid;
        gap: 8px;
    }

    .script-editor-shell {
        min-width: 0;
        overflow: hidden;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        background: ${(props): string => props.theme.bgColor};
    }

    .script-editor-shell:focus-within {
        border-color: ${(props): string => props.theme.primaryColor};
        box-shadow: 0 0 0 1px ${(props): string => props.theme.primaryColor};
    }

    .script-config-toolbar {
        min-height: 40px;
        display: flex;
        align-items: center;
        justify-content: space-between;
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
        padding: 0 12px;
        background: ${(props): string => props.theme.bgColorLight};
    }

    .script-config-label {
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        font-weight: 400;
        line-height: 20px;
    }

    .script-config-actions {
        display: inline-flex;
        align-items: center;
        gap: 8px;
    }

    .script-format-button {
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        padding: 5px 8px;
        background: transparent;
        color: ${(props): string => props.theme.primaryColor};
        font-size: 13px;
        cursor: pointer;
    }

    .script-format-button:hover,
    .script-copy-button:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }

    .script-copy-button {
        width: 32px;
        height: 32px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        padding: 0;
        background: transparent;
        cursor: pointer;
    }

    .script-copy-button img {
        width: 20px;
        height: 20px;
        filter: ${(props): string => props.theme.mode === 'dark' ? 'brightness(0) invert(1)' : 'none'};
        opacity: 0.85;
    }

    .script-error {
        color: ${(props): string => props.theme.dangerColor};
        font-size: 12px;
        line-height: 18px;
    }

    .row,
    .kv-row,
    .array-row {
        display: grid;
        gap: 8px;
    }

    .kv-row {
        grid-template-columns: minmax(0, 1fr) minmax(0, 1fr) auto;
        align-items: center;
    }

    .array-row {
        grid-template-columns: minmax(0, 1fr) auto;
        align-items: center;
    }

    label {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 14px;
        font-weight: 400;
    }

    .settings-password {
        width: 100%;
        font-size: 14px;
    }

    .settings-input {
        width: 100%;
        font-size: 14px;
    }

    .settings-input[readonly] {
        background: ${(props): string => props.theme.bgColorLight};
        color: ${(props): string => props.theme.textColorSecondary};
        cursor: default;
    }

    .number-field {
        width: 50%;
        display: grid;
        grid-template-columns: minmax(0, 1fr) auto;
        align-items: center;
        gap: 8px;
    }

    .session-number {
        width: 100%;
        font-size: 14px;
    }

    .number-unit {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        line-height: 32px;
    }

    .config-group-title {
        margin-top: 6px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 14px;
        font-weight: 400;
        line-height: 22px;
    }

    .session-content .check-row {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .extra-path-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 12px;
    }

    .extra-path-section {
        margin-top: 8px;
    }

    .path-add {
        display: inline-flex;
        align-items: center;
        gap: 4px;
        flex: 0 0 auto;
        border: 0;
        padding: 2px 0;
        background: transparent;
        color: ${(props): string => props.theme.primaryColor};
        font-size: 14px;
        cursor: pointer;
    }

    .path-add-icon {
        width: 16px;
        height: 16px;
        background: currentColor;
        mask: url(${addIcon}) center / contain no-repeat;
    }

    .path-row {
        min-width: 0;
        display: grid;
        grid-template-columns: minmax(0, 1fr) 28px;
        align-items: center;
        gap: 8px;
    }

    .env-row {
        min-width: 0;
        display: grid;
        grid-template-columns: minmax(0, 1fr) minmax(0, 1fr) 28px;
        align-items: center;
        gap: 8px;
    }

    .path-remove {
        width: 28px;
        height: 28px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorSecondary};
        cursor: pointer;
    }

    .path-remove:hover {
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.dangerColor};
    }

    .path-remove-icon {
        width: 16px;
        height: 16px;
        background: currentColor;
        mask: url(${deleteIcon}) center / contain no-repeat;
    }

    .path-empty {
        padding: 4px 0;
        color: ${(props): string => props.theme.textColorPlaceholder};
        font-size: 12px;
        line-height: 20px;
        text-align: center;
    }

    .inline-actions,
    .check-row {
        display: flex;
        align-items: center;
        gap: 8px;
        flex-wrap: wrap;
    }

    .settings-footer {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 16px;
        padding: 12px 16px 16px;
        background: ${(props): string => props.theme.bgColor};
        transition: box-shadow 0.15s ease;
    }

    .settings-footer.has-content-below {
        z-index: 1;
        box-shadow: ${(props): string => props.theme.boxShadowLighter};
    }

    .settings-footer-actions {
        display: flex;
        align-items: center;
        gap: 8px;
        flex: 0 0 auto;
    }

    .hint,
    .warning,
    .error {
        font-size: 12px;
        line-height: 1.5;
    }

    .hint {
        color: ${(props): string => props.theme.textColorSecondary};
    }

    .warning {
        color: ${(props): string => props.theme.warningColor ?? props.theme.primaryColor};
    }

    .error {
        color: ${(props): string => props.theme.dangerColor};
    }

    .toggle-button {
        width: 26px;
        height: 26px;
        flex: 0 0 26px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }

    .toggle-button:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }
`;

interface AgentSettingsDialogProps {
    trigger?: React.ReactNode;
    open?: boolean;
    createOnOpen?: boolean;
    onOpenChange?: (open: boolean) => void;
}

interface DraftAgent {
    name: string;
    command: string;
    args: string[];
    env: Array<{ key: string; value: string }>;
    saveAndSwitch: boolean;
}

const EMPTY_DRAFT = (): DraftAgent => ({
    name: '',
    command: '',
    args: [],
    env: [],
    saveAndSwitch: false,
});

export const AgentSettingsDialog = ({ trigger, open: controlledOpen, createOnOpen = false, onOpenChange }: AgentSettingsDialogProps): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const { applyAgentConfigSnapshot, pendingPrompt } = useChatState();
    const [internalOpen, setInternalOpen] = useState(false);
    const open = controlledOpen ?? internalOpen;
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [snapshot, setSnapshot] = useState<AgentConfigSnapshot | null>(null);
    const initialSnapshotRef = useRef<AgentConfigSnapshot | null>(null);
    const panelRef = useRef<HTMLDivElement | null>(null);
    const [selectedAgentName, setSelectedAgentName] = useState<string | null>(null);
    const [saveAndSwitchSelected, setSaveAndSwitchSelected] = useState(false);
    const [draftAgent, setDraftAgent] = useState<DraftAgent | null>(null);
    const [showAdvanced, setShowAdvanced] = useState(false);
    const [configMode, setConfigMode] = useState<'form' | 'script'>('form');
    const [scriptValue, setScriptValue] = useState('');
    const [scriptError, setScriptError] = useState<string | null>(null);
    const [panelScrollState, setPanelScrollState] = useState({ hasContentAbove: false, hasContentBelow: false });
    const [error, setError] = useState<string | null>(null);

    const setOpen = (nextOpen: boolean): void => {
        if (controlledOpen === undefined) setInternalOpen(nextOpen);
        onOpenChange?.(nextOpen);
    };

    const updatePanelScrollState = (element: HTMLDivElement | null): void => {
        if (!element) return;
        setPanelScrollState({
            hasContentAbove: element.scrollTop > 0,
            hasContentBelow: element.scrollTop + element.clientHeight < element.scrollHeight - 1,
        });
    };

    useEffect(() => {
        if (!open) return;
        setLoading(true);
        setError(null);
        setSaveAndSwitchSelected(false);
        setDraftAgent(createOnOpen ? EMPTY_DRAFT() : null);
        setShowAdvanced(false);
        setConfigMode('form');
        setPanelScrollState({ hasContentAbove: false, hasContentBelow: false });
        const loadSettings = async (): Promise<void> => {
            try {
                const nextSnapshot = await fetchAgentConfig();
                setSnapshot(nextSnapshot);
                initialSnapshotRef.current = nextSnapshot;
                const editableActiveAgent = nextSnapshot.activeAgentName === 'msinsight-native'
                    || nextSnapshot.agentServers.some((agent) => agent.name === nextSnapshot.activeAgentName);
                setSelectedAgentName(editableActiveAgent ? nextSnapshot.activeAgentName : 'msinsight-native');
                if (createOnOpen) setDraftAgent(EMPTY_DRAFT());
            } catch (nextError) {
                setError(nextError instanceof Error ? nextError.message : String(nextError));
            } finally {
                setLoading(false);
            }
        };
        loadSettings();
    }, [createOnOpen, open]);

    useEffect(() => {
        if (!open) return;
        const frame = requestAnimationFrame(() => updatePanelScrollState(panelRef.current));
        return () => cancelAnimationFrame(frame);
    }, [configMode, draftAgent, loading, open, showAdvanced, snapshot]);

    const activeAgent = useMemo(() => {
        if (!snapshot) return undefined;
        return snapshot.agentServers.find((agent) => agent.name === (selectedAgentName ?? snapshot.activeAgentName));
    }, [selectedAgentName, snapshot]);

    const isBuiltinSelected = !draftAgent && selectedAgentName === 'msinsight-native';

    const envEntries = useMemo<Array<[string, string]>>(() => {
        if (draftAgent) return draftAgent.env.map((entry) => [entry.key, entry.value]);
        if (!activeAgent) return [];
        return Object.entries(activeAgent.env ?? {}) as Array<[string, string]>;
    }, [activeAgent, draftAgent]);

    const editingAgent = draftAgent ?? activeAgent;
    const isCreatingAgent = Boolean(draftAgent);

    const getBuiltinScriptConfig = (): Record<string, unknown> => {
        const { provider = '', model = '', baseUrl = '', apiKey = '' } = snapshot?.builtinAgent ?? {};
        return { provider, model, baseUrl, apiKey };
    };

    const getExternalScriptConfig = (agent: { name?: string; command: string; args: string[]; env: Record<string, string> }, includeName: boolean): Record<string, unknown> => ({
        ...(includeName ? { name: agent.name ?? '' } : {}),
        command: agent.command,
        args: agent.args,
        env: agent.env,
    });

    const getScriptConfig = (): Record<string, unknown> => {
        if (isBuiltinSelected) return getBuiltinScriptConfig();
        if (draftAgent) {
            return getExternalScriptConfig({
                name: draftAgent.name,
                command: draftAgent.command,
                args: draftAgent.args,
                env: Object.fromEntries(draftAgent.env.map(({ key, value }) => [key, value])),
            }, true);
        }
        return getExternalScriptConfig(activeAgent ?? { command: '', args: [], env: {} }, false);
    };

    const resetScriptValue = (): void => {
        setScriptValue(JSON.stringify(getScriptConfig(), null, 2));
        setScriptError(null);
    };

    const applyScriptValue = (value: string): void => {
        setScriptValue(value);
        try {
            const parsed: unknown = JSON.parse(value);
            if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) throw new Error(t('jsonObjectRequired'));
            const config = parsed as Record<string, unknown>;
            if (isBuiltinSelected) {
                if (['provider', 'model', 'baseUrl', 'apiKey'].some((key) => typeof config[key] !== 'string')) throw new Error(t('builtinJsonInvalid'));
                setSnapshot((current) => current ? ({
                    ...current,
                    builtinAgent: {
                        ...current.builtinAgent,
                        provider: config.provider as string,
                        model: config.model as string,
                        baseUrl: config.baseUrl as string,
                        apiKey: config.apiKey as string,
                    },
                }) : current);
            } else {
                const validEnv = config.env && typeof config.env === 'object' && !Array.isArray(config.env)
                    && Object.values(config.env).every((item) => typeof item === 'string');
                if (typeof config.command !== 'string' || !Array.isArray(config.args)
                    || !config.args.every((item) => typeof item === 'string') || !validEnv) throw new Error(t('agentJsonInvalid'));
                const env = config.env as Record<string, string>;
                if (draftAgent) {
                    if (typeof config.name !== 'string') throw new Error(t('agentJsonInvalid'));
                    setDraftAgent((current) => current ? ({
                        ...current,
                        name: config.name as string,
                        command: config.command as string,
                        args: config.args as string[],
                        env: Object.entries(env).map(([key, entryValue]) => ({ key, value: entryValue })),
                    }) : current);
                } else {
                    updateActiveAgent((agent) => ({
                        ...agent,
                        command: config.command as string,
                        args: config.args as string[],
                        env,
                    }));
                }
            }
            setScriptError(null);
        } catch (nextError) {
            setScriptError(nextError instanceof Error ? nextError.message : String(nextError));
        }
    };

    const switchConfigMode = (mode: 'form' | 'script'): void => {
        if (mode === 'script') resetScriptValue();
        if (mode === 'form' && scriptError) return;
        setConfigMode(mode);
    };

    const formatScriptValue = (): void => {
        try {
            applyScriptValue(JSON.stringify(JSON.parse(scriptValue), null, 2));
        } catch (nextError) {
            setScriptError(nextError instanceof Error ? nextError.message : String(nextError));
        }
    };

    const copyScriptValue = async (): Promise<void> => {
        await copyToClipboard(scriptValue);
    };

    const selectAgent = (agentName: string): void => {
        if (configMode === 'script') {
            const nextConfig = agentName === 'msinsight-native'
                ? getBuiltinScriptConfig()
                : getExternalScriptConfig(snapshot?.agentServers.find((agent) => agent.name === agentName) ?? { command: '', args: [], env: {} }, false);
            setScriptValue(JSON.stringify(nextConfig, null, 2));
            setScriptError(null);
        }
        setDraftAgent(null);
        setSelectedAgentName(agentName);
        setSaveAndSwitchSelected(false);
    };

    const createAgent = (): void => {
        const emptyDraft = EMPTY_DRAFT();
        if (configMode === 'script') {
            setScriptValue(JSON.stringify(getExternalScriptConfig({ ...emptyDraft, env: {} }, true), null, 2));
            setScriptError(null);
        }
        setDraftAgent(emptyDraft);
        setSaveAndSwitchSelected(false);
    };

    const extraPaths = snapshot?.sessionConfig.defaultAllowlist.extraPaths ?? [];

    const updateActiveAgent = (updater: (agent: NonNullable<typeof activeAgent>) => NonNullable<typeof activeAgent>) => {
        setSnapshot((current) => {
            if (!current) return current;
            const currentAgent = current.agentServers.find((agent) => agent.name === (selectedAgentName ?? current.activeAgentName)) ?? current.agentServers[0];
            if (!currentAgent) return current;
            const nextAgent = updater(currentAgent);
            return {
                ...current,
                agentServers: current.agentServers.map((agent) => agent.name === currentAgent.name ? nextAgent : agent),
            };
        });
    };

    const updateEditingAgent = (updater: (agent: { command: string; args: string[] }) => { command: string; args: string[] }): void => {
        if (draftAgent) {
            setDraftAgent((current) => current ? { ...current, ...updater(current) } : current);
            return;
        }
        updateActiveAgent((agent) => ({ ...agent, ...updater(agent) }));
    };

    const updateEditingEnv = (nextEntries: Array<[string, string]>): void => {
        if (draftAgent) {
            setDraftAgent((current) => current ? {
                ...current,
                env: nextEntries.map(([key, value]) => ({ key, value })),
            } : current);
            return;
        }
        updateActiveAgent((agent) => ({ ...agent, env: Object.fromEntries(nextEntries) }));
    };

    const validateSnapshot = ({ agentDefinitionsChanged, builtinValidationRequired }: { agentDefinitionsChanged: boolean; builtinValidationRequired: boolean }): string | null => {
        if (!snapshot || (!editingAgent && !isBuiltinSelected)) return t('settingsNotLoaded');
        if (pendingPrompt) return t('agentBusy');
        if (builtinValidationRequired) {
            if (!snapshot.builtinAgent.provider.trim()) return t('providerRequired');
            if (!snapshot.builtinAgent.model.trim()) return t('modelRequired');
            if (!snapshot.builtinAgent.baseUrl.trim()) return t('baseUrlRequired');
        }
        if (!agentDefinitionsChanged) return null;
        if (!editingAgent) return t('settingsNotLoaded');
        if (!editingAgent.command.trim()) return t('commandRequired');
        if (editingAgent.args.some((arg) => !String(arg).trim())) return t('argsRequired');
        if (draftAgent) {
            if (!draftAgent.name.trim()) return t('newAgentNameRequired');
            if (snapshot.agentServers.some((agent) => agent.name === draftAgent.name.trim()) || draftAgent.name.trim() === 'msinsight-native') {
                return t('agentNameUnique');
            }
            if (draftAgent.env.some((entry) => !entry.key.trim())) return t('envKeysRequired');
        } else if (Object.keys(activeAgent?.env ?? {}).some((key) => !String(key).trim())) {
            return t('envKeysRequired');
        }
        return null;
    };

    const buildSavePayload = (): AgentConfigSnapshot | null => {
        if (!snapshot) return null;
        const normalizedActive = snapshot.agentServers.map((agent) => ({
            ...agent,
            args: agent.args.map((arg) => String(arg).trim()),
            env: Object.fromEntries(Object.entries(agent.env).map(([key, value]) => [String(key).trim(), String(value)])),
        }));
        let activeAgentName = saveAndSwitchSelected && selectedAgentName ? selectedAgentName : snapshot.activeAgentName;
        const agentServers = [...normalizedActive];
        if (draftAgent) {
            const newAgent = {
                name: draftAgent.name.trim(),
                command: draftAgent.command.trim(),
                args: draftAgent.args.map((arg) => String(arg).trim()),
                env: Object.fromEntries(draftAgent.env.filter((entry) => entry.key.trim()).map((entry) => [entry.key.trim(), String(entry.value)])),
            };
            agentServers.push(newAgent);
            if (draftAgent.saveAndSwitch) activeAgentName = newAgent.name;
        }
        return {
            ...snapshot,
            activeAgentName,
            agentServers,
            sessionConfig: {
                ...snapshot.sessionConfig,
                defaultAllowlist: {
                    ...snapshot.sessionConfig.defaultAllowlist,
                    extraPaths: snapshot.sessionConfig.defaultAllowlist.extraPaths.map((path) => String(path).trim()).filter(Boolean),
                },
            },
        };
    };

    const handleSave = async (): Promise<void> => {
        if (configMode === 'script' && scriptError) {
            setError(scriptError);
            return;
        }
        const payload = buildSavePayload();
        const initialSnapshot = initialSnapshotRef.current;
        if (!payload || !initialSnapshot) return;
        const agentServersChanged = JSON.stringify({
            activeAgentName: payload.activeAgentName,
            agentServers: payload.agentServers,
        }) !== JSON.stringify({
            activeAgentName: initialSnapshot.activeAgentName,
            agentServers: initialSnapshot.agentServers,
        });
        const builtinAgentChanged = JSON.stringify(payload.builtinAgent) !== JSON.stringify(initialSnapshot.builtinAgent);
        const agentDefinitionsChanged = JSON.stringify(payload.agentServers) !== JSON.stringify(initialSnapshot.agentServers);
        const sessionConfigChanged = JSON.stringify(payload.sessionConfig) !== JSON.stringify(initialSnapshot.sessionConfig);
        const builtinValidationRequired = builtinAgentChanged
            || (payload.activeAgentName === 'msinsight-native' && payload.activeAgentName !== initialSnapshot.activeAgentName);
        const validationError = validateSnapshot({ agentDefinitionsChanged, builtinValidationRequired });
        if (validationError) {
            setError(validationError);
            return;
        }
        setSaving(true);
        setError(null);
        try {
            let savedSnapshot = payload;
            if (builtinAgentChanged) {
                const result = await saveBuiltinAgentConfig(payload.builtinAgent);
                savedSnapshot = result.snapshot ?? savedSnapshot;
            }
            if (sessionConfigChanged) {
                const result = await saveAgentSessionConfig(payload.sessionConfig);
                savedSnapshot = result.snapshot ?? savedSnapshot;
            }
            if (agentServersChanged) {
                const result = await saveAgentServersConfig({
                    activeAgentName: payload.activeAgentName,
                    agentServers: payload.agentServers,
                });
                savedSnapshot = result.snapshot ?? savedSnapshot;
            }
            setSnapshot(savedSnapshot);
            initialSnapshotRef.current = savedSnapshot;
            await applyAgentConfigSnapshot(savedSnapshot);
            setDraftAgent(null);
            setOpen(false);
            message.success(t('settingsSaved'));
        } catch (nextError) {
            if (isBackendUnavailableError(nextError)) {
                return;
            }
            const errorMessage = nextError instanceof Error ? nextError.message : String(nextError);
            setError(errorMessage);
            message.error(errorMessage);
        } finally {
            setSaving(false);
        }
    };

    return (
        <Container>
            {trigger ? <span className="settings-trigger" onClick={() => setOpen(true)}>{trigger}</span> : null}
            <Drawer
                className={`settings-drawer${panelScrollState.hasContentAbove ? ' content-scrolled' : ''}`}
                closable={false}
                getContainer={false}
                mask
                maskClosable
                onClose={() => setOpen(false)}
                open={open}
                placement="right"
                title={(
                    <div className="settings-header">
                        <button aria-label={t('back')} className="settings-back" onClick={() => setOpen(false)} type="button">
                            <img alt="" src={backIcon} />
                        </button>
                        <span>{t('agentRuntimeSettings')}</span>
                    </div>
                )}
                width="100%"
            >
                <div className="settings-layout">
                    <div className="panel" onScroll={(event) => updatePanelScrollState(event.currentTarget)} ref={panelRef}>
                    {loading ? <div className="hint">{t('loadingSettings')}</div> : null}
                    {pendingPrompt ? (
                        <div className="warning" role="status">
                            {t('agentBusy')}
                        </div>
                    ) : null}
                    {error ? <div className="error">{error}</div> : null}
                    {snapshot && (editingAgent || isBuiltinSelected) ? (
                        <>
                            <div className="section">
                                <div aria-label={t('agentToEdit')} className="agent-grid" role="group">
                                    <button
                                        aria-pressed={isBuiltinSelected}
                                        className={`agent-card${isBuiltinSelected ? ' selected' : ''}`}
                                        onClick={() => selectAgent('msinsight-native')}
                                        title={t('builtinAgentCardName')}
                                        type="button"
                                    >
                                        <span className="agent-card-icon logo"><img alt="" src={agentLogo} /></span>
                                        <span className="agent-card-name">{t('builtinAgentCardName')}</span>
                                    </button>
                                    {snapshot.agentServers.map((agent) => {
                                        const selected = !isCreatingAgent && selectedAgentName === agent.name;
                                        return <button
                                            aria-pressed={selected}
                                            className={`agent-card${selected ? ' selected' : ''}`}
                                            key={agent.name}
                                            onClick={() => selectAgent(agent.name)}
                                            title={agent.name}
                                            type="button"
                                        >
                                            <span className="agent-card-icon">{agent.name.trim().charAt(0) || 'A'}</span>
                                            <span className="agent-card-name">{agent.name}</span>
                                        </button>;
                                    })}
                                    {isCreatingAgent ? <button aria-pressed="true" className="agent-card selected" type="button">
                                        <span className="agent-card-icon logo"><img alt="" src={agentLogo} /></span>
                                        <span className="agent-card-name">{draftAgent?.name.trim() || t('newAgent')}</span>
                                    </button> : null}
                                    <button className="agent-card add-card" onClick={createAgent} type="button">
                                        <span aria-hidden="true" className="add-icon" />
                                        <span className="agent-card-name">{t('addAgent')}</span>
                                    </button>
                                </div>
                                {isBuiltinSelected ? <Alert message={t('builtinAgentHint')} /> : null}
                                <div aria-label={t('configMode')} className="config-tabs" role="tablist">
                                    <button
                                        aria-selected={configMode === 'form'}
                                        className={`config-tab${configMode === 'form' ? ' active' : ''}`}
                                        onClick={() => switchConfigMode('form')}
                                        role="tab"
                                        type="button"
                                    >
                                        {t('formConfig')}
                                    </button>
                                    <button
                                        aria-selected={configMode === 'script'}
                                        className={`config-tab${configMode === 'script' ? ' active' : ''}`}
                                        onClick={() => switchConfigMode('script')}
                                        role="tab"
                                        type="button"
                                    >
                                        {t('scriptConfig')}
                                    </button>
                                </div>
                                {configMode === 'form' ? <>
                                {isCreatingAgent ? (
                                    <div className="row">
                                        <label htmlFor="new-agent-name">{t('newAgentName')}</label>
                                        <Input className="settings-input" id="new-agent-name" onChange={(event) => setDraftAgent((current) => current ? { ...current, name: event.target.value } : current)} type="text" value={draftAgent?.name ?? ''} />
                                    </div>
                                ) : <div className="row">
                                    <label htmlFor="agent-name">{t('agentName')}</label>
                                    <Input className="settings-input" id="agent-name" readOnly type="text" value={isBuiltinSelected ? t('builtinAgentCardName') : activeAgent?.name ?? ''} />
                                </div>}
                                {isBuiltinSelected ? <>
                                <div className="row">
                                    <label htmlFor="builtin-provider">{t('provider')}</label>
                                    <Input className="settings-input" id="builtin-provider" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, provider: event.target.value } }) : current)} type="text" value={snapshot.builtinAgent.provider} />
                                </div>
                                <div className="row">
                                    <label htmlFor="builtin-model">{t('model')}</label>
                                    <Input className="settings-input" id="builtin-model" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, model: event.target.value } }) : current)} type="text" value={snapshot.builtinAgent.model} />
                                </div>
                                <div className="row">
                                    <label htmlFor="builtin-base-url">{t('baseUrl')}</label>
                                    <Input className="settings-input" id="builtin-base-url" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, baseUrl: event.target.value } }) : current)} type="text" value={snapshot.builtinAgent.baseUrl} />
                                </div>
                                <div className="row">
                                    <label htmlFor="builtin-api-key">{t('apiKey')}</label>
                                    <PasswordInput className="settings-password" id="builtin-api-key" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, apiKey: event.target.value } }) : current)} value={snapshot.builtinAgent.apiKey} />
                                </div>
                                </> : editingAgent ? <>
                                <div className="row">
                                    <label htmlFor="agent-command">{t('command')}</label>
                                    <Input className="settings-input" id="agent-command" onChange={(event) => updateEditingAgent((agent) => ({ ...agent, command: event.target.value }))} type="text" value={editingAgent.command} />
                                </div>
                                <div className="row">
                                    <div className="extra-path-header">
                                        <label>{t('args')}</label>
                                        <button className="path-add" onClick={() => updateEditingAgent((agent) => ({ ...agent, args: [...agent.args, ''] }))} type="button">
                                            <span aria-hidden="true" className="path-add-icon" />
                                            <span>{t('addArg')}</span>
                                        </button>
                                    </div>
                                    {!editingAgent.args.length ? <div className="path-empty">{t('noArgs')}</div> : null}
                                    {editingAgent.args.map((arg, index) => (
                                        <div className="path-row" key={`arg-${index}`}>
                                            <Input
                                                aria-label={t('argLabel', { index: index + 1 })}
                                                className="settings-input"
                                                onChange={(event) => updateEditingAgent((agent) => ({
                                                    ...agent,
                                                    args: agent.args.map((item, itemIndex) => itemIndex === index ? event.target.value : item),
                                                }))}
                                                type="text"
                                                value={arg}
                                            />
                                            <button aria-label={t('removeArg', { index: index + 1 })} className="path-remove" onClick={() => updateEditingAgent((agent) => ({ ...agent, args: agent.args.filter((_, itemIndex) => itemIndex !== index) }))} title={t('removeArg', { index: index + 1 })} type="button">
                                                <span aria-hidden="true" className="path-remove-icon" />
                                            </button>
                                        </div>
                                    ))}
                                </div>
                                <div className="row">
                                    <div className="extra-path-header">
                                        <label>{t('env')}</label>
                                        <button className="path-add" onClick={() => updateEditingEnv([...envEntries, ['', '']])} type="button">
                                            <span aria-hidden="true" className="path-add-icon" />
                                            <span>{t('addEnvEntry')}</span>
                                        </button>
                                    </div>
                                    {!envEntries.length ? <div className="path-empty">{t('noEnvEntries')}</div> : null}
                                    {envEntries.map(([key, value], index) => (
                                        <div className="env-row" key={`env-${index}`}>
                                            <Input
                                                aria-label={t('envKeyLabel', { index: index + 1 })}
                                                className="settings-input"
                                                onChange={(event) => updateEditingEnv(envEntries.map((entry, currentIndex) => [currentIndex === index ? event.target.value : entry[0], entry[1]]))}
                                                placeholder={t('envKeyPlaceholder')}
                                                type="text"
                                                value={key}
                                            />
                                            <Input
                                                aria-label={t('envValueLabel', { index: index + 1 })}
                                                className="settings-input"
                                                onChange={(event) => updateEditingEnv(envEntries.map((entry, currentIndex) => currentIndex === index ? [entry[0], event.target.value] : entry))}
                                                placeholder={t('envValuePlaceholder')}
                                                type="text"
                                                value={value}
                                            />
                                            <button aria-label={t('removeEnv', { index: index + 1 })} className="path-remove" onClick={() => updateEditingEnv(envEntries.filter((_, currentIndex) => currentIndex !== index))} title={t('removeEnv', { index: index + 1 })} type="button">
                                                <span aria-hidden="true" className="path-remove-icon" />
                                            </button>
                                        </div>
                                    ))}
                                </div>
                                </> : null}
                                </> : <div className="script-config-panel" role="tabpanel">
                                    <div className="script-editor-shell">
                                        <div className="script-config-toolbar">
                                            <span className="script-config-label">JSON</span>
                                            <div className="script-config-actions">
                                                <button className="script-format-button" onClick={formatScriptValue} type="button">{t('formatJson')}</button>
                                                <button
                                                    aria-label={t('copyJson')}
                                                    className="script-copy-button"
                                                    onClick={() => {
                                                        copyScriptValue();
                                                    }}
                                                    title={t('copyJson')}
                                                    type="button"
                                                >
                                                    <img alt="" src={copyIcon} />
                                                </button>
                                            </div>
                                        </div>
                                        <Suspense fallback={<div className="hint">{t('loadingEditor')}</div>}>
                                            <JsonEditor ariaLabel={t('scriptConfig')} onChange={applyScriptValue} value={scriptValue} />
                                        </Suspense>
                                    </div>
                                    {scriptError ? <div className="script-error" role="alert">{scriptError}</div> : null}
                                </div>}
                            </div>
                            <div className="section session-section">
                                <button aria-expanded={showAdvanced} className="session-toggle" onClick={() => setShowAdvanced((current) => !current)} type="button">
                                    <img alt="" className="session-chevron" src={arrowDownIcon} />
                                    <span>{t('sessionConfig')}</span>
                                </button>
                                {showAdvanced ? <div className="session-content">
                                <div className="row">
                                    <div className="row">
                                        <label htmlFor="request-timeout">{t('requestTimeout')}</label>
                                        <div className="number-field">
                                            <InputNumber className="session-number" id="request-timeout" min={0} onChange={(value) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, requestTimeoutMs: Number(value ?? 0) } }) : current)} step={1000} value={snapshot.sessionConfig.requestTimeoutMs} />
                                            <span aria-hidden="true" className="number-unit">ms</span>
                                        </div>
                                    </div>
                                    <div className="row">
                                        <label htmlFor="prompt-timeout">{t('promptTimeout')}</label>
                                        <div className="number-field">
                                            <InputNumber className="session-number" id="prompt-timeout" min={0} onChange={(value) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, promptRequestTimeoutMs: Number(value ?? 0) } }) : current)} step={1000} value={snapshot.sessionConfig.promptRequestTimeoutMs} />
                                            <span aria-hidden="true" className="number-unit">ms</span>
                                        </div>
                                    </div>
                                </div>
                                <div className="row">
                                    <label htmlFor="permission-timeout">{t('permissionTimeout')}</label>
                                    <div className="number-field">
                                        <InputNumber className="session-number" id="permission-timeout" min={0} onChange={(value) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, permissionRequestTimeoutMs: Number(value ?? 0) } }) : current)} step={1000} value={snapshot.sessionConfig.permissionRequestTimeoutMs} />
                                        <span aria-hidden="true" className="number-unit">ms</span>
                                    </div>
                                </div>
                                <div className="config-group-title">{t('rootContentConfig')}</div>
                                <label className="check-row">
                                    <input checked={snapshot.sessionConfig.defaultAllowlist.includeDocsRoot} onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, defaultAllowlist: { ...current.sessionConfig.defaultAllowlist, includeDocsRoot: event.target.checked } } }) : current)} type="checkbox" />
                                    <span>{t('includeDocsRoot')}</span>
                                </label>
                                <label className="check-row">
                                    <input checked={snapshot.sessionConfig.defaultAllowlist.includeAgentWorkspaceRoot} onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, defaultAllowlist: { ...current.sessionConfig.defaultAllowlist, includeAgentWorkspaceRoot: event.target.checked } } }) : current)} type="checkbox" />
                                    <span>{t('includeAgentWorkspaceRoot')}</span>
                                </label>
                                <label className="check-row">
                                    <input checked={snapshot.sessionConfig.defaultAllowlist.includeProjectRoot} onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, defaultAllowlist: { ...current.sessionConfig.defaultAllowlist, includeProjectRoot: event.target.checked } } }) : current)} type="checkbox" />
                                    <span>{t('includeProjectRoot')}</span>
                                </label>
                                <div className="row extra-path-section">
                                    <div className="extra-path-header">
                                        <label>{t('extraAllowlistPaths')}</label>
                                        <button className="path-add" onClick={() => setSnapshot((current) => current ? ({
                                            ...current,
                                            sessionConfig: {
                                                ...current.sessionConfig,
                                                defaultAllowlist: {
                                                    ...current.sessionConfig.defaultAllowlist,
                                                    extraPaths: [...extraPaths, ''],
                                                },
                                            },
                                        }) : current)} type="button">
                                            <span aria-hidden="true" className="path-add-icon" />
                                            <span>{t('addPath')}</span>
                                        </button>
                                    </div>
                                    {!extraPaths.length ? <div className="path-empty">{t('noExtraPaths')}</div> : null}
                                    {extraPaths.map((path, index) => (
                                        <div className="path-row" key={`extra-path-${index}`}>
                                            <Input
                                                aria-label={`${t('extraAllowlistPaths')} ${index + 1}`}
                                                className="settings-input"
                                                onChange={(event) => setSnapshot((current) => current ? ({
                                                    ...current,
                                                    sessionConfig: {
                                                        ...current.sessionConfig,
                                                        defaultAllowlist: {
                                                            ...current.sessionConfig.defaultAllowlist,
                                                            extraPaths: extraPaths.map((item, itemIndex) => itemIndex === index ? event.target.value : item),
                                                        },
                                                    },
                                                }) : current)}
                                                type="text"
                                                value={path}
                                            />
                                            <button aria-label={t('removePath', { index: index + 1 })} className="path-remove" onClick={() => setSnapshot((current) => current ? ({
                                                ...current,
                                                sessionConfig: {
                                                    ...current.sessionConfig,
                                                    defaultAllowlist: {
                                                        ...current.sessionConfig.defaultAllowlist,
                                                        extraPaths: extraPaths.filter((_, itemIndex) => itemIndex !== index),
                                                    },
                                                },
                                            }) : current)} title={t('removePath', { index: index + 1 })} type="button">
                                                <span aria-hidden="true" className="path-remove-icon" />
                                            </button>
                                        </div>
                                    ))}
                                </div>
                                </div> : null}
                            </div>
                        </>
                    ) : null}
                    </div>
                    {snapshot && (editingAgent || isBuiltinSelected) ? <div className={`settings-footer${panelScrollState.hasContentBelow ? ' has-content-below' : ''}`}>
                        {isCreatingAgent ? <label className="check-row">
                            <input checked={draftAgent?.saveAndSwitch ?? false} onChange={(event) => setDraftAgent((current) => current ? { ...current, saveAndSwitch: event.target.checked } : current)} type="checkbox" />
                            <span>{t('saveAndSwitchThisAgent')}</span>
                        </label> : <label className="check-row">
                            <input checked={saveAndSwitchSelected} onChange={(event) => setSaveAndSwitchSelected(event.target.checked)} type="checkbox" />
                            <span>{t('saveAndSwitchSelectedAgent')}</span>
                        </label>}
                        <div className="settings-footer-actions">
                            <Button onClick={() => setOpen(false)} size="small" type="default">{t('cancel')}</Button>
                            <Button disabled={pendingPrompt || saving} onClick={() => { handleSave(); }} size="small" type="primary">{saving ? t('saving') : t('save')}</Button>
                        </div>
                    </div> : null}
                </div>
            </Drawer>
        </Container>
    );
};
