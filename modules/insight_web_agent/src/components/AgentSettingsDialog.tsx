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
import { useEffect, useMemo, useState } from 'react';
import { Button, Select } from '@insight/lib/components';
import { fetchAgentConfig, saveAgentConfig } from '../api';
import { useChatState } from '../hooks/useChatState';
import type { AgentConfigSnapshot } from '../types';

const Container = styled.div`
    .settings-drawer .ant-drawer-content {
        background: ${(props): string => props.theme.bgColorLight};
    }

    .settings-drawer .ant-drawer-header {
        padding: 14px 16px;
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
        background: ${(props): string => props.theme.bgColorLight};
    }

    .settings-drawer .ant-drawer-title,
    .settings-drawer .ant-drawer-close {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .settings-drawer .ant-drawer-body {
        padding: 12px 14px 16px;
        background: ${(props): string => props.theme.bgColorLight};
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
    input[type='number'] {
        width: 100%;
        min-width: 0;
        box-sizing: border-box;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 8px 10px;
        background: ${(props): string => props.theme.bgColorLight};
        color: ${(props): string => props.theme.textColorPrimary};
        outline: none;
    }

    input[readonly] {
        color: ${(props): string => props.theme.textColorSecondary};
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
    const { applyAgentConfigSnapshot, pendingPrompt } = useChatState();
    const [open, setOpen] = useState(false);
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [snapshot, setSnapshot] = useState<AgentConfigSnapshot | null>(null);
    const [selectedAgentName, setSelectedAgentName] = useState<string | null>(null);
    const [saveAndSwitchSelected, setSaveAndSwitchSelected] = useState(false);
    const [draftAgent, setDraftAgent] = useState<DraftAgent | null>(null);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (!open) return;
        setLoading(true);
        setError(null);
        setSaveAndSwitchSelected(false);
        void fetchAgentConfig()
            .then((nextSnapshot) => {
                setSnapshot(nextSnapshot);
                setSelectedAgentName(nextSnapshot.activeAgentName);
            })
            .catch((nextError) => setError(nextError instanceof Error ? nextError.message : String(nextError)))
            .finally(() => setLoading(false));
    }, [open]);

    const activeAgent = useMemo(() => {
        if (!snapshot) return undefined;
        return snapshot.agentServers.find((agent) => agent.name === (selectedAgentName ?? snapshot.activeAgentName)) ?? snapshot.agentServers[0];
    }, [selectedAgentName, snapshot]);

    const envEntries = useMemo(() => {
        if (!activeAgent) return [];
        const entries = Object.entries(activeAgent.env ?? {});
        return entries.length ? entries : [['', '']];
    }, [activeAgent]);

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

    const validateSnapshot = (): string | null => {
        if (!snapshot || !activeAgent) return 'Settings are not loaded yet.';
        if (pendingPrompt) return 'Agent is busy. Wait for the current prompt to finish before saving settings.';
        if (!activeAgent.command.trim()) return 'Command cannot be empty.';
        if (activeAgent.args.some((arg) => !String(arg).trim())) return 'Args cannot be empty.';
        if (Object.keys(activeAgent.env ?? {}).some((key) => !String(key).trim())) return 'Env keys cannot be empty.';
        if (draftAgent) {
            if (!draftAgent.name.trim()) return 'New agent name is required.';
            if (!draftAgent.command.trim()) return 'New agent command is required.';
            if (draftAgent.args.some((arg) => !String(arg).trim())) return 'Args cannot be empty.';
            if (draftAgent.env.some((entry) => !entry.key.trim())) return 'Env keys cannot be empty.';
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
        let activeAgentName = saveAndSwitchSelected && activeAgent ? activeAgent.name : snapshot.activeAgentName;
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
        const validationError = validateSnapshot();
        if (validationError) {
            setError(validationError);
            return;
        }
        const payload = buildSavePayload();
        if (!payload) return;
        setSaving(true);
        setError(null);
        try {
            const result = await saveAgentConfig(payload);
            if (result.snapshot) {
                setSnapshot(result.snapshot);
                applyAgentConfigSnapshot(result.snapshot);
            }
            setDraftAgent(null);
            setOpen(false);
            message.success('Agent settings saved.');
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
            <span onClick={() => setOpen(true)}>{trigger}</span>
            <Drawer
                className="settings-drawer"
                getContainer={false}
                mask
                maskClosable
                onClose={() => setOpen(false)}
                open={open}
                placement="right"
                title="Agent Runtime Settings"
                width={420}
            >
                <div className="panel">
                    {loading ? <div className="hint">Loading settings...</div> : null}
                    {pendingPrompt ? (
                        <div className="warning" role="status">
                            Agent is busy. Wait for the current prompt to finish before saving settings.
                        </div>
                    ) : null}
                    {error ? <div className="error">{error}</div> : null}
                    {snapshot && activeAgent ? (
                        <>
                            <div className="section">
                                <div className="section-title">Current Agent</div>
                                <div className="row">
                                    <label htmlFor="agent-selector">Agent to edit</label>
                                    <Select
                                        aria-label="Agent to edit"
                                        id="agent-selector"
                                        onChange={(value) => setSelectedAgentName(String(value))}
                                        options={snapshot.agentServers.map((agent) => ({ label: agent.name, value: agent.name }))}
                                        value={activeAgent.name}
                                        width="100%"
                                    />
                                </div>
                                <div className="row">
                                    <label htmlFor="agent-name">Agent name</label>
                                    <input id="agent-name" readOnly type="text" value={activeAgent.name} />
                                </div>
                                <div className="row">
                                    <label htmlFor="agent-command">Command</label>
                                    <input id="agent-command" onChange={(event) => updateActiveAgent((agent) => ({ ...agent, command: event.target.value }))} type="text" value={activeAgent.command} />
                                </div>
                                <div className="row">
                                    <div className="inline-actions">
                                        <label>Args</label>
                                        <Button onClick={() => updateActiveAgent((agent) => ({ ...agent, args: [...agent.args, ''] }))} size="small" type="default">Add arg</Button>
                                    </div>
                                    {activeAgent.args.map((arg, index) => (
                                        <div className="array-row" key={`arg-${index}`}>
                                            <input
                                                aria-label={`Arg ${index + 1}`}
                                                onChange={(event) => updateActiveAgent((agent) => ({
                                                    ...agent,
                                                    args: agent.args.map((item, itemIndex) => itemIndex === index ? event.target.value : item),
                                                }))}
                                                type="text"
                                                value={arg}
                                            />
                                            <Button onClick={() => updateActiveAgent((agent) => ({ ...agent, args: agent.args.filter((_, itemIndex) => itemIndex !== index) }))} size="small" type="default">Remove arg {index + 1}</Button>
                                        </div>
                                    ))}
                                </div>
                                <div className="row">
                                    <div className="inline-actions">
                                        <label>Env</label>
                                        <Button onClick={() => updateActiveAgent((agent) => ({ ...agent, env: Object.fromEntries([...envEntries, ['', '']]) }))} size="small" type="default">Add env entry</Button>
                                    </div>
                                    {envEntries.map(([key, value], index) => (
                                        <div className="kv-row" key={`env-${index}`}>
                                            <input
                                                aria-label={`Env key ${index + 1}`}
                                                onChange={(event) => updateActiveAgent((agent) => ({
                                                    ...agent,
                                                    env: Object.fromEntries(envEntries.map((entry, currentIndex) => [currentIndex === index ? event.target.value : entry[0], entry[1]]).filter((pair, currentIndex) => currentIndex === index || Boolean(pair[0]))),
                                                }))}
                                                type="text"
                                                value={key}
                                            />
                                            <input
                                                aria-label={`Env value ${index + 1}`}
                                                onChange={(event) => updateActiveAgent((agent) => ({
                                                    ...agent,
                                                    env: Object.fromEntries(envEntries.map((entry, currentIndex) => currentIndex === index ? [entry[0], event.target.value] : entry)),
                                                }))}
                                                type="text"
                                                value={value}
                                            />
                                            <Button onClick={() => updateActiveAgent((agent) => ({ ...agent, env: Object.fromEntries(envEntries.filter((_, currentIndex) => currentIndex !== index)) }))} size="small" type="default">Remove env {index + 1}</Button>
                                        </div>
                                    ))}
                                </div>
                                <label className="check-row">
                                    <input checked={saveAndSwitchSelected} onChange={(event) => setSaveAndSwitchSelected(event.target.checked)} type="checkbox" />
                                    <span>Save and switch to selected agent</span>
                                </label>
                            </div>
                            <div className="section">
                                <div className="inline-actions">
                                    <div className="section-title">New Agent</div>
                                    {!draftAgent ? <Button onClick={() => setDraftAgent(EMPTY_DRAFT())} size="small" type="default">Add agent</Button> : null}
                                    {draftAgent ? <Button onClick={() => setDraftAgent(null)} size="small" type="default">Cancel draft</Button> : null}
                                </div>
                                {draftAgent ? (
                                    <>
                                        <div className="row">
                                            <label htmlFor="new-agent-name">New agent name</label>
                                            <input id="new-agent-name" onChange={(event) => setDraftAgent((current) => current ? { ...current, name: event.target.value } : current)} type="text" value={draftAgent.name} />
                                        </div>
                                        <div className="row">
                                            <label htmlFor="new-agent-command">New agent command</label>
                                            <input id="new-agent-command" onChange={(event) => setDraftAgent((current) => current ? { ...current, command: event.target.value } : current)} type="text" value={draftAgent.command} />
                                        </div>
                                        <div className="row">
                                            <div className="inline-actions">
                                                <label>New agent args</label>
                                                <Button onClick={() => setDraftAgent((current) => current ? { ...current, args: [...current.args, ''] } : current)} size="small" type="default">Add new agent arg</Button>
                                            </div>
                                            {draftAgent.args.map((arg, index) => (
                                                <div className="array-row" key={`draft-arg-${index}`}>
                                                    <input
                                                        aria-label={`New agent arg ${index + 1}`}
                                                        onChange={(event) => setDraftAgent((current) => current ? {
                                                            ...current,
                                                            args: current.args.map((item, itemIndex) => itemIndex === index ? event.target.value : item),
                                                        } : current)}
                                                        type="text"
                                                        value={arg}
                                                    />
                                                    <Button onClick={() => setDraftAgent((current) => current ? { ...current, args: current.args.filter((_, itemIndex) => itemIndex !== index) } : current)} size="small" type="default">Remove new agent arg {index + 1}</Button>
                                                </div>
                                            ))}
                                        </div>
                                        <div className="row">
                                            <div className="inline-actions">
                                                <label>New agent env</label>
                                                <Button onClick={() => setDraftAgent((current) => current ? { ...current, env: [...current.env, { key: '', value: '' }] } : current)} size="small" type="default">Add new agent env entry</Button>
                                            </div>
                                            {draftAgent.env.map((entry, index) => (
                                                <div className="kv-row" key={`draft-env-${index}`}>
                                                    <input
                                                        aria-label={`New agent env key ${index + 1}`}
                                                        onChange={(event) => setDraftAgent((current) => current ? {
                                                            ...current,
                                                            env: current.env.map((item, itemIndex) => itemIndex === index ? { ...item, key: event.target.value } : item),
                                                        } : current)}
                                                        type="text"
                                                        value={entry.key}
                                                    />
                                                    <input
                                                        aria-label={`New agent env value ${index + 1}`}
                                                        onChange={(event) => setDraftAgent((current) => current ? {
                                                            ...current,
                                                            env: current.env.map((item, itemIndex) => itemIndex === index ? { ...item, value: event.target.value } : item),
                                                        } : current)}
                                                        type="text"
                                                        value={entry.value}
                                                    />
                                                    <Button onClick={() => setDraftAgent((current) => current ? { ...current, env: current.env.filter((_, itemIndex) => itemIndex !== index) } : current)} size="small" type="default">Remove new agent env {index + 1}</Button>
                                                </div>
                                            ))}
                                        </div>
                                        <label className="check-row">
                                            <input checked={draftAgent.saveAndSwitch} onChange={(event) => setDraftAgent((current) => current ? { ...current, saveAndSwitch: event.target.checked } : current)} type="checkbox" />
                                            <span>Save and switch to this agent</span>
                                        </label>
                                    </>
                                ) : <div className="hint">Create a new agent draft without renaming or deleting existing agents.</div>}
                            </div>
                            <div className="section">
                                <div className="section-title">Session Config</div>
                                <div className="kv-row">
                                    <div className="row">
                                        <label htmlFor="request-timeout">Request timeout</label>
                                        <input id="request-timeout" onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, requestTimeoutMs: Number(event.target.value) } }) : current)} type="number" value={snapshot.sessionConfig.requestTimeoutMs} />
                                    </div>
                                    <div className="row">
                                        <label htmlFor="prompt-timeout">Prompt timeout</label>
                                        <input id="prompt-timeout" onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, promptRequestTimeoutMs: Number(event.target.value) } }) : current)} type="number" value={snapshot.sessionConfig.promptRequestTimeoutMs} />
                                    </div>
                                </div>
                                <div className="row">
                                    <label htmlFor="permission-timeout">Permission timeout</label>
                                    <input id="permission-timeout" onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, permissionRequestTimeoutMs: Number(event.target.value) } }) : current)} type="number" value={snapshot.sessionConfig.permissionRequestTimeoutMs} />
                                </div>
                                <label className="check-row">
                                    <input checked={snapshot.sessionConfig.defaultAllowlist.includeDocsRoot} onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, defaultAllowlist: { ...current.sessionConfig.defaultAllowlist, includeDocsRoot: event.target.checked } } }) : current)} type="checkbox" />
                                    <span>Include docs root</span>
                                </label>
                                <label className="check-row">
                                    <input checked={snapshot.sessionConfig.defaultAllowlist.includeAgentWorkspaceRoot} onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, defaultAllowlist: { ...current.sessionConfig.defaultAllowlist, includeAgentWorkspaceRoot: event.target.checked } } }) : current)} type="checkbox" />
                                    <span>Include agent workspace root</span>
                                </label>
                                <label className="check-row">
                                    <input checked={snapshot.sessionConfig.defaultAllowlist.includeProjectRoot} onChange={(event) => setSnapshot((current) => current ? ({ ...current, sessionConfig: { ...current.sessionConfig, defaultAllowlist: { ...current.sessionConfig.defaultAllowlist, includeProjectRoot: event.target.checked } } }) : current)} type="checkbox" />
                                    <span>Include project root</span>
                                </label>
                                <div className="row">
                                    <div className="inline-actions">
                                        <label>Extra paths</label>
                                        <Button onClick={() => setSnapshot((current) => current ? ({
                                            ...current,
                                            sessionConfig: {
                                                ...current.sessionConfig,
                                                defaultAllowlist: {
                                                    ...current.sessionConfig.defaultAllowlist,
                                                    extraPaths: [...extraPaths, ''],
                                                },
                                            },
                                        }) : current)} size="small" type="default">Add extra path</Button>
                                    </div>
                                    {extraPaths.map((path, index) => (
                                        <div className="array-row" key={`extra-path-${index}`}>
                                            <input
                                                aria-label={`Extra path ${index + 1}`}
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
                                            }) : current)} size="small" type="default">Remove extra path {index + 1}</Button>
                                        </div>
                                    ))}
                                    <div className="warning">Extra paths may not exist yet; save will still be allowed.</div>
                                </div>
                            </div>
                            <div className="footer-actions">
                                <Button onClick={() => setOpen(false)} size="small" type="default">Cancel</Button>
                                <Button disabled={pendingPrompt || saving} onClick={() => { void handleSave(); }} size="small" type="primary">Save settings</Button>
                            </div>
                        </>
                    ) : null}
                </div>
            </Drawer>
        </Container>
    );
};
