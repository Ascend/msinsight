/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { WorkspaceControlState } from './WorkspaceControlState';

describe('WorkspaceControlState', () => {
    let state: WorkspaceControlState;

    beforeEach(() => {
        jest.useFakeTimers();
        state = new WorkspaceControlState();
    });

    afterEach(() => {
        state.dispose();
        jest.useRealTimers();
    });

    test('accepts input four seconds after a short operation and fades at five seconds', () => {
        const finish = state.begin('zoom', new AbortController().signal);
        jest.advanceTimersByTime(100);
        finish();
        jest.advanceTimersByTime(4000);
        state.recordInput();
        expect(state.getSnapshot()).toMatchObject({ phase: 'input', color: 'input', executing: false });
        jest.advanceTimersByTime(999);
        expect(state.getSnapshot().phase).toBe('input');
        jest.advanceTimersByTime(1);
        expect(state.getSnapshot()).toMatchObject({ phase: 'fading', color: 'input' });
        jest.advanceTimersByTime(349);
        expect(state.getSnapshot().phase).toBe('fading');
        jest.advanceTimersByTime(1);
        expect(state.getSnapshot().phase).toBe('idle');
    });

    test('starts the five-second grace period only after the last concurrent operation ends', () => {
        const finishFirst = state.begin('zoom', new AbortController().signal);
        const second = new AbortController();
        state.begin('pan', second.signal);
        finishFirst();
        jest.advanceTimersByTime(5000);
        expect(state.getSnapshot()).toMatchObject({ phase: 'active', executing: true });

        second.abort();
        jest.advanceTimersByTime(4999);
        expect(state.getSnapshot()).toMatchObject({ phase: 'active', executing: false });
        jest.advanceTimersByTime(1);
        expect(state.getSnapshot().phase).toBe('fading');
    });

    test('resumes without fading and starts a fresh grace period when the next operation ends', () => {
        const phases: string[] = [];
        state.subscribe(() => phases.push(state.getSnapshot().phase));
        const finishFirst = state.begin('zoom', new AbortController().signal);
        jest.advanceTimersByTime(4000);
        finishFirst();
        jest.advanceTimersByTime(4000);
        const finishSecond = state.begin('pan', new AbortController().signal);
        jest.advanceTimersByTime(200);
        expect(state.getSnapshot().executing).toBe(true);
        finishSecond();
        jest.advanceTimersByTime(4999);

        expect(state.getSnapshot().phase).toBe('active');
        expect(phases).not.toContain('fading');
        expect(phases).not.toContain('idle');
        jest.advanceTimersByTime(1);
        expect(state.getSnapshot().phase).toBe('fading');
    });

    test('accepts input from the gap even if it arrives after the next operation starts', () => {
        const finish = state.begin('zoom', new AbortController().signal);
        const epoch = state.getSnapshot().epoch;
        finish();
        expect(state.getInputEpoch()).toBe(epoch);
        jest.advanceTimersByTime(200);
        const inputAt = Date.now();
        jest.advanceTimersByTime(100);
        state.begin('pan', new AbortController().signal);
        expect(state.getInputEpoch()).toBe(epoch);

        state.recordInput(epoch, inputAt);
        expect(state.getSnapshot()).toMatchObject({ phase: 'input', color: 'input', executing: true });
        jest.advanceTimersByTime(1099);
        expect(state.getSnapshot().color).toBe('input');
        jest.advanceTimersByTime(1);
        expect(state.getSnapshot()).toMatchObject({ phase: 'active', color: 'active' });
    });

    test('a new operation during the fade resumes the same window', () => {
        const finish = state.begin('zoom', new AbortController().signal);
        const epoch = state.getSnapshot().epoch;
        finish();
        jest.advanceTimersByTime(5100);
        expect(state.getSnapshot().phase).toBe('fading');

        state.begin('pan', new AbortController().signal);
        expect(state.getSnapshot()).toMatchObject({ phase: 'active', executing: true, epoch });
        jest.advanceTimersByTime(350);
        expect(state.getSnapshot()).toMatchObject({ phase: 'active', executing: true });
    });

    test('input during the fade changes color without extending or leaking into the next window', () => {
        const finish = state.begin('zoom', new AbortController().signal);
        const previousEpoch = state.getSnapshot().epoch;
        finish();
        jest.advanceTimersByTime(5300);
        expect(state.getInputEpoch()).toBe(previousEpoch);
        state.recordInput(previousEpoch);
        expect(state.getSnapshot()).toMatchObject({ phase: 'fading', color: 'input', executing: false });

        jest.advanceTimersByTime(50);
        expect(state.getInputEpoch()).toBeNull();
        state.recordInput(previousEpoch);
        expect(state.getSnapshot()).toMatchObject({ phase: 'idle', color: 'active' });
        state.begin('pan', new AbortController().signal);
        expect(state.getInputEpoch()).not.toBe(previousEpoch);
        state.recordInput(previousEpoch);
        expect(state.getSnapshot()).toMatchObject({ phase: 'active', color: 'active', executing: true });
    });
});
