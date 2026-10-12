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
import React, { useEffect, useRef, type RefObject } from 'react';
import { useTheme } from '@emotion/react';
import { createWelcomeDotsEffect, type WelcomeDotsEffect } from './welcomeDotsEffect';

interface WelcomeDotsProps {
    hitAreaRef: RefObject<HTMLElement>;
    replayKey: string;
    className?: string;
}

export const WelcomeDots = ({ hitAreaRef, replayKey, className }: WelcomeDotsProps): JSX.Element => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const effectRef = useRef<WelcomeDotsEffect>();
    const { mode } = useTheme();

    useEffect(() => {
        const canvas = canvasRef.current;
        const hitArea = hitAreaRef.current;
        if (!canvas || !hitArea) return;
        const effect = createWelcomeDotsEffect(canvas, hitArea);
        effectRef.current = effect;
        return () => {
            effect?.destroy();
            effectRef.current = undefined;
        };
    }, [hitAreaRef]);

    useEffect(() => {
        effectRef.current?.setDarkMode(mode === 'dark');
    }, [mode]);

    useEffect(() => {
        effectRef.current?.ripple();
    }, [replayKey]);

    return <canvas ref={canvasRef} className={className} aria-hidden="true" style={{ pointerEvents: 'none' }} />;
};
