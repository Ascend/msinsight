/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
export interface OverviewTransition {
    close: (onClosed: () => void) => void;
    dispose: () => void;
}

// Keep a shared strip surface throughout the morph; only blend in the dialog near the end.
export const animateOverviewOpening = (dialog: HTMLDialogElement, source: HTMLElement, origin: DOMRect): OverviewTransition => {
    const immediate: OverviewTransition = { close: done => done(), dispose: () => {} };
    if (typeof dialog.animate !== 'function') { delete dialog.dataset.entering; return immediate; }
    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    let animations: Animation[] = [];
    let surface: HTMLDivElement | undefined;
    let stripSurface: HTMLDivElement | undefined;
    let backdrop: HTMLDivElement | undefined;
    let closing = false;
    let disposed = false;
    let complete: (() => void) | undefined;
    let generation = 0;
    let startFrame: number | undefined;
    let waitingForPaint = false;
    const clear = (): void => {
        generation++;
        if (startFrame !== undefined) cancelAnimationFrame(startFrame);
        startFrame = undefined;
        waitingForPaint = false;
        animations.forEach(animation => animation.cancel());
        animations = [];
        surface?.remove();
        stripSurface?.remove();
        backdrop?.remove();
        surface = undefined;
        stripSurface = undefined;
        backdrop = undefined;
        delete dialog.dataset.opening;
        delete dialog.dataset.entering;
    };
    const start = (reverse: boolean): void => {
        const from = reverse ? source.getBoundingClientRect() : origin;
        const target = dialog.getBoundingClientRect();
        if (!from.width || !target.width || !target.height) { delete dialog.dataset.entering; complete?.(); return; }
        const navigatorElement = dialog.querySelector<HTMLElement>('[data-testid="overviewNavigator"]');
        const navigator = navigatorElement?.getBoundingClientRect() ?? target;
        const style = getComputedStyle(dialog);
        const background = style.backgroundColor;
        const border = style.borderColor;
        surface = document.createElement('div');
        surface.className = 'overview-opening-surface';
        surface.setAttribute('aria-hidden', 'true');
        surface.setAttribute('inert', '');
        stripSurface = document.createElement('div');
        stripSurface.className = 'overview-opening-strip';
        stripSurface.setAttribute('aria-hidden', 'true');
        stripSurface.setAttribute('inert', '');
        backdrop = document.createElement('div');
        backdrop.className = 'overview-opening-backdrop';
        backdrop.setAttribute('aria-hidden', 'true');
        const preview = source.cloneNode(true) as HTMLElement;
        delete preview.dataset.floating;
        preview.querySelector('.overview-expand-button')?.remove();
        preview.querySelectorAll('[id], [data-testid]').forEach(element => {
            element.removeAttribute('id');
            element.removeAttribute('data-testid');
        });
        // Preserve the exact compact layout; scale the entire surface without reflowing its children.
        Object.assign(preview.style, { width: `${from.width}px`, height: `${from.height}px` });
        const track = preview.firstElementChild as HTMLElement | null;
        Object.assign(stripSurface.style, {
            left: `${from.left}px`,
            top: `${from.top}px`,
            width: `${from.width}px`,
            height: `${from.height}px`,
            transformOrigin: '0 0',
            backgroundColor: background,
            willChange: 'transform, opacity',
        });
        stripSurface.appendChild(preview);
        const contents = Array.from(dialog.children) as HTMLElement[];
        dialog.append(backdrop, surface, stripSurface);
        if (track && source.firstElementChild) track.scrollLeft = source.firstElementChild.scrollLeft;
        dialog.dataset.opening = 'true';
        source.dataset.floating = 'true';
        const options: KeyframeAnimationOptions = { duration: reducedMotion ? 360 : 560, easing: 'cubic-bezier(0.4, 0, 0.2, 1)', fill: 'both', direction: reverse ? 'reverse' : 'normal' };
        Object.assign(surface.style, {
            left: `${target.left}px`,
            top: `${target.top}px`,
            width: `${target.width}px`,
            height: `${target.height}px`,
            transformOrigin: '0 0',
            willChange: 'transform',
            boxShadow: '0 16px 40px rgba(0,0,0,0.2)',
        });
        const shell = surface.animate([
            { transform: `translate(${from.left - target.left}px, ${from.top - target.top}px) scale(${from.width / target.width}, ${from.height / target.height})` },
            { transform: 'translate(0px, 0px) scale(1, 1)' },
        ], options);
        animations = [shell,
            backdrop.animate([{ opacity: 0 }, { opacity: 1 }], options),
            stripSurface.animate([
                { transform: 'translate(0px, 0px) scale(1, 1)' },
                { transform: `translate(${navigator.left - from.left}px, ${navigator.top - from.top}px) scale(${navigator.width / from.width}, ${navigator.height / from.height})` },
            ], options),
            stripSurface.animate([{ opacity: 1 }, { opacity: 1, offset: 0.7 }, { opacity: 0, offset: 0.95 }, { opacity: 0 }], options),
            ...contents.flatMap(element => element === navigatorElement
                ? [
                    element.animate([
                        { transformOrigin: '0 0', transform: `translate(${from.left - navigator.left}px, ${from.top - navigator.top}px) scale(${from.width / navigator.width}, ${from.height / navigator.height})` },
                        { transformOrigin: '0 0', transform: 'translate(0px, 0px) scale(1, 1)' },
                    ], options),
                    element.animate([{ opacity: 0 }, { opacity: 0, offset: 0.7 }, { opacity: 1, offset: 0.95 }, { opacity: 1 }], options),
                ]
                : [element.animate([{ opacity: 0 }, { opacity: 1, offset: 0.8 }, { opacity: 1 }], options)]),
        ];
        // An opaque shell prevents the page shining through while content crossfades.
        surface.style.backgroundColor = background;
        surface.style.borderColor = border;
        // Initial React layout and canvas drawing can be costly. Never backdate to the previous
        // document timeline tick: paint the starting frame before starting the shared clock.
        animations.forEach(animation => { animation.pause(); animation.currentTime = 0; });
        waitingForPaint = true;
        startFrame = requestAnimationFrame(() => {
            startFrame = requestAnimationFrame(() => {
                startFrame = undefined;
                waitingForPaint = false;
                animations.forEach(animation => animation.play());
            });
        });
        delete dialog.dataset.entering;
        const current = ++generation;
        void Promise.all(animations.map(animation => animation.finished)).then(() => {
            if (disposed || current !== generation) return;
            // Close while the final compact frame is still held, before removing animation styles.
            if (closing) { delete source.dataset.floating; complete?.(); }
            clear();
        }).catch(() => {});
    };
    start(false);
    return {
        close: done => {
            if (closing || disposed) return;
            closing = true;
            complete = done;
            if (waitingForPaint) { delete source.dataset.floating; complete(); clear(); return; }
            if (animations.length) animations.forEach(animation => animation.reverse());
            else start(true);
        },
        dispose: () => {
            disposed = true;
            clear();
            delete source.dataset.floating;
        },
    };
};
