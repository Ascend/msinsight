/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export interface WindowMessageDebugRecord {
    id: number;
    timestamp: number;
    direction: 'inbound' | 'outbound';
    channel: string;
    event: string;
    requestId?: string;
    sessionId?: string;
    moduleId?: string;
    command?: string;
    args?: unknown;
    targetRequestId?: string;
    connectionToken?: string;
    status?: string;
    summary: string;
    origin?: string;
    source?: string;
    target?: string;
    payload: string;
    truncated: boolean;
}

export interface WindowMessageDebugInput {
    direction: WindowMessageDebugRecord['direction'];
    data: unknown;
    origin?: string;
    source?: string;
    target?: string;
}

const MAX_WINDOW_MESSAGE_RECORDS = 200;
const MAX_WINDOW_MESSAGE_PAYLOAD_LENGTH = 50000;
const subscribers = new Set<(records: readonly WindowMessageDebugRecord[]) => void>();
let records: WindowMessageDebugRecord[] = [];
let sequence = 0;
let enabled = false;

export const setWindowMessageDebugEnabled = (value: boolean): void => {
    enabled = value;
};

export const recordWindowMessageDebug = ({ direction, data, origin, source, target }: WindowMessageDebugInput): void => {
    if (!enabled) return;
    const normalizedData = normalizeDebugData(data);
    if (!isCommunicationMessage(normalizedData)) return;
    const serialized = serializeDebugPayload(normalizedData);
    const message = normalizedData as Record<string, unknown>;
    records = [...records, {
        id: ++sequence,
        timestamp: Date.now(),
        direction,
        channel: readString(message.channel) ?? 'connector',
        event: readString(message.event) ?? 'unknown',
        requestId: readString(message.requestId),
        sessionId: readString(message.sessionId),
        moduleId: readString(message.moduleId),
        command: readString(message.command),
        args: isJsonObject(message.args) ? message.args : undefined,
        targetRequestId: readString(message.targetRequestId),
        connectionToken: readString(message.connectionToken),
        status: readString(message.status),
        summary: createDebugSummary(message),
        origin,
        source: source ?? (direction === 'outbound' ? currentWindowName() : undefined),
        target: target ?? (direction === 'inbound' ? currentWindowName() : undefined),
        payload: serialized.value,
        truncated: serialized.truncated,
    }].slice(-MAX_WINDOW_MESSAGE_RECORDS);
    notifySubscribers();
};

export const getWindowMessageDebugRecords = (): readonly WindowMessageDebugRecord[] => [...records];

export const subscribeWindowMessageDebug = (listener: (records: readonly WindowMessageDebugRecord[]) => void): (() => void) => {
    subscribers.add(listener);
    listener(getWindowMessageDebugRecords());
    return () => {
        subscribers.delete(listener);
    };
};

export const clearWindowMessageDebugRecords = (): void => {
    records = [];
    notifySubscribers();
};

const notifySubscribers = (): void => {
    const snapshot = getWindowMessageDebugRecords();
    subscribers.forEach((listener) => {
        try {
            listener(snapshot);
        } catch (error) {
            globalThis.console.error(error);
        }
    });
};

const normalizeDebugData = (data: unknown): unknown => {
    if (typeof data !== 'string') return data;
    try {
        return JSON.parse(data);
    } catch {
        return data;
    }
};

const isCommunicationMessage = (data: unknown): boolean => {
    if (!data || typeof data !== 'object' || Array.isArray(data)) return false;
    const record = data as Record<string, unknown>;
    return typeof record.channel === 'string' || typeof record.event === 'string';
};

const serializeDebugPayload = (data: unknown): { value: string; truncated: boolean } => {
    let value: string;
    try {
        value = JSON.stringify(data, null, 2) ?? String(data);
    } catch {
        value = String(data);
    }
    if (value.length <= MAX_WINDOW_MESSAGE_PAYLOAD_LENGTH) return { value, truncated: false };
    return {
        value: `${value.slice(0, MAX_WINDOW_MESSAGE_PAYLOAD_LENGTH)}\n... [payload truncated]`,
        truncated: true,
    };
};

const readString = (value: unknown): string | undefined => typeof value === 'string' && value ? value : undefined;

const isJsonObject = (value: unknown): value is Record<string, unknown> => (
    Boolean(value) && typeof value === 'object' && !Array.isArray(value)
);

const createDebugSummary = (message: Record<string, unknown>): string => {
    const body = asRecord(message.body);
    const error = asRecord(message.error);
    const fields = [
        ['command', message.command],
        ['module', message.moduleId ?? message.module ?? body?.moduleId ?? body?.activeModule],
        ['request', message.requestId],
        ['targetRequest', message.targetRequestId],
        ['token', abbreviateToken(message.connectionToken)],
        ['session', message.sessionId],
        ['from', message.from],
        ['to', message.to],
        ['status', message.status],
        ['error', error?.code ?? error?.message],
    ] as const;
    return fields
        .map(([label, value]) => readSummaryValue(value) ? `${label}=${readSummaryValue(value)}` : undefined)
        .filter((value): value is string => Boolean(value))
        .join(' · ');
};

const asRecord = (value: unknown): Record<string, unknown> | undefined => (
    value && typeof value === 'object' && !Array.isArray(value) ? value as Record<string, unknown> : undefined
);

const abbreviateToken = (value: unknown): unknown => typeof value === 'string' && value.length > 12
    ? `${value.slice(0, 8)}…`
    : value;

const readSummaryValue = (value: unknown): string | undefined => {
    if (typeof value === 'string' && value) return value;
    if (typeof value === 'number' || typeof value === 'boolean') return String(value);
    return undefined;
};

const currentWindowName = (): string => typeof window === 'object' ? window.name || 'framework' : 'window';
