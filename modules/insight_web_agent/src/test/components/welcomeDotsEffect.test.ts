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
import { createWelcomeDotsEffect } from '../../components/welcomeDotsEffect';

let frames: Map<number, FrameRequestCallback>;
let clock: number;
let nextFrame: number;
let resizeCallback: () => void;
let intersect: (visible: boolean) => void;
let canvas: HTMLCanvasElement;
let area: HTMLDivElement;
let context: CanvasRenderingContext2D;
let effect: ReturnType<typeof createWelcomeDotsEffect>;
let reducedMotion: MediaQueryList;
let hoverMedia: MediaQueryList;
let resizeDisconnect: jest.Mock;
let intersectionDisconnect: jest.Mock;
const originalResizeObserver = globalThis.ResizeObserver;
const originalIntersectionObserver = globalThis.IntersectionObserver;
const originalMatchMedia = window.matchMedia;
const originalDpr = window.devicePixelRatio;

function media(matches: boolean): MediaQueryList {
    const target = new EventTarget();
    return Object.assign(target, { matches }) as MediaQueryList;
}

function flushFrame(elapsed = 16): void {
    clock += elapsed;
    jest.advanceTimersByTime(elapsed);
    const pending = [...frames.values()];
    frames.clear();
    pending.forEach((callback) => callback(clock));
}

function settle(): void {
    for (let step = 0; (frames.size || jest.getTimerCount()) && step < 200; step += 1) flushFrame();
    expect(frames.size).toBe(0);
    expect(jest.getTimerCount()).toBe(0);
}

function move(pointerType = 'mouse'): void {
    area.dispatchEvent(Object.assign(new Event('pointermove'), { pointerType, clientX: 200, clientY: 220 }));
}

beforeEach(() => {
    jest.useFakeTimers();
    frames = new Map();
    clock = 0;
    nextFrame = 0;
    reducedMotion = media(false);
    hoverMedia = media(true);
    window.matchMedia = jest.fn((query) => query.includes('reduced-motion') ? reducedMotion : hoverMedia);
    jest.spyOn(performance, 'now').mockImplementation(() => clock);
    jest.spyOn(window, 'requestAnimationFrame').mockImplementation((callback) => {
        frames.set(++nextFrame, callback);
        return nextFrame;
    });
    jest.spyOn(window, 'cancelAnimationFrame').mockImplementation((id) => { frames.delete(id); });
    resizeDisconnect = jest.fn();
    intersectionDisconnect = jest.fn();
    globalThis.ResizeObserver = jest.fn((callback) => {
        resizeCallback = callback;
        return { observe: jest.fn(), disconnect: resizeDisconnect };
    }) as unknown as typeof ResizeObserver;
    globalThis.IntersectionObserver = jest.fn((callback) => {
        intersect = (visible) => callback([{ isIntersecting: visible }]);
        return { observe: jest.fn(), disconnect: intersectionDisconnect };
    }) as unknown as typeof IntersectionObserver;
    area = document.createElement('div');
    canvas = document.createElement('canvas');
    area.appendChild(canvas);
    context = {
        clearRect: jest.fn(), setTransform: jest.fn(), beginPath: jest.fn(), arc: jest.fn(), fill: jest.fn(), fillStyle: '',
    } as unknown as CanvasRenderingContext2D;
    jest.spyOn(canvas, 'getContext').mockReturnValue(context);
    jest.spyOn(canvas, 'getBoundingClientRect').mockReturnValue({ width: 361, height: 388, left: 20, top: 26 } as DOMRect);
    effect = createWelcomeDotsEffect(canvas, area);
});

afterEach(() => {
    effect?.destroy();
    jest.restoreAllMocks();
    jest.useRealTimers();
    window.matchMedia = originalMatchMedia;
    window.devicePixelRatio = originalDpr;
    globalThis.ResizeObserver = originalResizeObserver;
    globalThis.IntersectionObserver = originalIntersectionObserver;
});

test('plays a finite intro and returns to idle after hover settles or leaves', () => {
    expect(frames.size).toBe(0);
    flushFrame(199);
    expect(frames.size).toBe(0);
    expect((context.arc as jest.Mock).mock.calls.every((call) => call[2] === 2)).toBe(true);
    flushFrame(1);
    expect(frames.size).toBe(1);
    flushFrame(1500);
    expect(frames.size).toBe(0);
    move();
    settle();
    expect((context.arc as jest.Mock).mock.calls.some((call) => call[2] > 2)).toBe(true);
    (context.arc as jest.Mock).mockClear();
    area.dispatchEvent(new Event('pointerleave'));
    settle();
    (context.arc as jest.Mock).mockClear();
    effect?.setDarkMode(false);
    expect((context.arc as jest.Mock).mock.calls.every((call) => call[2] === 2)).toBe(true);
});

test('ignores touch input and stops active motion immediately when reduced motion is enabled', () => {
    settle();
    move('touch');
    expect(frames.size).toBe(0);
    move();
    expect(frames.size).toBe(1);
    Object.assign(reducedMotion, { matches: true });
    reducedMotion.dispatchEvent(new Event('change'));
    effect?.ripple();
    move();
    expect(frames.size).toBe(0);
});

test('redraws themes without regenerating texture and caps the backing resolution at 2x', () => {
    settle();
    const random = jest.spyOn(Math, 'random');
    effect?.setDarkMode(true);
    expect(random).not.toHaveBeenCalled();
    expect(frames.size).toBe(0);
    window.devicePixelRatio = 3;
    resizeCallback();
    expect(canvas.width).toBe(722);
    expect(canvas.height).toBe(776);
    expect(context.setTransform).toHaveBeenLastCalledWith(2, 0, 0, 2, 0, 0);
});

test('stops when hidden, replays on reappearance, and releases all work on destroy', () => {
    intersect(false);
    expect(frames.size).toBe(0);
    expect(jest.getTimerCount()).toBe(0);
    intersect(true);
    expect(frames.size).toBe(0);
    flushFrame(200);
    expect(frames.size).toBe(1);
    effect?.destroy();
    expect(frames.size).toBe(0);
    expect(resizeDisconnect).toHaveBeenCalled();
    expect(intersectionDisconnect).toHaveBeenCalled();
    move();
    effect?.ripple();
    resizeCallback();
    intersect(false);
    intersect(true);
    expect(frames.size).toBe(0);
});

test('cancels a delayed intro on unmount before any animation frame starts', () => {
    expect(jest.getTimerCount()).toBe(1);
    effect?.destroy();
    expect(jest.getTimerCount()).toBe(0);
    (context.clearRect as jest.Mock).mockClear();
    flushFrame(2000);
    expect(frames.size).toBe(0);
    expect(context.clearRect).not.toHaveBeenCalled();
});
