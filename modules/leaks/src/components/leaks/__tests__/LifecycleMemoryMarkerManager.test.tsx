/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React from 'react';
import { ThemeProvider } from '@emotion/react';
import { fireEvent, render } from '@testing-library/react';
import { Modal } from 'antd';
import { Session } from '../../../entity/session';
import { LifecycleMemoryMarkerManager } from '../LifecycleMemoryMarkerManager';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({
        t: (key: string, options?: { count?: number; id?: number | string; marker?: string }) => {
            if (key === 'memoryMarkerCount') return `${options?.count ?? 0} markers`;
            if (key === 'linkedBlockId') return `Block ${options?.id ?? '--'}`;
            if (key === 'renameMemoryMarker') return `Rename ${options?.marker ?? ''}`;
            if (key === 'hideMemoryMarker') return `Hide ${options?.marker ?? ''}`;
            if (key === 'showMemoryMarker') return `Show ${options?.marker ?? ''}`;
            return key;
        },
    }),
}));

jest.mock('@insight/lib/utils', () => ({
    safeJSONParse: (value: string) => JSON.parse(value),
}), { virtual: true });

const theme = {
    bgColorCommon: '#fff',
    bgColorLight: '#f5f5f5',
    borderColor: '#ccc',
    borderColorLight: '#ddd',
    boxShadow: 'none',
    contentBackgroundColor: '#fff',
    iconColor: '#666',
    primaryColor: '#1677ff',
    textColorPrimary: '#111',
    textColorSecondary: '#666',
} as any;

const createSession = (): Session => {
    const session = new Session();
    session.module = 'memsnapshot';
    session.fileHash = 'snapshot';
    session.deviceId = '0';
    session.eventType = 'malloc';
    session.addLifecycleMemoryMarker(250, 'ordinary');
    session.addLifecycleMemoryMarker(500, 'middle');
    session.addLifecycleMemoryMarker(750, 'block', 'block', '#FF7A45', 202);
    return session;
};

describe('LifecycleMemoryMarkerManager', () => {
    it('shows spatial relationships and edits color or individual flags', () => {
        const session = createSession();
        const view = render(<ThemeProvider theme={theme}>
            <LifecycleMemoryMarkerManager session={session} onClose={jest.fn()} />
        </ThemeProvider>);
        expect(view.getAllByTestId('memoryMarkerManagerRow')).toHaveLength(3);
        expect(view.getByText('3 markers')).toBeTruthy();
        expect(view.getByText('Block 202')).toBeTruthy();
        expect(view.getAllByTestId('memoryMarkerManagerGap')).toHaveLength(2);
        expect(view.getAllByTestId('memoryMarkerManagerGapDirection')).toHaveLength(2);
        expect([...view.getByTestId('memoryMarkerManagerFooter').children].map(element => element.textContent)).toEqual(['clearAllMemoryMarkers', '3 markers']);

        const nameInput = view.getByRole('textbox', { name: 'Rename Flag 1' });
        expect((nameInput as HTMLInputElement).value).toBe('Flag 1');
        fireEvent.change(nameInput, { target: { value: 'Peak memory' } });
        fireEvent.blur(nameInput);
        expect(session.getLifecycleMemoryMarkers()[0].name).toBe('Peak memory');
        fireEvent.click(view.getByRole('button', { name: 'Hide Flag 2' }));
        expect(session.getLifecycleMemoryMarkers().find(marker => marker.id === 'middle')?.hidden).toBe(true);
        expect(view.getByTestId('memoryMarkerManagerGap').style.height).toBe('216px');
        expect(view.getAllByTestId('memoryMarkerManagerNode')[1].getAttribute('data-hidden')).toBe('true');
        fireEvent.click(view.getByRole('button', { name: 'Show Flag 2' }));
        expect(view.getAllByTestId('memoryMarkerManagerGap')).toHaveLength(2);

        fireEvent.change(view.getByLabelText('markerColor: Flag 1'), { target: { value: '#00aa00' } });
        expect(session.getLifecycleMemoryMarkers().find(marker => marker.id === 'ordinary')?.color).toBe('#00AA00');
        fireEvent.click(view.getByRole('button', { name: 'deleteMemoryMarker: Flag 3' }));
        expect(session.getLifecycleMemoryMarkers().map(marker => marker.id)).toEqual(['ordinary', 'middle']);
    });

    it('closes after clearing the active context and supports external dismissal', () => {
        const session = createSession();
        const onClose = jest.fn();
        const confirm = jest.spyOn(Modal, 'confirm').mockImplementation(options => {
            (options.onOk as (() => void))();
            return { destroy: jest.fn(), update: jest.fn() } as any;
        });
        const view = render(<ThemeProvider theme={theme}>
            <LifecycleMemoryMarkerManager session={session} onClose={onClose} />
        </ThemeProvider>);
        fireEvent.click(view.getByRole('button', { name: 'clearAllMemoryMarkers' }));
        expect(confirm).toHaveBeenCalledWith(expect.objectContaining({
            title: 'clearAllMemoryMarkersConfirm',
            content: 'clearAllMemoryMarkersDescription',
        }));
        expect(session.getLifecycleMemoryMarkers()).toEqual([]);
        expect(onClose).toHaveBeenCalledTimes(1);

        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onClose).toHaveBeenCalledTimes(2);
        confirm.mockRestore();
    });
});
