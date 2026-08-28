/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import styled from '@emotion/styled';
import { Button } from '@insight/lib/components';
import {
    FRONTEND_AGENT_EXECUTE_COMMAND,
    toCommandError,
    type JsonObject,
} from '@insight/lib/FrontendAgentCommand';
import {
    clearWindowMessageDebugRecords,
    getWindowMessageDebugRecords,
    setWindowMessageDebugEnabled,
    subscribeWindowMessageDebug,
    type WindowMessageDebugRecord,
} from '@insight/lib/WindowMessageRouter';
import { Input, Modal, Select, Tag } from 'antd';
import React, { useCallback, useEffect, useMemo, useState } from 'react';
import { frontendAgentCommandController } from '@/agent/frontendAgentCommandController';

interface WindowMessageDebuggerProps {
    open: boolean;
    onClose: () => void;
}

interface ReplayResult {
    ok: boolean;
    durationMs: number;
    text: string;
}

const ALL = 'all';
const safeStringify = (value: unknown): string => {
    try {
        return JSON.stringify(value, null, 2) ?? String(value);
    } catch {
        return String(value);
    }
};

export const WindowMessageDebugger = ({ open, onClose }: WindowMessageDebuggerProps): JSX.Element => {
    const [records, setRecords] = useState<readonly WindowMessageDebugRecord[]>(() => getWindowMessageDebugRecords());
    const [direction, setDirection] = useState(ALL);
    const [channel, setChannel] = useState(ALL);
    const [query, setQuery] = useState('');
    const [replaying, setReplaying] = useState<ReadonlySet<number>>(new Set());
    const [replayResults, setReplayResults] = useState<ReadonlyMap<number, ReplayResult>>(new Map());

    const replay = useCallback(async (record: WindowMessageDebugRecord): Promise<void> => {
        if (!record.command || replaying.has(record.id)) return;
        setReplaying(current => new Set(current).add(record.id));
        const startedAt = performance.now();
        try {
            const output = await frontendAgentCommandController.replayCommandForDebug(
                record.command,
                (record.args ?? {}) as JsonObject,
            );
            setReplayResults(current => new Map(current).set(record.id, {
                ok: true,
                durationMs: performance.now() - startedAt,
                text: safeStringify(output),
            }));
        } catch (error) {
            setReplayResults(current => new Map(current).set(record.id, {
                ok: false,
                durationMs: performance.now() - startedAt,
                text: safeStringify(toCommandError(error)),
            }));
        } finally {
            setReplaying(current => {
                const next = new Set(current);
                next.delete(record.id);
                return next;
            });
        }
    }, [replaying]);

    useEffect(() => {
        setWindowMessageDebugEnabled(true);
        return () => setWindowMessageDebugEnabled(false);
    }, []);

    useEffect(() => {
        if (!open) return;
        return subscribeWindowMessageDebug(setRecords);
    }, [open]);

    const channelOptions = useMemo(() => [
        { label: 'All channels', value: ALL },
        ...Array.from(new Set(records.map(record => record.channel))).sort((left, right) => left.localeCompare(right)).map(value => ({ label: value, value })),
    ], [records]);
    const filteredRecords = useMemo(() => {
        const normalizedQuery = query.trim().toLowerCase();
        return [...records].reverse().filter((record) => {
            if (direction !== ALL && record.direction !== direction) return false;
            if (channel !== ALL && record.channel !== channel) return false;
            if (!normalizedQuery) return true;
            return [
                record.event,
                record.requestId,
                record.sessionId,
                record.moduleId,
                record.command,
                record.targetRequestId,
                record.connectionToken,
                record.status,
                record.summary,
                record.payload,
            ].some(value => value?.toLowerCase().includes(normalizedQuery));
        });
    }, [records, direction, channel, query]);

    return <Modal
        title={`Window Messages (${filteredRecords.length}/${records.length})`}
        open={open}
        onCancel={onClose}
        footer={null}
        width="min(1100px, 92vw)"
        destroyOnClose={false}
    >
        <Toolbar>
            <Select
                value={direction}
                onChange={setDirection}
                options={[
                    { label: 'All directions', value: ALL },
                    { label: 'Inbound', value: 'inbound' },
                    { label: 'Outbound', value: 'outbound' },
                ]}
            />
            <Select value={channel} onChange={setChannel} options={channelOptions} />
            <Input
                value={query}
                onChange={event => setQuery(event.target.value)}
                allowClear
                placeholder="Filter event, requestId, moduleId, command, payload"
            />
            <Button onClick={clearWindowMessageDebugRecords}>Clear</Button>
        </Toolbar>
        <RecordList>
            {filteredRecords.length === 0
                ? <EmptyState>No window messages match the current filters.</EmptyState>
                : filteredRecords.map(record => <WindowMessageRecordView
                    key={record.id}
                    record={record}
                    replaying={replaying.has(record.id)}
                    replayResult={replayResults.get(record.id)}
                    onReplay={() => {
                        void replay(record);
                    }}
                />)}
        </RecordList>
    </Modal>;
};

interface WindowMessageRecordViewProps {
    record: WindowMessageDebugRecord;
    replaying: boolean;
    replayResult?: ReplayResult;
    onReplay: () => void;
}

const WindowMessageRecordView = ({ record, replaying, replayResult, onReplay }: WindowMessageRecordViewProps): JSX.Element => {
    const [expanded, setExpanded] = useState(false);
    const replayable = record.event === FRONTEND_AGENT_EXECUTE_COMMAND && record.direction === 'inbound' && Boolean(record.command);
    return <Record>
        <RecordHeader type="button" onClick={() => setExpanded(value => !value)}>
            <Time>{formatTime(record.timestamp)}</Time>
            <Direction data-direction={record.direction}>{record.direction === 'inbound' ? 'IN' : 'OUT'}</Direction>
            <Tag>{record.channel}</Tag>
            <Route title={`${record.source ?? '?'} → ${record.target ?? '?'}`}>
                <RouteEndpoint>{record.source ?? '?'}</RouteEndpoint>
                <RouteArrow>→</RouteArrow>
                <RouteEndpoint>{record.target ?? '?'}</RouteEndpoint>
            </Route>
            <EventName>{record.event}</EventName>
            <Summary title={record.summary}>{record.summary || '-'}</Summary>
            <TruncatedCell>{record.truncated ? <Tag color="orange">truncated</Tag> : null}</TruncatedCell>
            <Expand>{expanded ? 'Hide' : 'Payload'}</Expand>
        </RecordHeader>
        {replayable
            ? <ReplayBar>
                <Button size="small" loading={replaying} onClick={onReplay}>Replay</Button>
                {replayResult
                    ? <>
                        <Tag color={replayResult.ok ? 'green' : 'red'}>
                            {replayResult.ok ? 'ok' : 'error'} · {replayResult.durationMs.toFixed(0)} ms
                        </Tag>
                        <ReplayResultText ok={replayResult.ok}>{replayResult.text || '(empty response)'}</ReplayResultText>
                    </>
                    : <ReplayHint>按原参数重跑该命令，仅经过 framework 执行路径（跳过 agent iframe）。</ReplayHint>}
            </ReplayBar>
            : null}
        {expanded
            ? <RecordDetails>
                <Metadata>
                    {record.origin ? <span>origin: {record.origin}</span> : null}
                    {record.source ? <span>source: {record.source}</span> : null}
                    {record.target ? <span>target: {record.target}</span> : null}
                    {record.sessionId ? <span>sessionId: {record.sessionId}</span> : null}
                    {record.command ? <span>command: {record.command}</span> : null}
                    {record.targetRequestId ? <span>targetRequestId: {record.targetRequestId}</span> : null}
                    {record.connectionToken ? <span>connectionToken: {record.connectionToken}</span> : null}
                    {record.status ? <span>status: {record.status}</span> : null}
                </Metadata>
                <Payload>{record.payload}</Payload>
            </RecordDetails>
            : null}
    </Record>;
};

const formatTime = (timestamp: number): string => new Date(timestamp).toLocaleTimeString(undefined, {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    fractionalSecondDigits: 3,
});

const ReplayBar = styled.div`
    display: flex;
    align-items: flex-start;
    padding: 4px 10px 8px;
    gap: 8px;
    border-top: 1px dashed ${(props): string => props.theme.borderColorLight};

    > .ant-btn {
        flex-shrink: 0;
    }

    > .ant-tag {
        flex-shrink: 0;
        margin-top: 2px;
    }
`;

const ReplayHint = styled.span`
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
`;

const ReplayResultText = styled.pre<{ ok: boolean }>`
    max-height: 180px;
    margin: 0;
    padding: 6px 8px;
    overflow: auto;
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColorDark};
    color: ${(props): string => props.ok ? props.theme.tableTextColor : '#ff7875'};
    font-family: Consolas, "Courier New", monospace;
    font-size: 12px;
    line-height: 16px;
    white-space: pre-wrap;
    word-break: break-word;
`;

const Toolbar = styled.div`
    display: grid;
    grid-template-columns: 150px 180px minmax(260px, 1fr) auto;
    gap: 8px;
    margin-bottom: 12px;

    @media (max-width: 800px) {
        grid-template-columns: 1fr 1fr;
    }
`;

const RecordList = styled.div`
    max-height: 65vh;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
`;

const Record = styled.div`
    border-bottom: 1px solid ${(props): string => props.theme.borderColorLight};

    &:last-child {
        border-bottom: 0;
    }
`;

const RecordHeader = styled.button`
    display: grid;
    grid-template-columns: 92px 38px max-content minmax(170px, 1fr) minmax(160px, 1fr) minmax(180px, 2fr) 76px 58px;
    width: 100%;
    padding: 8px 10px;
    border: 0;
    background: transparent;
    color: ${(props): string => props.theme.textColorPrimary};
    cursor: pointer;
    gap: 8px;
    align-items: center;
    text-align: left;

    &:hover {
        background: ${(props): string => props.theme.bgColorLight};
    }

    @media (max-width: 800px) {
        grid-template-columns: 82px 38px minmax(140px, 1fr) minmax(120px, 1fr) 58px;

        > :nth-of-type(3),
        > :nth-of-type(6),
        > :nth-of-type(7) {
            display: none;
        }
    }
`;

const Time = styled.span`
    color: ${(props): string => props.theme.textColorSecondary};
    font-family: Consolas, "Courier New", monospace;
    font-size: 12px;
`;

const Direction = styled.span`
    border-radius: 3px;
    padding: 1px 4px;
    color: #ffffff;
    background: #1677ff;
    font-size: 11px;
    font-weight: 600;
    text-align: center;

    &[data-direction='outbound'] {
        background: #52a447;
    }
`;

const Route = styled.span`
    display: flex;
    min-width: 0;
    align-items: center;
    color: ${(props): string => props.theme.textColorSecondary};
    font-family: ui-monospace, "SFMono-Regular", Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
    font-size: 12px;
    gap: 5px;
`;

const RouteEndpoint = styled.span`
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const RouteArrow = styled.span`
    flex: 0 0 auto;
    color: ${(props): string => props.theme.primaryColor};
    font-weight: 700;
`;

const EventName = styled.span`
    && {
        overflow: hidden;
        font-family: ui-monospace, "SFMono-Regular", Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
        font-variant-ligatures: none;
        font-weight: 600;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
`;

const Summary = styled.span`
    overflow: hidden;
    color: ${(props): string => props.theme.textColorSecondary};
    font-family: Consolas, "Courier New", monospace;
    font-size: 12px;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const TruncatedCell = styled.span`
    display: flex;
    width: 76px;
    justify-content: flex-start;
`;

const Expand = styled.span`
    color: ${(props): string => props.theme.primaryColor};
    text-align: right;
`;

const RecordDetails = styled.div`
    padding: 0 10px 10px;
`;

const Metadata = styled.div`
    display: flex;
    flex-wrap: wrap;
    margin-bottom: 6px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    gap: 12px;
`;

const Payload = styled.pre`
    max-height: 360px;
    margin: 0;
    padding: 10px;
    overflow: auto;
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColorDark};
    color: ${(props): string => props.theme.tableTextColor};
    font-family: Consolas, "Courier New", monospace;
    font-size: 12px;
    line-height: 18px;
    white-space: pre-wrap;
    word-break: break-word;
`;

const EmptyState = styled.div`
    padding: 36px 12px;
    color: ${(props): string => props.theme.textColorSecondary};
    text-align: center;
`;
