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
import { DownloadOutlined, QuestionCircleOutlined, RobotOutlined, SearchOutlined } from '@ant-design/icons';
import { Button, Checkbox, Input, Tooltip } from '@insight/lib/components';
import { Modal } from 'antd';
import React, { useCallback, useEffect, useId, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { agentKindLogoAsset } from '../agentBrand';
import { exportSessionBatch, fetchSessionExportStatus, querySessionExports } from '../api';
import { shouldPersistSessionExport } from '../env';
import { downloadSessionExport } from '../sessionExportDownload';
import type { AgentServerItem, SessionExportFilters, SessionExportStatus } from '../types';
import { AgentKindIcon } from './AgentKindIcon';

const PAGE_SIZE = 10;
const POLL_INTERVAL_MS = 750;
const DAY_MS = 24 * 60 * 60 * 1000;
type TimePreset = 'all' | 1 | 7 | 30 | 'custom';
type Operation = 'query' | 'export';
const TIME_PRESET_OPTIONS: Array<{ value: TimePreset; key: 'batchExportAllTime' | 'batchExportLastDay' | 'batchExportLastWeek' | 'batchExportLastMonth' | 'batchExportCustomRange' }> = [
    { value: 'all', key: 'batchExportAllTime' },
    { value: 1, key: 'batchExportLastDay' },
    { value: 7, key: 'batchExportLastWeek' },
    { value: 30, key: 'batchExportLastMonth' },
    { value: 'custom', key: 'batchExportCustomRange' },
];

const Container = styled.div`
    .ant-modal-content {
        display: flex;
        flex-direction: column;
        max-height: calc(100vh - 24px);
        overflow: hidden;
        border-radius: ${(props): string => props.theme.borderRadiusLarge || '8px'};
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
        box-shadow: ${(props): string => props.theme.boxShadowDark || props.theme.boxShadow};
    }
    .ant-modal-header {
        flex-shrink: 0;
        padding: 16px 20px;
        background: ${(props): string => props.theme.bgColor};
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
    }
    .ant-modal-title {
        font-size: 16px;
        font-weight: 600;
        color: ${(props): string => props.theme.textColorPrimary};
    }
    .dialog-title {
        display: flex;
        align-items: center;
        gap: 8px;
    }
    .dialog-title .anticon {
        color: ${(props): string => props.theme.primaryColor};
    }
    .ant-modal-close {
        color: ${(props): string => props.theme.textColorSecondary};
        top: 12px;
        right: 12px;
    }
    .ant-modal-close-x {
        width: 32px;
        height: 32px;
        line-height: 32px;
    }
    .ant-modal-close:hover {
        color: ${(props): string => props.theme.textColorPrimary};
    }
    .ant-modal-body {
        display: flex;
        flex-direction: column;
        min-height: 0;
        padding: 0;
        font-size: 13px;
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
    }
    .ant-modal-footer {
        flex-shrink: 0;
        padding: 12px 16px;
        background: ${(props): string => props.theme.bgColor};
        border-top: 1px solid ${(props): string => props.theme.borderColor};
        display: flex;
        flex-wrap: wrap;
        justify-content: flex-end;
        gap: 8px;
    }
    .ant-modal-footer .ant-btn, .ant-modal-footer .ant-btn + .ant-btn {
        height: auto;
        min-height: 32px;
        margin: 0;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
    }
    .export-body {
        display: flex;
        flex-direction: column;
        gap: 16px;
        max-height: 650px;
        padding: 14px 20px 18px;
        overflow: auto;
    }
    .filter-section {
        display: flex;
        flex-direction: column;
        gap: 8px;
        min-width: 0;
    }
    .section-heading {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
        flex-wrap: wrap;
    }
    .section-heading h3 {
        display: inline-flex;
        align-items: center;
        gap: 6px;
        margin: 0;
        font-size: 13px;
        font-weight: 500;
        color: ${(props): string => props.theme.textColorPrimary};
    }
    .section-heading h3 label {
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }
    .help-icon {
        border: 0;
        padding: 0;
        background: transparent;
        line-height: 1;
        color: ${(props): string => props.theme.icon || props.theme.textColorSecondary};
        font-size: 13px;
        cursor: pointer;
    }
    .help-icon:hover {
        color: ${(props): string => props.theme.primaryColor};
    }
    .agent-actions {
        display: flex;
        align-items: center;
        flex-wrap: wrap;
        gap: 8px;
    }
    .agent-count {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }
    .agent-actions .ant-btn {
        min-width: 0;
        padding: 0;
        height: 22px;
        font-size: 12px;
    }
    .agent-actions .ant-btn:disabled {
        color: ${(props): string => props.theme.textColorDisabled};
    }
    .agent-grid, .time-filters {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(min(100%, 230px), 1fr));
        gap: 8px;
    }
    .agent-grid {
        overflow: auto;
        max-height: 154px;
        padding: 3px;
    }
    .agent-option.ant-checkbox-wrapper {
        align-items: center;
        margin: 0;
        min-width: 0;
        padding: 7px 10px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
        background: ${(props): string => props.theme.bgColor};
    }
    .agent-option.ant-checkbox-wrapper-checked {
        background: ${(props): string => props.theme.primaryColorLight5};
    }
    .agent-option:not(.ant-checkbox-wrapper-disabled):hover {
        border-color: ${(props): string => props.theme.primaryColor};
    }
    .agent-option .ant-checkbox, .result-option .ant-checkbox {
        top: 0;
        flex: none;
    }
    .agent-option .ant-checkbox + span, .result-option .ant-checkbox + span {
        flex: 1;
        min-width: 0;
        padding-right: 0;
    }
    .agent-identity {
        display: flex;
        align-items: center;
        gap: 8px;
        min-width: 0;
    }
    .agent-logo {
        display: grid;
        place-items: center;
        flex: 0 0 24px;
        width: 24px;
        height: 24px;
        padding: 3px;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorSecondary};
    }
    .agent-name {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .time-filters .filter-section {
        align-self: start;
        padding: 10px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
    }
    .time-presets {
        display: flex;
        flex-wrap: wrap;
        gap: 6px;
    }
    .time-presets .ant-btn {
        min-width: 0;
        height: 30px;
        padding: 0 7px;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
        font-size: 12px;
        border-color: ${(props): string => props.theme.borderColor};
        color: ${(props): string => props.theme.textColorSecondary};
    }
    .time-presets .active:not(:disabled) {
        border-color: ${(props): string => props.theme.primaryColor};
        background: ${(props): string => props.theme.primaryColorLight5};
        color: ${(props): string => props.theme.primaryColor};
    }
    .time-presets .active:not(:disabled) > span {
        color: ${(props): string => props.theme.primaryColor};
    }
    .range-fields {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(min(100%, 174px), 1fr));
        gap: 8px;
        margin-top: 4px;
    }
    .range-field {
        display: flex;
        flex-direction: column;
        min-width: 0;
        gap: 4px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }
    .range-field .ant-input {
        min-width: 0;
        padding: 4px 8px;
        font-size: 12px;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
        color-scheme: ${(props): string => props.theme.mode === 'dark' ? 'dark' : 'light'};
    }
    .filter-section .ant-input,
    .filter-section .ant-input-affix-wrapper {
        width: 100%;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
    }
    .filter-action-bar {
        display: flex;
        justify-content: flex-end;
        margin-top: 2px;
    }
    .query-button.ant-btn {
        height: 32px;
        padding: 0 16px;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
    }
    .export-status:empty {
        position: absolute;
        width: 1px;
        height: 1px;
        overflow: hidden;
        clip-path: inset(50%);
        padding: 0;
    }
    .export-status {
        flex-basis: 100%;
        min-width: 0;
        max-height: 96px;
        overflow: auto;
        padding: 8px 12px;
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
        background: ${(props): string => props.theme.primaryColorLight5};
        color: ${(props): string => props.theme.textColorPrimary};
        line-height: 1.5;
        font-size: 12px;
        overflow-wrap: anywhere;
    }
    .export-saved {
        display: flex;
        align-items: center;
        gap: 4px;
        min-width: 0;
        text-align: left;
    }
    .saved-label {
        flex: none;
        white-space: nowrap;
    }
    .saved-path {
        flex: 1;
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .error {
        color: ${(props): string => props.theme.dangerColor};
        font-size: 12px;
        overflow-wrap: anywhere;
    }
    .ant-modal-footer .error {
        flex-basis: 100%;
        text-align: left;
    }
    .results {
        min-width: 0;
        border-top: 1px solid ${(props): string => props.theme.borderColor};
        padding-top: 12px;
    }
    .results-heading {
        margin-bottom: 8px;
        font-weight: 500;
        font-size: 13px;
        color: ${(props): string => props.theme.textColorPrimary};
    }
    .result-list {
        margin: 0;
        padding: 0;
        list-style: none;
    }
    .result-row {
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
    }
    .result-option.ant-checkbox-wrapper {
        width: 100%;
        display: flex;
        align-items: center;
        padding: 8px;
    }
    .result-option:not(.ant-checkbox-wrapper-disabled):hover,
    .result-option.ant-checkbox-wrapper-checked {
        background: ${(props): string => props.theme.bgColorDark};
    }
    .result-title {
        display: block;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 13px;
        line-height: 18px;
        overflow-wrap: anywhere;
    }
    .result-meta {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        gap: 4px 8px;
        margin-top: 2px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 11px;
    }
    .result-agent {
        display: inline-flex;
        align-items: center;
        gap: 4px;
        padding: 1px 6px;
        border-radius: 3px;
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorSecondary};
        overflow-wrap: anywhere;
    }
    .result-agent .agent-logo {
        flex-basis: 16px;
        width: 16px;
        height: 16px;
        padding: 1px;
    }
    .pagination {
        display: flex;
        justify-content: space-between;
        align-items: center;
        gap: 8px;
        padding-top: 10px;
    }
    .pagination .ant-btn {
        min-width: 0;
        height: 26px;
        padding: 0 10px;
        font-size: 12px;
    }
    .pagination-hint {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }
    .issues {
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusBase || '4px'};
        padding: 10px;
        line-height: 1.5;
        font-size: 12px;
    }
    .issues ul {
        max-height: 120px;
        overflow: auto;
        margin: 6px 0 0;
        padding-left: 16px;
        overflow-wrap: anywhere;
    }
    button:focus-visible, .saved-path:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 2px;
    }
    .ant-checkbox-input:focus-visible + .ant-checkbox-inner {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }
`;

const AgentLogo = ({ name }: { name: string }): JSX.Element => (
    <span className="agent-logo" aria-hidden="true">
        {agentKindLogoAsset({ name }) ? <AgentKindIcon name={name} /> : <RobotOutlined />}
    </span>
);

const FilterHelp = ({ text, label = text }: { text: string; label?: string }): JSX.Element => (
    <Tooltip title={text} trigger={['hover', 'focus']}>
        <button type="button" className="help-icon" aria-label={label}><QuestionCircleOutlined aria-hidden="true" /></button>
    </Tooltip>
);

export interface SessionBatchExportDialogProps {
    open: boolean;
    onOpenChange: (open: boolean) => void;
    /** Descriptor names are the owner IDs accepted by the export API. */
    agents: AgentServerItem[];
}

export const SessionBatchExportDialog = (props: SessionBatchExportDialogProps): JSX.Element | null => (
    props.open ? <SessionBatchExportContent {...props} /> : null
);

const utc = (value: string): string => {
    const parsed = new Date(value);
    return Number.isNaN(parsed.getTime()) ? value : parsed.toISOString();
};
const localDateTime = (date: Date): string => new Date(date.getTime() - date.getTimezoneOffset() * 60_000).toISOString().slice(0, 16);
const parseLocalDateTime = (value: string): Date | undefined => {
    if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}$/.test(value)) return undefined;
    const date = new Date(value);
    return Number.isFinite(date.getTime()) && localDateTime(date) === value ? date : undefined;
};

type TimeRange = { from: string; to: string };
interface TimeFilterProps {
    preset: TimePreset;
    range: TimeRange;
    valid: boolean;
    disabled: boolean;
    timeZone: string;
    onPresetChange: (preset: TimePreset) => void;
    onRangeChange: (range: TimeRange) => void;
}

const TimeFilter = ({ preset, range, valid, disabled, timeZone, onPresetChange, onRangeChange }: TimeFilterProps): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const id = useId();
    const title = t('batchExportCreatedRange');
    const ariaLabels = {
        from: t('batchExportCreatedFrom'),
        to: t('batchExportCreatedTo'),
    };
    return <section className="filter-section" aria-label={title}>
        <div className="section-heading"><h3>
            <span>{title}</span>
            <FilterHelp text={t('batchExportCreatedTip', { zone: timeZone })} />
        </h3></div>
        <div className="time-presets" role="group" aria-label={title}>
            {TIME_PRESET_OPTIONS.map((option) => <Button
                key={option.value}
                aria-pressed={preset === option.value}
                className={preset === option.value ? 'active' : undefined}
                disabled={disabled}
                onClick={() => { if (preset !== option.value) onPresetChange(option.value); }}
            >{t(option.key)}</Button>)}
        </div>
        {preset === 'custom' && <>
            <div className="range-fields">
                {(['from', 'to'] as const).map((field) => <label key={field} className="range-field" htmlFor={`${id}-${field}`}>
                    <span>{t(field === 'from' ? 'batchExportFrom' : 'batchExportTo')}</span>
                    <Input
                        id={`${id}-${field}`} type="datetime-local" step={60} value={range[field]} disabled={disabled}
                        aria-label={ariaLabels[field]} aria-invalid={!valid} aria-describedby={valid ? undefined : `${id}-error`}
                        onChange={(event) => onRangeChange({ ...range, [field]: event.target.value })}
                    />
                </label>)}
            </div>
            {!valid && <div role="alert" id={`${id}-error`} className="error">{t('batchExportCreatedRangeInvalid')}</div>}
        </>}
    </section>;
};

const SessionBatchExportContent = ({ agents, onOpenChange }: SessionBatchExportDialogProps): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const id = useId();
    const owners = [...new Set(agents.filter((agent) => agent.available !== false).map((agent) => agent.name))];
    const ownersKey = JSON.stringify(owners);
    const [agentIds, setAgentIds] = useState<string[] | undefined>();
    const selectedAgents = owners.filter((name) => agentIds === undefined || agentIds.includes(name));
    const allAgents = owners.length > 0 && selectedAgents.length === owners.length;
    const [createdPreset, setCreatedPreset] = useState<TimePreset>('all');
    const [createdRange, setCreatedRange] = useState(() => {
        const to = new Date();
        return { from: localDateTime(new Date(to.getTime() - 30 * DAY_MS)), to: localDateTime(to) };
    });
    const createdFrom = parseLocalDateTime(createdRange.from);
    const createdTo = parseLocalDateTime(createdRange.to);
    const validCreatedRange = Boolean(createdFrom && createdTo && createdFrom.getTime() <= createdTo.getTime());

    const validFilters = selectedAgents.length > 0 && (createdPreset !== 'custom' || validCreatedRange);
    const timeZone = Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC';
    const [keyword, setKeyword] = useState('');
    const [status, setStatus] = useState<SessionExportStatus>();
    const [selectedKeys, setSelectedKeys] = useState<string[]>([]);
    const [page, setPage] = useState(1);
    const [operation, setOperation] = useState<Operation>();
    const busy = operation !== undefined;
    const exporting = operation === 'export';
    const [error, setError] = useState('');
    const [delivery, setDelivery] = useState<{ savedPath?: string }>();
    const generation = useRef(0);
    const controller = useRef<AbortController>();
    const timer = useRef<ReturnType<typeof setTimeout>>();
    const submitting = useRef<Operation>();
    const queryOwners = useRef<string>();
    const deliveredJob = useRef<string>();

    const cancelWork = useCallback((): void => {
        generation.current += 1;
        controller.current?.abort();
        if (timer.current !== undefined) clearTimeout(timer.current);
        timer.current = undefined;
        submitting.current = undefined;
    }, []);
    useEffect(() => cancelWork, [cancelWork]);

    const discardSnapshot = useCallback((): void => {
        cancelWork();
        setOperation(undefined);
        setStatus(undefined);
        setSelectedKeys([]);
        setPage(1);
        setError('');
        setDelivery(undefined);
    }, [cancelWork]);
    useEffect(() => {
        // Discovery changes invalidate a query, but must not cancel delivery of an accepted export.
        if (submitting.current !== 'export') discardSnapshot();
    }, [ownersKey, discardSnapshot]);

    const changeFilter = (change: () => void): void => {
        if (submitting.current) return;
        discardSnapshot();
        change();
    };
    const closeDialog = (): void => {
        if (submitting.current !== 'export') onOpenChange(false);
    };

    const finishError = (cause: unknown, current: number): void => {
        if (generation.current !== current) return;
        setError(cause instanceof Error ? cause.message : String(cause));
        submitting.current = undefined;
        setOperation(undefined);
    };

    const poll = async (jobId: string, current: number, signal: AbortSignal, persist?: boolean): Promise<void> => {
        try {
            const next = await fetchSessionExportStatus(jobId, signal);
            if (generation.current !== current) return;
            if (next.jobId !== jobId) throw new Error(t('batchExportInvalidStatus'));
            setStatus(next);
            if (next.phase === 'scanning' || next.phase === 'exporting') {
                timer.current = setTimeout(() => { void poll(jobId, current, signal, persist); }, POLL_INTERVAL_MS);
                return;
            }
            if (next.phase === 'failed') {
                throw new Error(next.error || t('batchExportFailed'));
            }
            if (next.phase === 'completed' && deliveredJob.current !== jobId) {
                if (!next.document) throw new Error(t('batchExportMissingDocument'));
                if (persist && !next.document.savedPath) throw new Error(t('batchExportMissingSavedPath'));
                downloadSessionExport(next.document);
                deliveredJob.current = jobId;
                setDelivery({ savedPath: next.document.savedPath });
            }
            submitting.current = undefined;
            setOperation(undefined);
        } catch (cause) {
            finishError(cause, current);
        }
    };

    const query = async (): Promise<void> => {
        if (submitting.current || !validFilters) return;
        discardSnapshot();
        const current = generation.current;
        const abort = new AbortController();
        controller.current = abort;
        queryOwners.current = ownersKey;
        submitting.current = 'query';
        setOperation('query');
        try {
            const timePayload: SessionExportFilters = {};
            if (createdPreset === 'custom') {
                timePayload.createdFrom = createdFrom!.toISOString();
                timePayload.createdTo = createdTo!.toISOString();
            } else if (createdPreset !== 'all') {
                timePayload.createdDays = createdPreset;
            } else {
                timePayload.allTime = true;
            }

            const { jobId } = await querySessionExports({ agentIds: selectedAgents, ...timePayload, keyword }, abort.signal);
            if (generation.current !== current) return;
            await poll(jobId, current, abort.signal);
        } catch (cause) {
            finishError(cause, current);
        }
    };

    const startExport = async (selected: boolean): Promise<void> => {
        if (submitting.current || !status || !ready || !items.length || (selected && !selectedKeys.length)) return;
        cancelWork();
        const current = generation.current;
        const abort = new AbortController();
        controller.current = abort;
        const persist = shouldPersistSessionExport();
        submitting.current = 'export';
        setOperation('export');
        setError('');
        setDelivery(undefined);
        try {
            const { jobId } = await exportSessionBatch({
                jobId: status.jobId,
                ...(selected ? { keys: selectedKeys } : {}),
                persist,
            }, abort.signal);
            if (generation.current !== current) return;
            await poll(jobId, current, abort.signal, persist);
        } catch (cause) {
            finishError(cause, current);
        }
    };

    const items = status?.items ?? [];
    const ready = status?.phase === 'ready' && queryOwners.current === ownersKey;
    const pageCount = Math.max(1, Math.ceil(items.length / PAGE_SIZE));
    const visibleItems = items.slice((page - 1) * PAGE_SIZE, page * PAGE_SIZE);
    const dialogHelp = [
        t('batchExportDescription'),
        t('batchExportSnapshotTip'),
        status?.filters.createdFrom && status.filters.createdTo
            ? t('batchExportBounds', { from: utc(status.filters.createdFrom), to: utc(status.filters.createdTo) })
            : undefined,
    ].filter(Boolean).join('\n');

    return <Container>
        <Modal
            open
            title={<span className="dialog-title">
                <DownloadOutlined aria-hidden="true" /><span>{t('advancedSessionExport')}</span>
                <FilterHelp text={dialogHelp} label={t('batchExportHelp')} />
            </span>}
            width={640}
            style={{ maxWidth: 'calc(100vw - 24px)' }}
            centered
            getContainer={false}
            onCancel={closeDialog}
            closable={!exporting}
            keyboard={!exporting}
            maskClosable={!exporting}
            closeIcon={<span aria-label={t('batchExportClose')}>×</span>}
            footer={[
                <div key="status" className="export-status" role="status" aria-live="polite">
                    {busy && (!status || status.phase === 'ready') ? t(exporting ? 'batchExportExporting' : 'batchExportWaiting') : null}
                    {busy && status && (status.phase === 'scanning' || status.phase === 'exporting') ? <>
                        {t(status.phase === 'scanning' ? 'batchExportScanning' : 'batchExportExporting')}{' '}
                        {status.total === null ? t('batchExportProgressUnknown', { processed: status.processed }) : t('batchExportProgress', { processed: status.processed, total: status.total })}
                    </> : null}
                    {delivery ? (delivery.savedPath ? <div className="export-saved">
                        <span className="saved-label">{t('batchExportSavedLabel')}</span>{' '}
                        <Tooltip title={delivery.savedPath} trigger={['hover', 'focus']}>
                            <span className="saved-path" tabIndex={0}>{delivery.savedPath}</span>
                        </Tooltip>
                    </div> : t('batchExportDownloadDispatched')) : null}
                </div>,
                error ? <div key="error" role="alert" className="error">{error}</div> : null,
                <Button key="cancel" disabled={exporting} onClick={closeDialog}>
                    {delivery ? t('batchExportDone') : t('cancel')}
                </Button>,
                <Button
                    key="selected"
                    disabled={busy || !ready || !selectedKeys.length}
                    onClick={() => { void startExport(true); }}
                >
                    {t('batchExportSelected', { count: selectedKeys.length })}
                </Button>,
                <Button
                    key="all"
                    type="primary"
                    disabled={busy || !ready || !items.length}
                    onClick={() => { void startExport(false); }}
                >
                    {t('batchExportAll', { count: items.length })}
                </Button>,
            ]}
        >
            <div className="export-body">
                <section className="filter-section" aria-label={t('batchExportOwners')}>
                    <div className="section-heading">
                        <h3>
                            <span id={`${id}-agents-heading`}>{t('batchExportOwners')}</span>
                        </h3>
                        <div className="agent-actions">
                             <span className="agent-count">{t('batchExportAgentCount', { selected: selectedAgents.length, total: owners.length })}</span>
                             <Button type="link" disabled={busy || !owners.length || allAgents} onClick={() => changeFilter(() => setAgentIds(undefined))}>{t('batchExportAllAgents')}</Button>
                             <Button type="link" disabled={busy || !selectedAgents.length} onClick={() => changeFilter(() => setAgentIds([]))}>{t('batchExportClearAgents')}</Button>
                        </div>
                    </div>
                    {owners.length > 0 && <div className="agent-grid" role="group" aria-labelledby={`${id}-agents-heading`} aria-describedby={selectedAgents.length ? undefined : `${id}-agents-error`}>
                        {owners.map((name) => <Checkbox
                            key={name}
                            className="agent-option"
                            disabled={busy}
                            checked={selectedAgents.includes(name)}
                            onChange={(event) => changeFilter(() => {
                                const next = event.target.checked ? [...selectedAgents, name] : selectedAgents.filter((owner) => owner !== name);
                                setAgentIds(next.length === owners.length ? undefined : next);
                            })}
                        >
                            <span className="agent-identity"><AgentLogo name={name} /><span className="agent-name" title={name}>{name}</span></span>
                        </Checkbox>)}
                    </div>}
                    {!selectedAgents.length && <div id={`${id}-agents-error`} role="alert" className="error">{t(owners.length ? 'batchExportAgentsRequired' : 'batchExportNoAgents')}</div>}
                </section>
                <div className="time-filters">
                    <TimeFilter preset={createdPreset} range={createdRange} valid={validCreatedRange} disabled={busy} timeZone={timeZone}
                        onPresetChange={(value) => changeFilter(() => setCreatedPreset(value))} onRangeChange={(value) => changeFilter(() => setCreatedRange(value))} />
                </div>
                <section className="filter-section" aria-label={t('batchExportKeyword')}>
                    <div className="section-heading">
                        <h3>
                            <label htmlFor={`${id}-keyword`}>{t('batchExportKeyword')}</label>
                            <FilterHelp text={t('batchExportTitleTip')} />
                        </h3>
                    </div>
                    <Input id={`${id}-keyword`} value={keyword} disabled={busy} allowClear maxLength={512} placeholder={t('batchExportKeywordPlaceholder')} onChange={(event) => changeFilter(() => setKeyword(event.target.value))} />
                </section>
                <div className="filter-action-bar">
                    <Button type="primary" className="query-button" icon={<SearchOutlined aria-hidden="true" />} disabled={busy || !validFilters} onClick={() => { void query(); }}>
                        {t('batchExportQuery')}
                    </Button>
                </div>
                {ready && !items.length && <div className="results">
                    <p className="results-heading">{t('batchExportResultCount', { count: 0 })}</p>
                    <p className="pagination-hint">{t(status.issues.length ? 'batchExportNoExportable' : 'batchExportNoMatches')}</p>
                </div>}
                {status && items.length > 0 && <section className="results" aria-label={t('batchExportResultCount', { count: items.length })}>
                    <div className="results-heading">{t('batchExportResultCount', { count: items.length })}</div>
                    <ul className="result-list">
                        {visibleItems.map((item) => <li className="result-row" key={item.key}>
                            <Checkbox className="result-option" aria-label={t('batchExportSelectSession', { title: item.title || item.sessionId, agent: item.agentId })} disabled={!ready || busy} checked={selectedKeys.includes(item.key)} onChange={() => setSelectedKeys((keys) => keys.includes(item.key) ? keys.filter((key) => key !== item.key) : [...keys, item.key])}>
                                <span className="result-title" title={item.sessionId}>{item.title || item.sessionId}</span>
                                <span className="result-meta">
                                    <span className="result-agent"><AgentLogo name={item.agentId} />{item.agentId}</span>
                                    {item.createdAt && <time dateTime={item.createdAt} title={t('batchExportCreatedAt')}>{t('batchExportCreatedRange')}: {utc(item.createdAt)}</time>}
                                    <time dateTime={item.updatedAt} title={t('batchExportUpdatedAt')}>{t('batchExportUpdatedRange')}: {utc(item.updatedAt)}</time>
                                </span>
                            </Checkbox>
                        </li>)}
                    </ul>
                    {pageCount > 1 && <nav className="pagination" aria-label={t('batchExportPages')}>
                        <Button disabled={page === 1} onClick={() => setPage((value) => value - 1)}>{t('batchExportPrevious')}</Button>
                        <span className="pagination-hint">{t('batchExportPage', { page, total: pageCount })}</span>
                        <Button disabled={page >= pageCount} onClick={() => setPage((value) => value + 1)}>{t('batchExportNext')}</Button>
                    </nav>}
                </section>}
                {Boolean(status?.issues.length) && <section className="issues" aria-label={t('batchExportIssues')}>
                    <strong>{t('batchExportIssues')}</strong>
                    <ul>{status?.issues.map((issue, index) => <li key={`${issue.agentId}:${issue.sessionId ?? ''}:${issue.code}:${index}`}>
                        {issue.agentId}{issue.sessionId ? ` / ${issue.sessionId}` : ''} — {issue.code}: {issue.message}
                    </li>)}</ul>
                </section>}
            </div>
        </Modal>
    </Container>;
};
