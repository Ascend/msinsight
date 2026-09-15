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
interface Dot {
    x: number;
    y: number;
    alpha: number;
    distance: number;
}

export interface WelcomeDotsEffect {
    ripple: () => void;
    setDarkMode: (dark: boolean) => void;
    destroy: () => void;
}

const GAP = 10;
const RADIUS = 2;
const RIPPLE_DELAY = 200;
const RIPPLE_DURATION = 1500;
const RIPPLE_BAND = 110;
const HOVER_RADIUS = 130;
const EDGE_FADE_EXPONENT = 2;
const BASE_COLOR = [99, 102, 241];
const HOT_COLOR = [123, 37, 244];

export function createWelcomeDotsEffect(canvas: HTMLCanvasElement, hitArea: HTMLElement): WelcomeDotsEffect | undefined {
    const context = canvas.getContext('2d');
    if (!context) return undefined;
    const ctx = context;
    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
    const hoverMedia = window.matchMedia('(hover: hover)');
    let dots: Dot[] = [];
    let width = 0;
    let height = 0;
    let dpr = 0;
    let maxDistance = 0;
    let themeAlpha = 0.08;
    let rippleStart: number | undefined;
    let rippleTimer: number | undefined;
    let hover = 0;
    let hoverTarget = 0;
    let pointerX = 0;
    let pointerY = 0;
    let raf = 0;
    let visible = true;
    let destroyed = false;

    function draw(now: number): void {
        ctx.clearRect(0, 0, width, height);
        let waveRadius = -1;
        let waveFade = 0;
        if (rippleStart !== undefined) {
            const progress = Math.min(1, Math.max(0, (now - rippleStart) / RIPPLE_DURATION));
            waveRadius = (1 - (1 - progress) ** 3) * (maxDistance + RIPPLE_BAND);
            waveFade = 1 - progress ** 2;
            if (progress === 1) rippleStart = undefined;
        }
        for (const dot of dots) {
            let light = 0;
            const waveDistance = Math.abs(dot.distance - waveRadius);
            if (waveRadius >= 0 && waveDistance < RIPPLE_BAND) {
                light = Math.cos(waveDistance / RIPPLE_BAND * Math.PI / 2) * waveFade;
            }
            if (hover > 0) {
                const proximity = Math.max(0, 1 - Math.hypot(dot.x - pointerX, dot.y - pointerY) / HOVER_RADIUS);
                light = Math.min(1, light + proximity ** 2 * hover);
            }
            if (light <= 0.01) light = 0;
            const alpha = Math.min(0.9, themeAlpha * dot.alpha * (1 + light * 1.6));
            // Keep faint highlighted dots so the pointer can reveal the faded edge.
            if (!light && alpha < 0.004) continue;
            const color = BASE_COLOR.map((base, index) => Math.round(base + (HOT_COLOR[index] - base) * light));
            ctx.fillStyle = `rgba(${color.join(',')},${alpha})`;
            ctx.beginPath();
            ctx.arc(dot.x, dot.y, RADIUS * (1 + light * 0.4), 0, Math.PI * 2);
            ctx.fill();
        }
    }

    function schedule(): void {
        if (!raf && !destroyed && visible && !document.hidden && width && height) {
            raf = requestAnimationFrame(frame);
        }
    }

    function frame(now: number): void {
        raf = 0;
        if (destroyed) return;
        hover += (hoverTarget - hover) * 0.12;
        if (Math.abs(hoverTarget - hover) < 0.005) hover = hoverTarget;
        draw(now);
        // A settled hover is a static image; pointer movement schedules the next frame.
        if (rippleStart !== undefined || hover !== hoverTarget) schedule();
    }

    function stop(): void {
        window.clearTimeout(rippleTimer);
        rippleTimer = undefined;
        cancelAnimationFrame(raf);
        raf = 0;
        rippleStart = undefined;
        hover = 0;
        hoverTarget = 0;
    }

    function ripple(): void {
        if (destroyed || !visible || document.hidden || !width || !height) return;
        window.clearTimeout(rippleTimer);
        rippleTimer = undefined;
        rippleStart = undefined;
        draw(performance.now());
        if (reducedMotion.matches) return;
        rippleTimer = window.setTimeout(() => {
            rippleTimer = undefined;
            if (destroyed || !visible || document.hidden || reducedMotion.matches || !width || !height) return;
            rippleStart = performance.now();
            schedule();
        }, RIPPLE_DELAY);
    }

    function resize(): void {
        if (destroyed) return;
        const rect = canvas.getBoundingClientRect();
        const nextWidth = Math.round(rect.width);
        const nextHeight = Math.round(rect.height);
        const nextDpr = Math.min(window.devicePixelRatio || 1, 2);
        if (width === nextWidth && height === nextHeight && dpr === nextDpr) return;
        const wasEmpty = !width || !height;
        width = nextWidth;
        height = nextHeight;
        dpr = nextDpr;
        canvas.width = Math.round(width * dpr);
        canvas.height = Math.round(height * dpr);
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        dots = [];
        if (!width || !height) {
            stop();
            return;
        }
        const radiusX = width * 0.52;
        const radiusY = height * 0.42;
        maxDistance = Math.hypot(radiusX, radiusY);
        for (let y = (height % GAP) / 2; y < height; y += GAP) {
            for (let x = (width % GAP) / 2; x < width; x += GAP) {
                const distance = Math.hypot((x - width / 2) / radiusX, (y - height / 2) / radiusY);
                if (distance >= 1) continue;
                dots.push({
                    x,
                    y,
                    alpha: (1 - distance) ** EDGE_FADE_EXPONENT * (0.72 + Math.random() * 0.28),
                    distance: Math.hypot(x - width / 2, y - height / 2),
                });
            }
        }
        draw(performance.now());
        if (wasEmpty) ripple();
    }

    function onPointerMove(event: PointerEvent): void {
        if (event.pointerType === 'touch' || !hoverMedia.matches || reducedMotion.matches) return;
        const rect = canvas.getBoundingClientRect();
        pointerX = event.clientX - rect.left;
        pointerY = event.clientY - rect.top;
        hoverTarget = 1;
        schedule();
    }

    function onPointerLeave(): void {
        if (!hover && !hoverTarget) return;
        hoverTarget = 0;
        schedule();
    }

    function onPreferencesChange(): void {
        stop();
        draw(performance.now());
    }

    function onVisibilityChange(): void {
        if (document.hidden) stop();
        else ripple();
    }

    const resizeObserver = new ResizeObserver(resize);
    // Intersection visibility also detects an ancestor iframe hidden by the host drawer.
    const intersectionObserver = new IntersectionObserver(([entry]) => {
        if (destroyed || visible === entry.isIntersecting) return;
        visible = entry.isIntersecting;
        if (visible) ripple();
        else stop();
    });
    hitArea.addEventListener('pointermove', onPointerMove);
    hitArea.addEventListener('pointerleave', onPointerLeave);
    hitArea.addEventListener('pointercancel', onPointerLeave);
    reducedMotion.addEventListener('change', onPreferencesChange);
    hoverMedia.addEventListener('change', onPreferencesChange);
    window.addEventListener('resize', resize);
    document.addEventListener('visibilitychange', onVisibilityChange);
    resizeObserver.observe(canvas);
    intersectionObserver.observe(canvas);
    resize();

    return {
        ripple,
        setDarkMode(dark): void {
            if (destroyed) return;
            themeAlpha = dark ? 0.22 : 0.08;
            draw(performance.now());
        },
        destroy(): void {
            destroyed = true;
            stop();
            resizeObserver.disconnect();
            intersectionObserver.disconnect();
            hitArea.removeEventListener('pointermove', onPointerMove);
            hitArea.removeEventListener('pointerleave', onPointerLeave);
            hitArea.removeEventListener('pointercancel', onPointerLeave);
            reducedMotion.removeEventListener('change', onPreferencesChange);
            hoverMedia.removeEventListener('change', onPreferencesChange);
            window.removeEventListener('resize', resize);
            document.removeEventListener('visibilitychange', onVisibilityChange);
        },
    };
}
