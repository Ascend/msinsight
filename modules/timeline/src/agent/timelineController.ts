/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { runInAction } from 'mobx';
import { clamp } from 'lodash';
import {
    COMMAND_ERROR_CODES,
    CommandError,
    type CommandDefinition,
    type CommandHandler,
    type JsonObject,
    type JsonValue,
    type ObservationData,
} from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { actionPanLeft, actionPanRight } from '../actions/actionPan';
import { actionFitToScreen } from '../actions/actionFitToScreen';
import { actionZoomIntoSelection } from '../actions/actionZoomIntoSelection';
import { resetZoom, undoZoom } from '../actions/zoomHistory';
import type { Action } from '../actions/types';
import { MAX_ZOOM_DURATION, PAN_RATE, type DomainRange } from '../entity/domain';
import { isValidSession, type SelectedDataType, type Session } from '../entity/session';
import type { InsightUnit } from '../entity/insight';

const MODULE_ID = 'Timeline';
const UNIT_SUMMARY_LIMIT = 10;
const MAX_PERCENTAGE_ZOOM_STEPS = 256;
const ZOOM_DIRECTIONS = ['in', 'out'] as const;
const PAN_DIRECTIONS = ['left', 'right'] as const;
const RESTORE_MODES = ['undo', 'reset'] as const;
const FOCUS_TARGETS = ['operator', 'selection'] as const;

type ZoomDirection = typeof ZOOM_DIRECTIONS[number];
type PanDirection = typeof PAN_DIRECTIONS[number];

let activeSession: Session | undefined;

export const setTimelineAgentSession = (session: Session | undefined): void => {
    activeSession = session;
};

export const observeTimeline = (): ObservationData => {
    const observedAt = Date.now();
    if (!isValidSession(activeSession)) {
        return {
            moduleId: MODULE_ID,
            observedAt,
            available: false,
            ...(activeSession === undefined ? {} : { phase: activeSession.phase }),
        };
    }

    const session = activeSession;
    return {
        moduleId: MODULE_ID,
        observedAt,
        available: true,
        phase: session.phase,
        timeUnit: session.isNsMode ? 'ns' : 'ms',
        dataComposition: {
            hasFtraceData: session.hasFtraceData,
            hasNonFtraceData: session.hasNonFtraceData,
        },
        viewport: viewportOf(session),
        selection: {
            range: session.selectedRange === undefined ? null : [...session.selectedRange],
            operator: summarizeOperator(session.selectedData),
            units: summarizeUnits(session),
        },
        navigation: availableNavigation(session),
    };
};

export const registerTimelineCommands = (client: ModuleAgentCommandClient): (() => void) => {
    const unregister = [
        client.registerCommand({
            name: 'Timeline.zoom',
            title: 'Zoom timeline view',
            description: 'Zoom one step with direction, or apply the equivalent bounded W/S zoom factor to approximately show a percentage of the entire timeline. Smaller percentages zoom in further. Uses the selection center when present, otherwise the viewport center. Supply exactly one of direction or percentage.',
            inputSchema: {
                type: 'object',
                properties: {
                    direction: { type: 'string', enum: [...ZOOM_DIRECTIONS] },
                    percentage: {
                        type: 'number',
                        minimum: 1,
                        maximum: 90,
                        description: 'Target viewport duration as a percentage of the full timeline (endTimeAll), not of the current viewport. For example, 10 shows approximately one tenth of the timeline.',
                    },
                },
                oneOf: [{ required: ['direction'] }, { required: ['percentage'] }],
                additionalProperties: false,
            },
        }, zoom),
        client.registerCommand(commandDefinition(
            'Timeline.pan',
            'Pan timeline view',
            'Pans the current timeline viewport left or right by five percent of its duration.',
            'direction',
            PAN_DIRECTIONS,
        ), pan),
        client.registerCommand(commandDefinition(
            'Timeline.restore',
            'Restore timeline view',
            'Undoes the last viewport change or resets the timeline to its initial range.',
            'mode',
            RESTORE_MODES,
        ), restore),
        client.registerCommand(commandDefinition(
            'Timeline.focus',
            'Focus timeline view',
            'Focuses the timeline on the currently selected operator or selected time range.',
            'target',
            FOCUS_TARGETS,
        ), focus),
    ];
    return () => unregister.forEach(stop => stop());
};

const zoom: CommandHandler = (args) => {
    if ('percentage' in args) {
        if (Object.keys(args).length !== 1 || typeof args.percentage !== 'number' ||
            !Number.isFinite(args.percentage) || args.percentage < 1 || args.percentage > 90) {
            throw invalid('Provide only percentage, a finite number from 1 to 90.');
        }
        return zoomToPercentage(args.percentage);
    }
    const direction = enumArgument(args, 'direction', ZOOM_DIRECTIONS);
    return navigate(session => zoomOneStep(session, direction));
};

const zoomOneStep = (session: Session, direction: ZoomDirection): void => {
    if (!canZoom(session, direction)) return;
    zoomByCount(session, direction === 'in' ? -1 : 1);
};

const zoomByCount = (session: Session, zoomCount: number): void => {
    const current = session.domainRange;
    const range = session.selectedRange ?? [current.domainStart, current.domainEnd];
    const zoomPoint = (range[0] + range[1]) / 2;
    runInAction(() => {
        session.domain.zoom = { zoomCount, zoomPoint };
    });
};

const zoomToPercentage = (percentage: number): JsonObject => {
    const session = requireSession();
    const globalDuration = session.endTimeAll;
    if (globalDuration === undefined || !Number.isFinite(globalDuration) || globalDuration <= 0) {
        throw new CommandError({
            code: COMMAND_ERROR_CODES.UNAVAILABLE,
            message: 'The full Timeline duration is not available yet.',
            retryable: true,
        });
    }
    const requestedDuration = globalDuration * (percentage / 100);
    const targetDuration = clamp(requestedDuration, session.domain.lowerBound, Math.min(session.domain.maxDuration, MAX_ZOOM_DURATION));
    const before = session.domainRange;
    const initialDuration = session.domain.duration;
    if (!Number.isFinite(initialDuration) || initialDuration <= 0) throw invalid('The current viewport duration is invalid.');
    // Choose the nearer adjacent W/S step, then execute the bounded burst synchronously so
    // the command transport is not held open while the page renders each intermediate frame.
    const count = (Math.log(targetDuration) - Math.log(initialDuration)) / Math.log(session.domain.ZOOM_RATE);
    const lower = Math.floor(count);
    const upper = Math.ceil(count);
    const distance = (steps: number): number => Math.abs(clamp(initialDuration * Math.pow(session.domain.ZOOM_RATE, steps),
        session.domain.lowerBound, Math.min(session.domain.maxDuration, MAX_ZOOM_DURATION)) - targetDuration);
    const stepCount = distance(lower) <= distance(upper) ? lower : upper;
    const appliedStepCount = Math.trunc(clamp(stepCount, -MAX_PERCENTAGE_ZOOM_STEPS, MAX_PERCENTAGE_ZOOM_STEPS));
    session.flushZoomingHistory();
    const history = session.contextMenu.zoomHistory;
    const previous = history[history.length - 1] ?? { domainStart: 0, domainEnd: globalDuration };
    if (appliedStepCount !== 0) zoomByCount(session, appliedStepCount);
    if (sameRange(before, session.domainRange)) {
        session.cancelZoomingHistory();
    } else {
        // Domain.zoom scheduled the final range through the history debounce. Replace it with
        // an atomic base/final pair so this whole burst is one undoable operation.
        session.cancelZoomingHistory();
        if (!sameRange(previous, before)) session.setZoomingHistory(before);
        session.setZoomingHistory(session.domainRange);
    }
    return {
        ...navigationResult(session, before),
        requestedPercentage: percentage,
        actualPercentage: session.domain.duration / globalDuration * 100,
        globalDuration,
        timeUnit: session.isNsMode ? 'ns' : 'ms',
        completedSteps: Math.abs(appliedStepCount),
        limitedByBounds: targetDuration !== requestedDuration,
        stepLimitReached: stepCount !== appliedStepCount,
    };
};

const pan: CommandHandler = (args) => {
    const direction = enumArgument(args, 'direction', PAN_DIRECTIONS);
    return navigate((session) => {
        const action = direction === 'left' ? actionPanLeft : actionPanRight;
        if (canPerform(action, session)) action.perform(session);
    });
};

const restore: CommandHandler = (args) => {
    const mode = enumArgument(args, 'mode', RESTORE_MODES);
    return navigate((session) => {
        if (mode === 'undo') {
            undoZoom(session);
        } else {
            resetZoom(session);
        }
    });
};

const focus: CommandHandler = (args) => {
    const target = enumArgument(args, 'target', FOCUS_TARGETS);
    return navigate((session) => {
        const action = target === 'operator' ? actionFitToScreen : actionZoomIntoSelection;
        if (canPerform(action, session)) action.perform(session);
    });
};

const navigate = (operation: (session: Session) => void): JsonObject => {
    const session = requireSession();
    session.flushZoomingHistory();
    const before = session.domainRange;
    operation(session);
    const after = session.domainRange;
    if (sameRange(before, after)) {
        session.cancelZoomingHistory();
    } else {
        session.flushZoomingHistory();
    }
    return navigationResult(session, before);
};

const commandDefinition = <T extends string>(
    name: string,
    title: string,
    description: string,
    property: string,
    values: readonly T[],
): CommandDefinition => ({
    name,
    title,
    description,
    inputSchema: {
        type: 'object',
        properties: {
            [property]: { type: 'string', enum: [...values] },
        },
        required: [property],
        additionalProperties: false,
    },
});

const enumArgument = <T extends string>(args: JsonObject, property: string, values: readonly T[]): T => {
    const keys = Object.keys(args);
    if (keys.length !== 1 || keys[0] !== property) {
        throw invalid(`Command args must contain only '${property}'.`);
    }
    const value = args[property];
    if (typeof value !== 'string' || !values.includes(value as T)) {
        throw invalid(`'${property}' must be one of: ${values.join(', ')}.`);
    }
    return value as T;
};

const requireSession = (): Session => {
    if (!isValidSession(activeSession)) {
        throw new CommandError({
            code: COMMAND_ERROR_CODES.UNAVAILABLE,
            message: 'No active Timeline analysis session is available.',
            retryable: true,
        });
    }
    return activeSession;
};

const navigationResult = (session: Session, before: DomainRange): JsonObject => {
    const viewport = viewportOf(session);
    return {
        unchanged: sameRange(before, session.domainRange),
        viewport,
        requiresObserve: true,
    };
};

const sameRange = (first: DomainRange, second: DomainRange): boolean =>
    first.domainStart === second.domainStart && first.domainEnd === second.domainEnd;

const viewportOf = (session: Session): JsonObject => {
    const { domainStart, domainEnd } = session.domainRange;
    return {
        start: domainStart,
        end: domainEnd,
        duration: domainEnd - domainStart,
    };
};

const availableNavigation = (session: Session): JsonObject => ({
    zoom: ZOOM_DIRECTIONS.filter(direction => canZoom(session, direction)),
    pan: PAN_DIRECTIONS.filter(direction => canPan(session, direction)),
    restore: session.contextMenu.zoomHistory.length === 0 ? [] : [...RESTORE_MODES],
    focus: FOCUS_TARGETS.filter(target => canPerform(target === 'operator' ? actionFitToScreen : actionZoomIntoSelection, session)),
});

const canZoom = (session: Session, direction: ZoomDirection): boolean => {
    if (direction === 'in') return !session.domain.isLowerBound;
    return session.domain.duration < Math.min(session.domain.maxDuration, MAX_ZOOM_DURATION);
};

const canPan = (session: Session, direction: PanDirection): boolean => {
    const current = session.domainRange;
    const next = pannedRange(session, direction);
    return current.domainStart !== next.domainStart || current.domainEnd !== next.domainEnd;
};

const pannedRange = (session: Session, direction: PanDirection): DomainRange => {
    const { domainStart, domainEnd } = session.domainRange;
    const duration = domainEnd - domainStart;
    const offset = (direction === 'left' ? -1 : 1) * PAN_RATE * duration;
    const newEnd = clamp(domainEnd + offset, duration, session.endTimeAll ?? session.domain.defaultDuration);
    return { domainStart: newEnd - duration, domainEnd: newEnd };
};

const canPerform = (action: Action, session: Session): boolean =>
    action.visible?.(session) !== false && action.disabled?.(session) !== true;

const summarizeOperator = (operator: SelectedDataType | undefined): JsonValue => {
    if (operator === undefined) return null;
    const summary: JsonObject = {
        name: operator.name,
        startTime: operator.startTime,
        duration: operator.duration,
        threadId: operator.threadId,
    };
    addString(summary, 'id', operator.id);
    addNumber(summary, 'depth', operator.depth);
    addString(summary, 'processId', operator.processId);
    addString(summary, 'cardId', operator.cardId);
    addString(summary, 'metaType', operator.metaType);
    return summary;
};

const summarizeUnits = (session: Session): JsonObject => ({
    count: session.selectedUnits.length,
    items: session.selectedUnits.slice(0, UNIT_SUMMARY_LIMIT).map((unit: InsightUnit) => ({
        name: unit.name,
        expanded: unit.isExpanded,
        visible: unit.isDisplay && unit.isUnitVisible && !unit.isMultiDeviceHidden && !unit.isMerged,
    })),
    truncated: session.selectedUnits.length > UNIT_SUMMARY_LIMIT,
});

const addString = (target: JsonObject, key: string, value: unknown): void => {
    if (typeof value === 'string') target[key] = value;
};

const addNumber = (target: JsonObject, key: string, value: unknown): void => {
    if (typeof value === 'number' && Number.isFinite(value)) target[key] = value;
};

const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID,
    message,
    retryable: false,
});
