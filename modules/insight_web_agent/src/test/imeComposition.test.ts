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
import { createImeCompositionTracker, isImeKeyEvent } from '../imeComposition';

test('detects IME process keys and native composing flags', () => {
    expect(isImeKeyEvent({ key: 'Enter' })).toBe(false);
    expect(isImeKeyEvent({ key: 'Enter', isComposing: true })).toBe(true);
    expect(isImeKeyEvent({ key: 'Enter', nativeEvent: { isComposing: true } })).toBe(true);
    expect(isImeKeyEvent({ key: 'Enter', keyCode: 229 })).toBe(true);
    expect(isImeKeyEvent({ key: 'Process' })).toBe(true);
});

test('ignores shortcuts while composition is active even if the key event looks like Enter', () => {
    const ime = createImeCompositionTracker();
    expect(ime.shouldIgnoreShortcut({ key: 'Enter' })).toBe(false);
    ime.onCompositionStart();
    expect(ime.shouldIgnoreShortcut({ key: 'Enter' })).toBe(true);
    ime.onCompositionUpdate();
    expect(ime.shouldIgnoreShortcut({ key: 'ArrowDown' })).toBe(true);
    ime.onCompositionEnd();
    expect(ime.shouldIgnoreShortcut({ key: 'Enter' })).toBe(false);
});

test('consumes the leftover Mac Enter that follows compositionend', () => {
    const ime = createImeCompositionTracker();
    ime.onCompositionStart();
    ime.onCompositionEnd();

    expect(ime.consumeSuppressedEnter({ key: 'Enter', shiftKey: true })).toBe(false);
    expect(ime.consumeSuppressedEnter({ key: 'Enter' })).toBe(true);
    expect(ime.consumeSuppressedEnter({ key: 'Enter' })).toBe(false);
});

test('does not consume a later Enter after composition ends without a leftover key', async () => {
    const ime = createImeCompositionTracker();
    ime.onCompositionStart();
    ime.onCompositionEnd();
    await Promise.resolve();
    expect(ime.consumeSuppressedEnter({ key: 'Enter' })).toBe(false);
});

test('reset clears composing and leftover Enter suppression', () => {
    const ime = createImeCompositionTracker();
    ime.onCompositionStart();
    expect(ime.shouldIgnoreShortcut({ key: 'ArrowDown' })).toBe(true);
    ime.onCompositionEnd();
    ime.reset();
    expect(ime.shouldIgnoreShortcut({ key: 'ArrowDown' })).toBe(false);
    expect(ime.consumeSuppressedEnter({ key: 'Enter' })).toBe(false);
});
