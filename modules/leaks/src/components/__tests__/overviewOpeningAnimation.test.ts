/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { animateOverviewOpening } from '../overviewOpeningAnimation';

describe('overview opening animation', () => {
    const originalAnimate = HTMLElement.prototype.animate;
    const originalMatchMedia = window.matchMedia;
    const originalRequestFrame = window.requestAnimationFrame;
    const originalCancelFrame = window.cancelAnimationFrame;
    let nextFrame: FrameRequestCallback | undefined;
    const paintFrame = (): void => { const callback = nextFrame; nextFrame = undefined; callback?.(0); };
    let animations: Array<{ frames: Keyframe[]; finish: () => void; cancel: jest.Mock; reverse: jest.Mock; pause: jest.Mock; play: jest.Mock; currentTime: number | null; options: KeyframeAnimationOptions }>;
    let dialog: HTMLDialogElement;
    let source: HTMLDivElement;
    const origin = { left: 20, top: 100, width: 1000, height: 72 } as DOMRect;

    beforeEach(() => {
        animations = [];
        nextFrame = undefined;
        window.requestAnimationFrame = jest.fn(callback => { nextFrame = callback; return 1; });
        window.cancelAnimationFrame = jest.fn(() => { nextFrame = undefined; });
        window.matchMedia = jest.fn(() => ({ matches: false } as MediaQueryList));
        HTMLElement.prototype.animate = jest.fn((frames, options) => {
            let finish = (): void => {};
            const finished = new Promise<void>(resolve => { finish = resolve; });
            const cancel = jest.fn();
            const reverse = jest.fn();
            const animation = { frames: frames as Keyframe[], finish, finished, cancel, reverse, pause: jest.fn(), play: jest.fn(), currentTime: null as number | null, options: options as KeyframeAnimationOptions };
            animations.push(animation);
            return animation as unknown as Animation;
        });
        dialog = document.createElement('dialog');
        source = document.createElement('div');
        source.innerHTML = '<div><div><div><svg height="36" data-testid="curve" /></div></div></div><button class="overview-expand-button"></button>';
        source.getBoundingClientRect = () => origin;
        dialog.innerHTML = '<canvas data-testid="detail"></canvas><div data-testid="overviewNavigator"></div>';
        (dialog.lastElementChild as HTMLElement).getBoundingClientRect = () => ({ left: 74, top: 66, width: 1152, height: 86 } as DOMRect);
        dialog.getBoundingClientRect = () => ({ left: 50, top: 30, width: 1200, height: 800 } as DOMRect);
        document.body.append(source, dialog);
    });
    afterEach(() => {
        source.remove();
        dialog.remove();
        HTMLElement.prototype.animate = originalAnimate;
        window.matchMedia = originalMatchMedia;
        window.requestAnimationFrame = originalRequestFrame;
        window.cancelAnimationFrame = originalCancelFrame;
    });

    it('continuously expands the shared strip and returns it to the live source bounds on close', async () => {
        const detail = dialog.firstElementChild;
        const transition = animateOverviewOpening(dialog, source, origin);
        const surface = dialog.querySelector('.overview-opening-strip');
        expect(surface?.querySelector('svg')).not.toBeNull();
        expect(surface?.querySelector('canvas, button, [data-testid]')).toBeNull();
        expect((surface?.firstElementChild as HTMLElement).style.width).toBe('1000px');
        expect((surface?.querySelector('svg') as SVGElement).getAttribute('height')).toBe('36');
        expect(animations[0].frames).toHaveLength(2);
        expect(animations[0].options.duration).toBe(560);
        expect(animations[0].frames[0].transform).toBe(`translate(-30px, 70px) scale(${1000 / 1200}, ${72 / 800})`);
        expect(animations[0].frames[1].transform).toBe('translate(0px, 0px) scale(1, 1)');
        expect(animations[2].frames[1]).toMatchObject({ transform: `translate(54px, -34px) scale(${1152 / 1000}, ${86 / 72})` });
        expect(animations[3].frames[1]).toMatchObject({ opacity: 1, offset: 0.7 });
        // Both surfaces occupy the same moving rectangle, including halfway through the transition.
        const navigatorMotion = animations.find(animation => animation.frames[0].transformOrigin === '0 0');
        expect(navigatorMotion?.frames[0].transform).toBe(`translate(-54px, 34px) scale(${1000 / 1152}, ${72 / 86})`);
        expect(navigatorMotion?.frames[1].transform).toBe('translate(0px, 0px) scale(1, 1)');
        const navigatorReveal = animations[animations.length - 1];
        expect(navigatorReveal.frames[1]).toMatchObject({ opacity: 0, offset: 0.7 });
        expect(animations.every(animation => animation.currentTime === 0 && animation.play.mock.calls.length === 0)).toBe(true);
        paintFrame();
        expect(animations.every(animation => animation.play.mock.calls.length === 0)).toBe(true);
        paintFrame();
        expect(animations.every(animation => animation.play.mock.calls.length === 1)).toBe(true);
        animations[0].finish();
        await Promise.resolve();
        expect(dialog.querySelector('.overview-opening-surface')).not.toBeNull();
        animations.forEach(animation => animation.finish());
        await Promise.resolve();
        await Promise.resolve();
        expect(dialog.querySelector('.overview-opening-surface')).toBeNull();
        expect(source.dataset.floating).toBe('true');
        expect(dialog.firstElementChild).toBe(detail);
        const completed = jest.fn(() => { expect(dialog.dataset.opening).toBe('true'); });
        source.getBoundingClientRect = () => ({ ...origin, top: 150 } as DOMRect);
        const next = animations.length;
        transition.close(completed);
        expect(completed).not.toHaveBeenCalled();
        expect(animations[next].options.direction).toBe('reverse');
        expect(animations[next].options.duration).toBe(560);
        expect(animations[next].frames[0].transform).toBe(`translate(-30px, 120px) scale(${1000 / 1200}, ${72 / 800})`);
        paintFrame();
        paintFrame();
        animations.slice(next).forEach(animation => animation.finish());
        await Promise.resolve();
        await Promise.resolve();
        expect(completed).toHaveBeenCalledTimes(1);
        expect(source.dataset.floating).toBeUndefined();
        expect(dialog.querySelector('.overview-opening-surface')).toBeNull();
        transition.dispose();
    });

    it('reverses an unfinished opening without jumping and cleans up on unmount', async () => {
        const transition = animateOverviewOpening(dialog, source, origin);
        const completed = jest.fn();
        paintFrame();
        paintFrame();
        transition.close(completed);
        transition.close(completed);
        expect(animations.every(animation => animation.reverse.mock.calls.length === 1)).toBe(true);
        transition.dispose();
        animations.forEach(animation => animation.finish());
        await Promise.resolve();
        await Promise.resolve();
        expect(completed).not.toHaveBeenCalled();
        expect(dialog.dataset.opening).toBeUndefined();
        expect(source.dataset.floating).toBeUndefined();
        expect(dialog.querySelector('.overview-opening-surface')).toBeNull();
    });

    it('keeps the requested two-way transition brief for reduced motion', async () => {
        window.matchMedia = jest.fn(() => ({ matches: true } as MediaQueryList));
        const transition = animateOverviewOpening(dialog, source, origin);
        expect(animations[0].options.duration).toBe(360);
        const completed = jest.fn();
        paintFrame();
        paintFrame();
        transition.close(completed);
        animations.forEach(animation => animation.finish());
        await Promise.resolve();
        await Promise.resolve();
        transition.dispose();
        expect(completed).toHaveBeenCalledTimes(1);
    });

    it('cancels a close before the first painted frame without starting a stale animation', () => {
        const transition = animateOverviewOpening(dialog, source, origin);
        const completed = jest.fn();
        transition.close(completed);
        paintFrame();
        expect(completed).toHaveBeenCalledTimes(1);
        expect(animations.every(animation => animation.play.mock.calls.length === 0)).toBe(true);
        expect(source.dataset.floating).toBeUndefined();
        expect(dialog.querySelector('.overview-opening-strip')).toBeNull();
        transition.dispose();
    });
});
