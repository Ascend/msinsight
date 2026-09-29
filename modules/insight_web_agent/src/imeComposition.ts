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

const IME_PROCESS_KEY_CODE = 229;

export interface ImeKeyboardLike {
    key?: string;
    keyCode?: number;
    which?: number;
    isComposing?: boolean;
    shiftKey?: boolean;
    ctrlKey?: boolean;
    metaKey?: boolean;
    altKey?: boolean;
    nativeEvent?: ImeKeyboardLike;
}

export interface ImeCompositionTracker {
    shouldIgnoreShortcut: (event: ImeKeyboardLike) => boolean;
    consumeSuppressedEnter: (event: ImeKeyboardLike) => boolean;
    onCompositionStart: () => void;
    onCompositionUpdate: () => void;
    onCompositionEnd: () => void;
    reset: () => void;
}

const readKey = (event: ImeKeyboardLike): string | undefined => event.key ?? event.nativeEvent?.key;

const readKeyCode = (event: ImeKeyboardLike): number | undefined => (
    event.keyCode ?? event.which ?? event.nativeEvent?.keyCode ?? event.nativeEvent?.which
);

export const isImeKeyEvent = (event: ImeKeyboardLike): boolean => {
    if (event.isComposing || event.nativeEvent?.isComposing) return true;
    if (readKey(event) === 'Process') return true;
    return readKeyCode(event) === IME_PROCESS_KEY_CODE;
};

const isUnmodifiedEnter = (event: ImeKeyboardLike): boolean => (
    readKey(event) === 'Enter' && !event.shiftKey && !event.ctrlKey && !event.metaKey && !event.altKey
);

export const createImeCompositionTracker = (): ImeCompositionTracker => {
    let composing = false;
    let swallowNextEnter = false;

    const reset = (): void => {
        composing = false;
        swallowNextEnter = false;
    };

    return {
        reset,
        onCompositionStart: (): void => {
            composing = true;
        },
        onCompositionUpdate: (): void => {
            composing = true;
        },
        onCompositionEnd: (): void => {
            composing = false;
            // Mac Chromium may dispatch a leftover Enter in this same turn.
            // A later intentional Enter (space/click/Esc confirm) must still send.
            swallowNextEnter = true;
            queueMicrotask(() => {
                swallowNextEnter = false;
            });
        },
        shouldIgnoreShortcut: (event: ImeKeyboardLike): boolean => composing || isImeKeyEvent(event),
        consumeSuppressedEnter: (event: ImeKeyboardLike): boolean => {
            if (!isUnmodifiedEnter(event) || !swallowNextEnter) return false;
            swallowNextEnter = false;
            return true;
        },
    };
};
