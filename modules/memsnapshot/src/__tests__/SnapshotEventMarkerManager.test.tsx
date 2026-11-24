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
import React, { useState } from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import * as testingLibrary from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { createInstance } from 'i18next';
import { I18nextProvider, initReactI18next } from 'react-i18next';
import en from '../../../lib/src/i18n/leaks/en.json';
import zh from '../../../lib/src/i18n/leaks/zh.json';
import { SnapshotEventMarkerManager } from '../frameTree/SnapshotEventMarkerManager';
import type { SnapshotEventMarker } from '../frameTree/timeline';

const { screen, waitFor } = testingLibrary;
const translations = createInstance() as React.ComponentProps<typeof I18nextProvider>['i18n'];
const theme = { primaryColor: '#0074d9', borderColorLight: '#ddd', textColorSecondary: '#666' } as any;
const close = jest.fn();
const clear = jest.fn();
const remove = jest.fn();
const Manager = (): JSX.Element => {
    const [markers, setMarkers] = useState<SnapshotEventMarker[]>([{ id: 1, coordinate: 130, eventId: 130, label: 'Checkpoint' }]);
    return <I18nextProvider i18n={translations}><ThemeProvider theme={theme}>
        <SnapshotEventMarkerManager markers={markers} onClose={close} onRemove={remove} onClear={clear}
            onUpdate={(id, updates): void => setMarkers(current => current.map(marker => marker.id === id ? { ...marker, ...updates } : marker))} />
    </ThemeProvider></I18nextProvider>;
};
beforeAll(async () => {
    await translations.use(initReactI18next).init({
        lng: 'enUS', fallbackLng: 'enUS', resources: { enUS: en, zhCN: zh }, interpolation: { escapeValue: false },
    });
});
beforeEach(async () => { await translations.changeLanguage('enUS'); });

it('commits names on Enter, blur and outside click while retaining the last valid name for empty or cancelled edits', () => {
    const view = render(<Manager />);
    const input = view.getByRole('textbox') as HTMLInputElement;
    const edit = (value: string): void => { act(() => input.focus()); fireEvent.change(input, { target: { value } }); };
    edit('  Peak memory  ');
    fireEvent.keyDown(input, { key: 'Enter' });
    expect(input.value).toBe('Peak memory');
    edit('Forward end');
    fireEvent.blur(input);
    for (const value of ['', '   ']) {
        edit(value);
        fireEvent.blur(input);
        expect(input.value).toBe('Forward end');
    }
    edit('Cancelled name');
    fireEvent.keyDown(input, { key: 'Escape' });
    expect(input.value).toBe('Forward end');
    expect(close).not.toHaveBeenCalled();
    edit('Outside saved');
    fireEvent.mouseDown(document.body);
    expect(view.getByRole('textbox', { name: 'Rename breakdown: Outside saved' })).toBeTruthy();
    expect(close).toHaveBeenCalledTimes(1);
});

it('translates an open clear confirmation in both directions and preserves cancel/confirm behavior', async () => {
    const view = render(<Manager />);
    for (const language of ['zhCN', 'enUS']) {
        fireEvent.click(view.getByRole('button', { name: translations.t('frameTree.clearMarkers', { ns: 'leaks' }) }));
        const previous = translations.t('frameTree.clearMarkersConfirm', { ns: 'leaks' });
        await screen.findByText(previous);
        await act(async () => { await translations.changeLanguage(language); });
        const content = translations.t('frameTree.clearMarkersConfirm', { ns: 'leaks' });
        await screen.findByText(content);
        expect(screen.queryByText(previous)).toBeNull();
        fireEvent.click(screen.getByRole('button', { name: /\u53d6\s*\u6d88|Cancel/ }));
        await waitFor(() => expect(screen.queryByText(content)).toBeNull());
    }
    expect(clear).not.toHaveBeenCalled();
    expect((view.getByRole('textbox') as HTMLInputElement).value).toBe('Checkpoint');
    fireEvent.click(view.getByRole('button', { name: en.leaks.frameTree.clearMarkers }));
    await screen.findByText(en.leaks.frameTree.clearMarkersConfirm);
    fireEvent.click(screen.getByRole('button', { name: en.leaks.clearAllMemoryMarkersConfirmButton }));
    await waitFor(() => expect(clear).toHaveBeenCalledTimes(1));
});
