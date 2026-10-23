/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { COMMAND_ERROR_CODES, CommandError, type CommandContext, type CommandDefinition, type JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ChartDesc } from '../entity/insight';
import type { Session } from '../entity/session';
import { searchCategory } from '../components/CategorySearch';
import { readVisibleSlices } from './visibleSlices';

export const selectSliceDefinition: CommandDefinition = {
    name: 'Timeline.selectSlice',
    title: 'View or select a timeline operator (查看、选中算子)',
    description: 'Use when the user wants to view, inspect, locate or select a named operator, e.g. 查看 MatMul 算子. ' +
        'Pass the operator name as name, e.g. {"name":"MatMul"}. Questions about operator meaning do not imply selection. ' +
        'Selects the first matching rendered visible slice without changing zoom, scroll position or opening search. ' +
        'Only when no visible slice matches, opens and fills the existing Timeline search UI, selects its first result and updates pagination. ' +
        'Respects the search UI card and locked-range filters. Optional cardId defaults to ALL. ' +
        'Matching is case-insensitive and non-exact by default, like the search UI. No cross-project search. ' +
        'Observe after selection to verify the selected operator. Timeline.focus only frames an existing selection.',
    inputSchema: {
        type: 'object',
        properties: {
            name: { type: 'string', minLength: 1 },
            cardId: { type: 'string', minLength: 1 },
            isMatchCase: { type: 'boolean', default: false },
            isMatchExact: { type: 'boolean', default: false },
        },
        required: ['name'],
        additionalProperties: false,
    },
};

const error = (code: string, message: string): CommandError => new CommandError({ code, message, retryable: false });

export const selectSlice = async (
    args: JsonObject, context: CommandContext, session: Session, isCurrent: () => boolean,
): Promise<JsonObject> => {
    const allowed = ['name', 'cardId', 'isMatchCase', 'isMatchExact'];
    if (Object.keys(args).some(key => !allowed.includes(key)) || typeof args.name !== 'string' || !args.name.trim() ||
        ('cardId' in args && (typeof args.cardId !== 'string' || !args.cardId.trim())) ||
        ['isMatchCase', 'isMatchExact'].some(key => key in args && typeof args[key] !== 'boolean')) {
        throw error(COMMAND_ERROR_CODES.INVALID, 'Provide name with optional cardId and boolean match options.');
    }
    const units = session.units;
    const check = (): void => {
        if (context.signal.aborted) throw error(COMMAND_ERROR_CODES.CANCELLED, 'Slice selection was cancelled.');
        if (Date.now() >= context.deadline) throw error(COMMAND_ERROR_CODES.TIMEOUT, 'Slice selection timed out.');
        if (!isCurrent() || session.units !== units || session.selectedRangeIsLock) {
            throw error(COMMAND_ERROR_CODES.UNAVAILABLE, 'Timeline changed or the selection is locked. Search again.');
        }
    };
    check();
    if (session.locateUnit !== undefined) throw error(COMMAND_ERROR_CODES.BUSY, 'Another slice navigation is in progress.');
    const name = (args.name as string).trim();
    const normalize = (value: string): string => args.isMatchCase === true ? value : value.toLowerCase();
    const visible = readVisibleSlices(session).slices.find(({ source, slice }) => {
        if (args.cardId !== undefined && args.cardId !== 'ALL' && source.unit.metadata?.cardId !== args.cardId) return false;
        return args.isMatchExact === true ? normalize(slice.name) === normalize(name) : normalize(slice.name).includes(normalize(name));
    });
    if (visible) {
        // 复用画布点击路径加载详情和连线，不触发搜索或改变视口。
        visible.source.select(visible.slice);
        return { status: 'selected', requiresObserve: true };
    }
    let active = true;
    let timer: ReturnType<typeof setTimeout> | undefined;
    let abort = (): void => {};
    try {
        return await new Promise<JsonObject>((resolve, reject) => {
            abort = () => reject(error(COMMAND_ERROR_CODES.CANCELLED, 'Slice selection was cancelled.'));
            context.signal.addEventListener('abort', abort, { once: true });
            timer = setTimeout(() => reject(error(COMMAND_ERROR_CODES.TIMEOUT, 'Slice selection timed out.')), Math.max(0, context.deadline - Date.now()));
            let selected = false;
            let totalCount: number | undefined;
            const finish = (): void => { if (selected && totalCount !== undefined) resolve({ status: 'selected', totalCount, requiresObserve: true }); };
            searchCategory(session, {
                content: (args.name as string).trim(),
                rankId: (args.cardId as string | undefined) ?? 'ALL',
                isMatchCase: args.isMatchCase === true,
                isMatchExact: args.isMatchExact === true,
            }, {
                // Requests cannot be aborted; reject late responses and deferred navigation before side effects.
                isCurrent: () => { try { check(); return active; } catch { return false; } },
                onFailure: () => { try { check(); reject(error(COMMAND_ERROR_CODES.UNAVAILABLE, 'The search or owning lane is unavailable.')); } catch (err) { reject(err); } },
                onSuccess: unit => {
                    try {
                        check();
                        const chart = unit.chart as ChartDesc<'stackStatus'> | undefined;
                        chart?.onClick?.(session.selectedData, session, unit.metadata);
                        selected = true;
                        finish();
                    } catch (err) { reject(err); }
                },
            }).then(count => {
                check();
                totalCount = count;
                if (count === 0) reject(error(COMMAND_ERROR_CODES.NOT_FOUND, 'No matching operator was found.'));
                else finish();
            }).catch(reject);
        });
    } finally {
        active = false;
        clearTimeout(timer);
        context.signal.removeEventListener('abort', abort);
    }
};
