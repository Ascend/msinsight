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
import {
    createContext,
    useContext,
    useEffect,
    useMemo,
    useRef,
    useState,
    type ReactNode,
    type RefObject,
} from 'react';
import { message } from 'antd';
import { cancelPrompt, createSession, deleteSession, fetchSessions, fetchState, loadSession, sendPrompt, setSessionMode, setSessionModel, switchAgent } from '../api';
import { apiUrl } from '../env';
import type { AgentCapabilities, AgentInfo, AgentServerItem, AppState, AvailableCommand, AvailableSkill, ChatMessage, ConfigOption, ImageAttachment, QueuedPrompt, ServerEvent, SessionItem, SessionRecord, SessionStatus } from '../types';

interface ChatStateValue {
    configOptions: ConfigOption[];
    agentServers: AgentServerItem[];
    activeAgentName?: string;
    agentInfo?: AgentInfo;
    agentError?: string;
    agentCapabilities?: AgentCapabilities;
    draftMode?: string;
    availableCommands: AvailableCommand[];
    availableSkills: AvailableSkill[];
    switchingAgent: boolean;
    currentSessionId?: string;
    input: string;
    images: ImageAttachment[];
    isDraftSession?: boolean;
    messages: ChatMessage[];
    messagesRef: RefObject<HTMLDivElement>;
    pendingPrompt: boolean;
    queuedCount: number;
    queuedPrompts: QueuedPrompt[];
    sessions: SessionItem[];
    createSession: () => Promise<void>;
    deleteSession: (session: SessionItem) => Promise<void>;
    sendMessage: () => Promise<void>;
    cancelMessage: () => Promise<void>;
    selectSession: (session: SessionItem) => Promise<void>;
    setInput: (value: string) => void;
    addImages: (images: ImageAttachment[]) => void;
    removeImage: (id: string) => void;
    clearQueuedPrompts: () => void;
    removeQueuedPrompt: (index: number) => void;
    setModel: (model: string) => Promise<void>;
    setMode: (mode: string, sessionId?: string) => Promise<void>;
    setAgent: (name: string) => Promise<void>;
}

interface ChatState {
    initialized?: boolean;
    activeSessionId?: string;
    isDraftSession: boolean;
    draftMessages: ChatMessage[];
    draftPendingPrompt: boolean;
    configOptions: ConfigOption[];
    agentServers: AgentServerItem[];
    activeAgentName?: string;
    agentInfo?: AgentInfo;
    agentError?: string;
    agentCapabilities?: AgentCapabilities;
    draftMode?: string;
    availableCommands: AvailableCommand[];
    availableSkills: AvailableSkill[];
    switchingAgent: boolean;
    sessions: SessionItem[];
    sessionRecords: Record<string, SessionRecord>;
    draftQueuedPrompts: QueuedPrompt[];
}

const initialState: ChatState = {
    isDraftSession: false,
    draftMessages: [],
    draftPendingPrompt: false,
    switchingAgent: false,
    sessions: [],
    configOptions: [],
    agentServers: [],
    availableCommands: [],
    availableSkills: [],
    sessionRecords: {},
    draftQueuedPrompts: [],
};

const ChatStateContext = createContext<ChatStateValue | null>(null);

export const ChatStateProvider = ({ children }: { children: ReactNode }): JSX.Element => {
    const [state, setState] = useState<ChatState>(initialState);
    const [input, setInput] = useState('');
    const [images, setImages] = useState<ImageAttachment[]>([]);
    const messagesRef = useRef<HTMLDivElement>(null);
    const stateRef = useRef(state);
    const queuedPromptInFlightRef = useRef(false);

    useEffect(() => {
        stateRef.current = state;
    }, [state]);

    const refreshSessions = async (): Promise<void> => {
        const sessions = await fetchSessions();
        setState((current) => ({ ...current, sessions: mergeSessionStatuses(sessions, current.sessionRecords) }));
    };

    const refreshInitialState = async (): Promise<Partial<AppState>> => {
        const nextState = await fetchState();
        setState((current) => ({
            ...current,
            initialized: nextState.initialized,
            configOptions: nextState.configOptions ?? current.configOptions,
            agentServers: nextState.agentServers ?? current.agentServers,
            activeAgentName: nextState.activeAgentName ?? current.activeAgentName,
            agentInfo: nextState.agentInfo ?? current.agentInfo,
            agentError: nextState.agentError,
            agentCapabilities: nextState.agentCapabilities ?? current.agentCapabilities,
            availableCommands: nextState.availableCommands ?? current.availableCommands,
            availableSkills: nextState.availableSkills ?? current.availableSkills,
        }));
        return nextState;
    };

    const applyEvent = (event: ServerEvent): void => {
        if (event.type === 'state') {
            setState((current) => ({
                ...current,
                initialized: event.state.initialized ?? current.initialized,
                configOptions: event.state.configOptions ?? current.configOptions,
                agentServers: event.state.agentServers ?? current.agentServers,
                activeAgentName: event.state.activeAgentName ?? current.activeAgentName,
                agentInfo: event.state.agentInfo ?? current.agentInfo,
                agentError: event.state.agentError,
                agentCapabilities: event.state.agentCapabilities ?? current.agentCapabilities,
                availableCommands: event.state.availableCommands ?? current.availableCommands,
                availableSkills: event.state.availableSkills ?? current.availableSkills,
            }));
            return;
        }

        if (event.type === 'message_added' && event.sessionId) {
            updateSessionRecord(event.sessionId, (record) => ({
                ...record,
                messages: [...record.messages, event.message],
                loaded: true,
            }));
            return;
        }

        if (event.type === 'message_delta' && event.sessionId) {
            updateSessionRecord(event.sessionId, (record) => ({
                ...record,
                messages: record.messages.map((message) => {
                    if (message.id !== event.id) return message;
                    if (event.field === 'images') {
                        return { ...message, images: [...(message.images ?? []), ...event.delta] };
                    }
                    return { ...message, [event.field]: `${message[event.field] ?? ''}${event.delta}` };
                }),
                loaded: true,
            }));
            return;
        }

        if (event.type === 'message_removed' && event.sessionId) {
            updateSessionRecord(event.sessionId, (record) => ({
                ...record,
                messages: record.messages.filter((message) => message.id !== event.id),
            }));
            return;
        }

        if (event.type === 'prompt_status' && event.sessionId) {
            updateSessionRecord(event.sessionId, (record) => ({
                ...record,
                messages: event.pendingPrompt ? record.messages : markLastAssistantComplete(record.messages),
                pendingPrompt: event.pendingPrompt,
                status: event.pendingPrompt ? 'working' : 'completed',
            }));
            return;
        }

        if (event.type === 'config_options') {
            if (!event.sessionId) {
                setState((current) => ({ ...current, configOptions: event.configOptions }));
                return;
            }
            updateSessionRecord(event.sessionId, (record) => ({ ...record, configOptions: event.configOptions }));
        }
    };

    const updateSessionRecord = (sessionId: string, updater: (record: SessionRecord) => SessionRecord): void => {
        setState((current) => {
            const record = updater(getSessionRecord(current, sessionId));
            const sessionRecords = { ...current.sessionRecords, [sessionId]: record };
            return {
                ...current,
                sessionRecords,
                sessions: mergeSessionStatuses(current.sessions, sessionRecords),
            };
        });
    };

    useEffect(() => {
        void initializeActiveSession();

        const events = new EventSource(apiUrl('/api/events'));
        events.onmessage = (event): void => applyEvent(JSON.parse(event.data) as ServerEvent);
        return () => events.close();
    }, []);

    useEffect(() => {
        messagesRef.current?.scrollTo({ top: messagesRef.current.scrollHeight });
    }, [state.activeSessionId, state.isDraftSession, activeMessages(state)]);

    useEffect(() => {
        const nextPrompt = getNextQueuedPrompt(state);
        if (!nextPrompt) return;
        if (queuedPromptInFlightRef.current) return;
        queuedPromptInFlightRef.current = true;
        setState((current) => dequeuePrompt(current, nextPrompt));
        void sendPromptNow(nextPrompt.prompt, nextPrompt.isDraftSession, nextPrompt.sessionId).finally(() => {
            queuedPromptInFlightRef.current = false;
        });
    }, [state]);

    const initializeActiveSession = async (): Promise<void> => {
        const nextState = await refreshInitialState();
        const next = await fetchSessions();
        setState((current) => ({ ...current, sessions: mergeSessionStatuses(next, current.sessionRecords) }));

        if (nextState.agentError || nextState.initialized === false) return;
        const firstSession = next.find((session) => !session.isPending);
        if (firstSession) {
            await selectSessionById(firstSession.sessionId);
            return;
        }
        await createRealSession();
    };

    const createRealSession = async (): Promise<void> => {
        if (activePendingPrompt(stateRef.current)) return;
        try {
            const response = await createSession();
            if (!response.sessionId) return;
            setState((current) => {
                const sessions = ensureCreatedSession(current.sessions, undefined, response.sessionId!, 'New session');
                return cacheLoadedSession({
                    ...current,
                    activeSessionId: response.sessionId,
                    isDraftSession: false,
                    draftMode: undefined,
                    draftMessages: [],
                    draftPendingPrompt: false,
                    draftQueuedPrompts: [],
                    sessions,
                }, response.sessionId!, response);
            });
            await refreshSessions();
        } catch (error) {
            message.error(error instanceof Error ? error.message : String(error));
            await refreshInitialState();
        }
    };

    const handleSelectSession = async (session: SessionItem): Promise<void> => {
        if (session.sessionId === state.activeSessionId || session.isPending) return;
        await selectSessionById(session.sessionId);
    };

    const selectSessionById = async (sessionId: string): Promise<void> => {
        if (sessionId === stateRef.current.activeSessionId) return;

        const existingRecord = stateRef.current.sessionRecords[sessionId];
        setState((current) => ({
            ...current,
            activeSessionId: sessionId,
            isDraftSession: false,
            sessionRecords: existingRecord?.loaded
                ? current.sessionRecords
                : {
                    ...current.sessionRecords,
                    [sessionId]: {
                        ...getSessionRecord(current, sessionId),
                        status: 'loading',
                    },
                },
        }));

        try {
            if (existingRecord?.loaded) return;

            const response = await loadSession(sessionId);
            setState((current) => cacheLoadedSession(current, sessionId, response));
        } catch (error) {
            updateSessionRecord(sessionId, (record) => ({ ...record, status: 'error' }));
            message.error(error instanceof Error ? error.message : String(error));
        }
    };

    const handleDeleteSession = async (session: SessionItem): Promise<void> => {
        if (session.pendingPrompt || session.isPending) return;
        const previousState = state;
        const nextSession = state.sessions.filter((item) => item.sessionId !== session.sessionId)[0];
        const shouldLoadNextSession = state.activeSessionId === session.sessionId
            && nextSession
            && !state.sessionRecords[nextSession.sessionId]?.loaded;
        setState((current) => getOptimisticDeleteState(current, session));

        try {
            await deleteSession(session.sessionId);
            if (shouldLoadNextSession) {
                await loadSessionIntoCache(nextSession.sessionId);
            }
        } catch (error) {
            setState(previousState);
            message.error(error instanceof Error ? error.message : String(error));
        }
    };

    const loadSessionIntoCache = async (sessionId: string): Promise<void> => {
        const response = await loadSession(sessionId);
        setState((current) => cacheLoadedSession(current, sessionId, response));
    };

    const sendMessage = async (): Promise<void> => {
        const text = input.trim();
        const promptImages = images;
        if (!text && !promptImages.length) return;
        if (activePendingPrompt(state)) {
            queuePrompt({ text, images: promptImages });
            setInput('');
            setImages([]);
            return;
        }

        setInput('');
        setImages([]);
        await sendPromptNow({ text, images: promptImages, mode: state.isDraftSession ? state.draftMode : undefined }, state.isDraftSession, state.activeSessionId);
    };

    const queuePrompt = (prompt: QueuedPrompt): void => {
        setState((current) => {
            if (current.isDraftSession || !current.activeSessionId) {
                return { ...current, draftQueuedPrompts: [...current.draftQueuedPrompts, prompt] };
            }
            const record = getSessionRecord(current, current.activeSessionId);
            const sessionRecords = {
                ...current.sessionRecords,
                [current.activeSessionId]: {
                    ...record,
                    queuedPrompts: [...record.queuedPrompts, prompt],
                },
            };
            return {
                ...current,
                sessionRecords,
                sessions: mergeSessionStatuses(current.sessions, sessionRecords),
            };
        });
    };

    const sendPromptNow = async (prompt: QueuedPrompt, isDraftSession: boolean, sessionId?: string): Promise<void> => {
        const optimisticSession = createOptimisticSession(prompt, isDraftSession);
        setState((current) => markPromptStarted(current, prompt, isDraftSession, sessionId, optimisticSession));

        try {
            const body = await sendPrompt(prompt.text, isDraftSession, sessionId, prompt.images, prompt.mode, prompt.hiddenContext);
            setState((current) => applyPromptSessionResult(current, prompt, optimisticSession, body.sessionId));
        } catch (error) {
            setState((current) => markPromptFailed(current, optimisticSession, sessionId));
            message.error(error instanceof Error ? error.message : String(error));
            await refreshInitialState();
        }
    };

    const addImages = (nextImages: ImageAttachment[]): void => {
        setImages((current) => [...current, ...nextImages]);
    };

    const removeImage = (id: string): void => {
        setImages((current) => current.filter((image) => image.id !== id));
    };

    const clearQueuedPrompts = (): void => {
        setState(clearActiveQueuedPrompts);
    };

    const removeQueuedPrompt = (index: number): void => {
        setState((current) => removeActiveQueuedPrompt(current, index));
    };

    const cancelMessage = async (): Promise<void> => {
        const sessionId = state.activeSessionId;
        setState((current) => markPromptCancelled(current, sessionId));
        if (!sessionId) return;
        try {
            await cancelPrompt(sessionId);
        } catch (error) {
            message.error(error instanceof Error ? error.message : String(error));
            await refreshInitialState();
        }
    };

    const updateModel = async (model: string): Promise<void> => {
        if (!model || activePendingPrompt(state)) return;
        try {
            const response = await setSessionModel(model, state.activeSessionId);
            setState((current) => {
                if (!current.activeSessionId) {
                    return { ...current, configOptions: response.configOptions ?? current.configOptions };
                }
                const record = {
                    ...getSessionRecord(current, current.activeSessionId),
                    configOptions: response.configOptions ?? getActiveConfigOptions(current),
                };
                return {
                    ...current,
                    sessionRecords: { ...current.sessionRecords, [current.activeSessionId]: record },
                };
            });
        } catch (error) {
            message.error(error instanceof Error ? error.message : String(error));
            await refreshInitialState();
        }
    };

    const updateMode = async (mode: string, sessionId?: string): Promise<void> => {
        const targetSessionId = sessionId ?? state.activeSessionId;
        if (!mode || activePendingPrompt(state)) return;
        if (!targetSessionId) {
            setState((current) => ({
                ...current,
                draftMode: mode,
                configOptions: updateConfigCurrentValue(current.configOptions, getModeConfig(current.configOptions)?.id, mode),
            }));
            return;
        }
        try {
            const response = await setSessionMode(mode, targetSessionId);
            setState((current) => {
                const record = {
                    ...getSessionRecord(current, targetSessionId),
                    configOptions: response.configOptions ?? getActiveConfigOptions(current),
                };
                return {
                    ...current,
                    sessionRecords: { ...current.sessionRecords, [targetSessionId]: record },
                };
            });
        } catch (error) {
            message.error(error instanceof Error ? error.message : String(error));
            await refreshInitialState();
        }
    };

    const updateAgent = async (name: string): Promise<void> => {
        if (!name || name === state.activeAgentName || activePendingPrompt(state)) return;
        setState((current) => ({ ...current, switchingAgent: true }));
        try {
            const response = await switchAgent(name);
            setState((current) => ({
                ...current,
                activeSessionId: undefined,
                isDraftSession: false,
                draftMode: undefined,
                draftMessages: [],
                draftPendingPrompt: false,
                draftQueuedPrompts: [],
                sessions: [],
                sessionRecords: {},
                configOptions: response.configOptions ?? [],
                agentServers: response.agentServers ?? current.agentServers,
                activeAgentName: response.activeAgentName ?? name,
                agentInfo: undefined,
                agentError: undefined,
                switchingAgent: false,
            }));
            await initializeActiveSession();
        } catch (error) {
            setState((current) => ({ ...current, switchingAgent: false }));
            message.error(error instanceof Error ? error.message : String(error));
        }
    };

    const value = useMemo<ChatStateValue>(() => ({
        configOptions: getActiveConfigOptions(state),
        agentServers: state.agentServers,
        activeAgentName: state.activeAgentName,
        agentInfo: state.agentInfo,
        agentError: state.agentError,
        agentCapabilities: state.agentCapabilities,
        availableCommands: state.availableCommands,
        availableSkills: state.availableSkills,
        switchingAgent: state.switchingAgent,
        currentSessionId: state.activeSessionId,
        input,
        images,
        isDraftSession: state.isDraftSession,
        messages: activeMessages(state),
        messagesRef,
        pendingPrompt: activePendingPrompt(state),
        queuedCount: activeQueuedCount(state),
        queuedPrompts: activeQueuedPrompts(state),
        sessions: state.sessions,
        createSession: createRealSession,
        deleteSession: handleDeleteSession,
        sendMessage,
        cancelMessage,
        selectSession: handleSelectSession,
        setInput,
        addImages,
        removeImage,
        clearQueuedPrompts,
        removeQueuedPrompt,
        setModel: updateModel,
        setMode: updateMode,
        setAgent: updateAgent,
    }), [images, input, state]);

    return <ChatStateContext.Provider value={value}>{children}</ChatStateContext.Provider>;
};

export const useChatState = (): ChatStateValue => {
    const context = useContext(ChatStateContext);
    if (!context) {
        throw new Error('useChatState must be used within ChatStateProvider');
    }
    return context;
};

const getSessionRecord = (state: ChatState, sessionId: string): SessionRecord => {
    return state.sessionRecords[sessionId] ?? {
        sessionId,
        messages: [],
        configOptions: state.configOptions,
        loaded: false,
        pendingPrompt: false,
        queuedPrompts: [],
        status: 'idle',
    };
};

const activeMessages = (state: ChatState): ChatMessage[] => {
    if (state.isDraftSession || !state.activeSessionId) return state.draftMessages;
    return getSessionRecord(state, state.activeSessionId).messages;
};

const activePendingPrompt = (state: ChatState): boolean => {
    if (state.isDraftSession || !state.activeSessionId) return state.draftPendingPrompt;
    return getSessionRecord(state, state.activeSessionId).pendingPrompt;
};

const getActiveConfigOptions = (state: ChatState): ConfigOption[] => {
    if (state.isDraftSession || !state.activeSessionId) return state.configOptions;
    return getSessionRecord(state, state.activeSessionId).configOptions ?? state.configOptions;
};

const getModeConfig = (configOptions: ConfigOption[]): ConfigOption | undefined => {
    return configOptions.find((option) => option.category === 'mode')
        ?? configOptions.find((option) => option.id.toLowerCase().includes('mode'));
};

const updateConfigCurrentValue = (configOptions: ConfigOption[], configId: string | undefined, value: string): ConfigOption[] => {
    if (!configId) return configOptions;
    return configOptions.map((option) => option.id === configId ? { ...option, currentValue: value } : option);
};

const cacheLoadedSession = (
    state: ChatState,
    sessionId: string,
    response: { messages?: ChatMessage[]; configOptions?: ConfigOption[]; pendingPrompt?: boolean },
): ChatState => {
    const record: SessionRecord = {
        ...getSessionRecord(state, sessionId),
        messages: response.messages ?? [],
        configOptions: response.configOptions ?? state.configOptions,
        pendingPrompt: Boolean(response.pendingPrompt),
        queuedPrompts: getSessionRecord(state, sessionId).queuedPrompts,
        loaded: true,
        status: response.pendingPrompt ? 'working' : 'idle',
    };
    const sessionRecords = { ...state.sessionRecords, [sessionId]: record };
    return {
        ...state,
        sessionRecords,
        sessions: mergeSessionStatuses(state.sessions, sessionRecords),
    };
};

const activeQueuedCount = (state: ChatState): number => {
    if (state.isDraftSession || !state.activeSessionId) return state.draftQueuedPrompts.length;
    return getSessionRecord(state, state.activeSessionId).queuedPrompts.length;
};

const activeQueuedPrompts = (state: ChatState): QueuedPrompt[] => {
    if (state.isDraftSession || !state.activeSessionId) return state.draftQueuedPrompts;
    return getSessionRecord(state, state.activeSessionId).queuedPrompts;
};

const clearActiveQueuedPrompts = (state: ChatState): ChatState => {
    if (state.isDraftSession || !state.activeSessionId) return { ...state, draftQueuedPrompts: [] };
    const record = getSessionRecord(state, state.activeSessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [state.activeSessionId]: { ...record, queuedPrompts: [] },
    };
    return { ...state, sessionRecords, sessions: mergeSessionStatuses(state.sessions, sessionRecords) };
};

const removeActiveQueuedPrompt = (state: ChatState, index: number): ChatState => {
    if (state.isDraftSession || !state.activeSessionId) {
        return { ...state, draftQueuedPrompts: state.draftQueuedPrompts.filter((_, promptIndex) => promptIndex !== index) };
    }
    const record = getSessionRecord(state, state.activeSessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [state.activeSessionId]: {
            ...record,
            queuedPrompts: record.queuedPrompts.filter((_, promptIndex) => promptIndex !== index),
        },
    };
    return { ...state, sessionRecords, sessions: mergeSessionStatuses(state.sessions, sessionRecords) };
};

const getNextQueuedPrompt = (state: ChatState): { prompt: QueuedPrompt; isDraftSession: boolean; sessionId?: string } | undefined => {
    if (!state.draftPendingPrompt && state.draftQueuedPrompts.length) {
        return { prompt: state.draftQueuedPrompts[0], isDraftSession: true };
    }

    for (const record of Object.values(state.sessionRecords)) {
        if (!record.pendingPrompt && record.queuedPrompts.length) {
            return { prompt: record.queuedPrompts[0], isDraftSession: false, sessionId: record.sessionId };
        }
    }
    return undefined;
};

const dequeuePrompt = (
    state: ChatState,
    prompt: { isDraftSession: boolean; sessionId?: string },
): ChatState => {
    if (prompt.isDraftSession) {
        return { ...state, draftQueuedPrompts: state.draftQueuedPrompts.slice(1) };
    }
    if (!prompt.sessionId) return state;
    const record = getSessionRecord(state, prompt.sessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [prompt.sessionId]: { ...record, queuedPrompts: record.queuedPrompts.slice(1) },
    };
    return {
        ...state,
        sessionRecords,
        sessions: mergeSessionStatuses(state.sessions, sessionRecords),
    };
};

const markLastAssistantComplete = (messages: ChatMessage[]): ChatMessage[] => {
    for (let index = messages.length - 1; index >= 0; index -= 1) {
        if (messages[index].role === 'assistant') {
            return messages.map((message, messageIndex) => (
                messageIndex === index ? { ...message, pending: false } : message
            ));
        }
    }
    return messages;
};

const mergeSessionStatuses = (sessions: SessionItem[], records: Record<string, SessionRecord>): SessionItem[] => {
    return sessions.map((session) => {
        const record = records[session.sessionId];
        if (!record) return session;
        return {
            ...session,
            pendingPrompt: record.pendingPrompt,
            status: record.status,
        };
    });
};

const createOptimisticSession = (prompt: QueuedPrompt, isDraftSession?: boolean): SessionItem | undefined => {
    if (!isDraftSession) return undefined;
    return {
        sessionId: '__pending_session__',
        title: getPromptTitle(prompt),
        updatedAt: 'Creating...',
        isPending: true,
        pendingPrompt: true,
        status: 'working',
    };
};

const applyPromptSessionResult = (
    state: ChatState,
    prompt: QueuedPrompt,
    optimisticSession: SessionItem | undefined,
    sessionId: string | undefined,
): ChatState => {
    if (!sessionId) return state;

    const existingRecord = getSessionRecord(state, sessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [sessionId]: {
            ...existingRecord,
            loaded: true,
            pendingPrompt: true,
            status: 'working' as SessionStatus,
        },
    };

    const sessions = ensureCreatedSession(state.sessions, optimisticSession, sessionId, getPromptTitle(prompt));
    return {
        ...state,
        activeSessionId: state.isDraftSession ? sessionId : state.activeSessionId,
        isDraftSession: state.isDraftSession ? false : state.isDraftSession,
        draftPendingPrompt: false,
        sessionRecords,
        sessions: mergeSessionStatuses(sessions, sessionRecords),
    };
};

const markPromptStarted = (
    state: ChatState,
    _prompt: QueuedPrompt,
    isDraftSession: boolean,
    sessionId: string | undefined,
    optimisticSession: SessionItem | undefined,
): ChatState => {
    if (isDraftSession || !sessionId) {
        return {
            ...state,
            draftPendingPrompt: true,
            sessions: optimisticSession ? [optimisticSession, ...state.sessions] : state.sessions,
        };
    }

    const record = getSessionRecord(state, sessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [sessionId]: {
            ...record,
            pendingPrompt: true,
            loaded: true,
            status: 'working' as SessionStatus,
        },
    };
    return {
        ...state,
        sessionRecords,
        sessions: mergeSessionStatuses(state.sessions, sessionRecords),
    };
};

const markPromptFailed = (
    state: ChatState,
    optimisticSession: SessionItem | undefined,
    sessionId: string | undefined,
): ChatState => {
    if (!sessionId) {
        return {
            ...state,
            draftPendingPrompt: false,
            sessions: optimisticSession ? state.sessions.filter((session) => session.sessionId !== optimisticSession.sessionId) : state.sessions,
        };
    }

    const record = getSessionRecord(state, sessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [sessionId]: {
            ...record,
            messages: markLastAssistantComplete(record.messages),
            pendingPrompt: false,
            status: 'error' as SessionStatus,
        },
    };
    return {
        ...state,
        sessionRecords,
        sessions: mergeSessionStatuses(state.sessions, sessionRecords),
    };
};

const markPromptCancelled = (state: ChatState, sessionId: string | undefined): ChatState => {
    if (!sessionId) {
        return {
            ...state,
            draftPendingPrompt: false,
            draftQueuedPrompts: [],
        };
    }

    const record = getSessionRecord(state, sessionId);
    const sessionRecords = {
        ...state.sessionRecords,
        [sessionId]: {
            ...record,
            messages: markLastAssistantComplete(record.messages),
            pendingPrompt: false,
            queuedPrompts: [],
            status: 'idle' as SessionStatus,
        },
    };
    return {
        ...state,
        sessionRecords,
        sessions: mergeSessionStatuses(state.sessions, sessionRecords),
    };
};

const ensureCreatedSession = (
    sessions: SessionItem[],
    optimisticSession: SessionItem | undefined,
    sessionId: string,
    title: string,
): SessionItem[] => {
    const withoutPendingSession = optimisticSession
        ? sessions.filter((session) => session.sessionId !== optimisticSession.sessionId)
        : sessions;
    if (withoutPendingSession.some((session) => session.sessionId === sessionId)) return withoutPendingSession;
    return [{ sessionId, title: title.slice(0, 80), updatedAt: 'Just now', pendingPrompt: true, status: 'working' }, ...withoutPendingSession];
};

const getPromptTitle = (prompt: QueuedPrompt): string => {
    const text = prompt.text.trim();
    if (text) return text.slice(0, 80);
    return prompt.images.length === 1 ? 'Image' : `${prompt.images.length} images`;
};

const getOptimisticDeleteState = (state: ChatState, deletedSession: SessionItem): ChatState => {
    const sessions = state.sessions.filter((session) => session.sessionId !== deletedSession.sessionId);
    const { [deletedSession.sessionId]: _deletedRecord, ...sessionRecords } = state.sessionRecords;
    if (state.activeSessionId !== deletedSession.sessionId) return { ...state, sessions, sessionRecords };

    const nextSession = sessions[0];
    if (!nextSession) {
        return {
            ...state,
            activeSessionId: undefined,
            isDraftSession: true,
            draftMessages: [],
            draftPendingPrompt: false,
            sessions,
            sessionRecords,
        };
    }

    return {
        ...state,
        activeSessionId: nextSession.sessionId,
        isDraftSession: false,
        sessions,
        sessionRecords,
    };
};
