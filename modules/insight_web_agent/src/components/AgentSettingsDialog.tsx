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
import { useEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Button, Select } from '@insight/lib/components';
import { fetchAgentConfig, saveAgentServersConfig, saveAgentSessionConfig, saveBuiltinAgentConfig } from '../api';
import { useChatState } from '../hooks/useChatState';
import type { AgentConfigSnapshot } from '../types';
import backIcon from '../icons/back.svg';

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
        z-index: 1200 !important;
    }

    .settings-drawer .ant-drawer-header {
        padding: 14px 16px;
        border-bottom: 0;
        background: ${(props): string => props.theme.bgColor};
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
        padding: 12px 14px 16px;
        background: ${(props): string => props.theme.bgColor};
    }

    .panel {
        display: grid;
        gap: 14px;
    }

    .section {
        display: grid;
        gap: 10px;
        padding: 12px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        background: ${(props): string => props.theme.bgColor};
    }

    .section-title {
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        font-weight: 700;
        letter-spacing: 0.04em;
        text-transform: uppercase;
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
        font-size: 12px;
        font-weight: 600;
    }

    input[type='text'],
    input[type='password'],
    input[type='number'] {
        width: 100%;
        height: 32px;
        min-width: 0;
        box-sizing: border-box;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 0 10px;
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
        outline: none;
        transition: border-color 0.15s ease, box-shadow 0.15s ease, background 0.15s ease;
    }

    input[type='text']:not(:disabled):not([readonly]):hover,
    input[type='password']:not(:disabled):not([readonly]):hover,
    input[type='number']:not(:disabled):not([readonly]):hover {
        border-color: ${(props): string => props.theme.primaryColor};
    }

    input[type='text']:not(:disabled):not([readonly]):focus,
    input[type='password']:not(:disabled):not([readonly]):focus,
    input[type='number']:not(:disabled):not([readonly]):focus {
        border-color: ${(props): string => props.theme.primaryColor};
        box-shadow: 0 0 0 2px ${(props): string => `${props.theme.primaryColor}33`};
    }

    input[readonly],
    input:disabled {
        background: ${(props): string => props.theme.bgColorLight};
        color: ${(props): string => props.theme.textColorSecondary};
        cursor: not-allowed;
        opacity: 0.72;
    }

    .inline-actions,
    .footer-actions,
    .check-row {
        display: flex;
        align-items: center;
        gap: 8px;
        flex-wrap: wrap;
    }

    .footer-actions {
        justify-content: flex-end;
    }

    .section-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
    }

    .advanced-toggle {
        border: 0;
        padding: 0;
        background: transparent;
        color: ${(props): string => props.theme.primaryColor};
        font-size: 12px;
        cursor: pointer;
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
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }

    .toggle-button:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }
`;

interface AgentSettingsDialogProps {
    trigger: React.ReactNode;
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

export const AgentSettingsDialog = ({ trigger }: AgentSettingsDialogProps): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const { applyAgentConfigSnapshot, pendingPrompt } = useChatState();
    const [open, setOpen] = useState(false);
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [snapshot, setSnapshot] = useState<AgentConfigSnapshot | null>(null);
    const initialSnapshotRef = useRef<AgentConfigSnapshot | null>(null);
    const [selectedAgentName, setSelectedAgentName] = useState<string | null>(null);
    const [saveAndSwitchSelected, setSaveAndSwitchSelected] = useState(false);
    const [draftAgent, setDraftAgent] = useState<DraftAgent | null>(null);
    const [showAdvanced, setShowAdvanced] = useState(false);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (!open) return;
        setLoading(true);
        setError(null);
        setSaveAndSwitchSelected(false);
        setDraftAgent(null);
        setShowAdvanced(false);
        const loadSettings = async (): Promise<void> => {
            try {
                const nextSnapshot = await fetchAgentConfig();
                setSnapshot(nextSnapshot);
                initialSnapshotRef.current = nextSnapshot;
                const editableActiveAgent = nextSnapshot.activeAgentName === 'msinsight-native'
                    || nextSnapshot.agentServers.some((agent) => agent.name === nextSnapshot.activeAgentName);
                setSelectedAgentName(editableActiveAgent ? nextSnapshot.activeAgentName : 'msinsight-native');
            } catch (nextError) {
                setError(nextError instanceof Error ? nextError.message : String(nextError));
            } finally {
                setLoading(false);
            }
        };
        void loadSettings();
    }, [open]);

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

    const extraPaths = snapshot?.sessionConfig.defaultAllowlist.extraPaths?.length
        ? snapshot.sessionConfig.defaultAllowlist.extraPaths
        : [''];

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
            const errorMessage = nextError instanceof Error ? nextError.message : String(nextError);
            setError(errorMessage);
            message.error(errorMessage);
        } finally {
            setSaving(false);
        }
    };

    return (
        <Container>
            <span className="settings-trigger" onClick={() => setOpen(true)}>{trigger}</span>
            <Drawer
                className="settings-drawer"
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
                zIndex={1200}
            >
                <div className="panel">
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
                                <div className="section-header">
                                    <div className="section-title">{t('agentSection')}</div>
                                    {isCreatingAgent
                                        ? <Button onClick={() => setDraftAgent(null)} size="small" type="default">{t('cancelDraft')}</Button>
                                        : <Button onClick={() => setDraftAgent(EMPTY_DRAFT())} size="small" type="default">{t('addAgent')}</Button>}
                                </div>
                                {isCreatingAgent ? (
                                    <div className="row">
                                        <label htmlFor="new-agent-name">{t('newAgentName')}</label>
                                        <input id="new-agent-name" onChange={(event) => setDraftAgent((current) => current ? { ...current, name: event.target.value } : current)} type="text" value={draftAgent?.name ?? ''} />
                                    </div>
                                ) : (
                                    <div className="row">
                                        <label htmlFor="agent-selector">{t('agentToEdit')}</label>
                                        <Select
                                            aria-label={t('agentToEdit')}
                                            id="agent-selector"
                                            onChange={(value) => setSelectedAgentName(String(value))}
                                            options={[
                                                { label: t('builtinAgentName'), value: 'msinsight-native' },
                                                ...snapshot.agentServers.map((agent) => ({ label: agent.name, value: agent.name })),
                                            ]}
                                            value={selectedAgentName ?? snapshot.activeAgentName}
                                            width="100%"
                                        />
                                    </div>
                                )}
                                {isBuiltinSelected ? <>
                                <div className="hint">{t('builtinAgentHint')}</div>
                                <div className="row">
                                    <label htmlFor="builtin-provider">{t('provider')}</label>
                                    <input id="builtin-provider" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, provider: event.target.value } }) : current)} type="text" value={snapshot.builtinAgent.provider} />
                                </div>
                                <div className="row">
                                    <label htmlFor="builtin-model">{t('model')}</label>
                                    <input id="builtin-model" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, model: event.target.value } }) : current)} type="text" value={snapshot.builtinAgent.model} />
                                </div>
                                <div className="row">
                                    <label htmlFor="builtin-base-url">{t('baseUrl')}</label>
                                    <input id="builtin-base-url" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, baseUrl: event.target.value } }) : current)} type="text" value={snapshot.builtinAgent.baseUrl} />
                                </div>
                                <div className="row">
                                    <label htmlFor="builtin-api-key">{t('apiKey')}</label>
                                    <input id="builtin-api-key" onChange={(event) => setSnapshot((current) => current ? ({ ...current, builtinAgent: { ...current.builtinAgent, apiKey: event.target.value } }) : current)} type="password" value={snapshot.builtinAgent.apiKey} />
                                </div>
                                </> : editingAgent ? <>
                                <div className="row">
                                    <label htmlFor="agent-command">{t('command')}</label>
                                    <input id="agent-command" onChange={(event) => updateEditingAgent((agent) => ({ ...agent, command: event.target.value }))} type="text" value={editingAgent.command} />
                                </div>
                                <div className="row">
                                    <div className="inline-actions">
                                        <label>{t('args')}</label>
                                        <Button onClick={() => updateEditingAgent((agent) => ({ ...agent, args: [...agent.args, ''] }))} size="small" type="default">{t('addArg')}</Button>
                                    </div>
                                    {editingAgent.args.map((arg, index) => (
                                        <div className="array-row" key={`arg-${index}`}>
                                            <input
                                                aria-label={t('argLabel', { index: index + 1 })}
                                                onChange={(event) => updateEditingAgent((agent) => ({
                                                    ...agent,
                                                    args: agent.args.map((item, itemIndex) => itemIndex === index ? event.target.value : item),
                                                }))}
                                                type="text"
                                                value={arg}
                                            />
                                            <Button onClick={() => updateEditingAgent((agent) => ({ ...agent, args: agent.args.filter((_, itemIndex) => itemIndex !== index) }))} size="small" type="default">{t('removeArg', { index: index + 1 })}</Button>
                                        </div>
                                    ))}
                                </div>
                                <div className="row">
                                    <div className="inline-actions">
                                        <label>{t('env')}</label>
                                        <Button onClick={() => updateEditingEnv([...envEntries, ['', '']])} size="small" type="default">{t('addEnvEntry')}</Button>
                                    </div>
                                    {envEntries.map(([key, value], index) => (
                                        <div className="kv-row" key={`env-${index}`}>
                                            <input
                                                aria-label={t('envKeyLabel', { index: index + 1 })}
                                                onChange={(event) => updateEditingEnv(envEntries.map((entry, currentIndex) => [currentIndex === index ? event.target.value : entry[0], entry[1]]))}
                                                type="text"
                                                value={key}
                                            />
                                            <input
                                                aria-label={t('envValueLabel', { index: index + 1 })}
                                                onChange={(event) => updateEditingEnv(envEntries.map((entry, currentIndex) => currentIndex === index ? [entry[0], event.target.value] : entry))}
                                                type="text"
                                                value={value}
                                            />
                                            <Button onClick={() => updateEditingEnv(envEntries.filter((_, currentIndex) => currentIndex !== index))} size="small" type="default">{t('removeEnv', { index: index + 1 })}</Button>
                                        </div>
                                    ))}
                                </div>
                                </> : null}
                                {isCreatingAgent ? <label className="check-row">
                                    <input checked={draftAgent?.saveAndSwitch ?? false} onChange={(event) => setDraftAgent((current) => current ? { ...current, saveAndSwitch: event.target.checked } : current)} type="checkbox" />
                                    <span>{t('saveAndSwitchThisAgent')}</span>
                                </label> : <label className="check-row">
                                    <input checked={saveAndSwitchSelected} onChange={(event) => setSaveAndSwitchSelected(event.target.checked)} type="checkbox" />
                                    <span>{t('saveAndSwitchSelectedAgent')}</span>
                                </label>}
                            </div>
                            <div className="section">
                                <div className="section-header">
                                    <div className="section-title">{t('sessionConfig')}</div>
                                    <button className="advanced-toggle" onClick={() => setShowAdvanced((current) => !current)} type="button">
                                        {showAdvanced ? t('collapse') : t('expand')}
                                    </button>
                                </div>
                                {showAdvanced ? <>
                                <div className="kv-row">
                                    <div className="row">
                                        <label htmlFor="request-timeout">{t('requestTimeout')}</label>
                                        <input id="request-timeout" onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, requestTimeoutMs: Number(event.target.value) } }) : current)} type="number" value={snapshot.sessionConfig.requestTimeoutMs} />
                                    </div>
                                    <div className="row">
                                        <label htmlFor="prompt-timeout">{t('promptTimeout')}</label>
                                        <input id="prompt-timeout" onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, promptRequestTimeoutMs: Number(event.target.value) } }) : current)} type="number" value={snapshot.sessionConfig.promptRequestTimeoutMs} />
                                    </div>
                                </div>
                                <div className="row">
                                    <label htmlFor="permission-timeout">{t('permissionTimeout')}</label>
                                    <input id="permission-timeout" onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, permissionRequestTimeoutMs: Number(event.target.value) } }) : current)} type="number" value={snapshot.sessionConfig.permissionRequestTimeoutMs} />
                                </div>
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
                                <div className="row">
                                    <div className="inline-actions">
                                        <label>{t('extraAllowlistPaths')}</label>
                                        <Button onClick={() => setSnapshot((current) => current ? ({
                                            ...current,
                                            sessionConfig: {
                                                ...current.sessionConfig,
                                                defaultAllowlist: {
                                                    ...current.sessionConfig.defaultAllowlist,
                                                    extraPaths: [...extraPaths, ''],
                                                },
                                            },
                                        }) : current)} size="small" type="default">{t('addPath')}</Button>
                                    </div>
                                    {extraPaths.map((path, index) => (
                                        <div className="array-row" key={`extra-path-${index}`}>
                                            <input
                                                aria-label={`${t('extraAllowlistPaths')} ${index + 1}`}
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
                                            <Button onClick={() => setSnapshot((current) => current ? ({
                                                ...current,
                                                sessionConfig: {
                                                    ...current.sessionConfig,
                                                    defaultAllowlist: {
                                                        ...current.sessionConfig.defaultAllowlist,
                                                        extraPaths: extraPaths.filter((_, itemIndex) => itemIndex !== index),
                                                    },
                                                },
                                            }) : current)} size="small" type="default">{t('removePath', { index: index + 1 })}</Button>
                                        </div>
                                    ))}
                                    <div className="warning">{t('extraPathsHint')}</div>
                                </div>
                                </> : null}
                            </div>
                            <div className="footer-actions">
                                <Button onClick={() => setOpen(false)} size="small" type="default">{t('cancel')}</Button>
                                <Button disabled={pendingPrompt || saving} onClick={() => { handleSave(); }} size="small" type="primary">{saving ? t('saving') : t('save')}</Button>
                            </div>
                        </>
                    ) : null}
                </div>
            </Drawer>
        </Container>
    );
};
