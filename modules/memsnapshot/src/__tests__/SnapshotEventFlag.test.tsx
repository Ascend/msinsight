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
import React from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { runInAction } from 'mobx';
import { createSession } from '../application/session';
import { SnapshotEventFlag } from '../frameTree/SnapshotEventFlag';

jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
const theme = { bgColorCommon: '#fff', bgColorLight: '#f5f5f5', borderColorLight: '#ddd', primaryColor: '#0074d9' } as any;
const sessionForAxis = (): ReturnType<typeof createSession> => {
    const session = createSession();
    runInAction(() => {
        session.deviceId = '0';
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 100;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 200;
        session.leaksWorkerInfo.renderOptions.viewport = { width: 1000, height: 480 };
        session.leaksWorkerInfo.renderOptions.zoom.x = 10;
        session.leaksWorkerInfo.renderOptions.transform = { x: -200, y: 0, scaleX: 2, scaleY: 1 };
    });
    return session;
};
const bounds = (width = 1000): DOMRect => ({
    left: 0, top: 0, right: width, bottom: 30, width, height: 30, x: 0, y: 0, toJSON: () => ({}),
});

it('colors flags and vertical lines together and restores hidden markers without changing their positions', () => {
    const content = (hidden: boolean): JSX.Element => <ThemeProvider theme={theme}>
        <SnapshotEventFlag session={sessionForAxis()} markers={[
            { id: 125, eventId: 125, coordinate: 125, label: 'Checkpoint', color: '#2FA87C', hidden },
            { id: 145, eventId: 145, coordinate: 145, label: 'Other' },
        ]} onSelect={jest.fn()} />
    </ThemeProvider>;
    const view = render(content(false));
    const flag = (): HTMLElement => view.container.querySelector('[data-time-marker="125"] [data-time-marker-select]') as HTMLElement;
    const line = (): HTMLElement => view.container.querySelector('[data-testid="snapshot-event-flag"][data-marker-id="125"]') as HTMLElement;
    expect(flag().style.color).toBe('rgb(47, 168, 124)');
    expect(line().style.color).toBe('rgb(47, 168, 124)');
    expect(line().style.left).toBe('300px');
    view.rerender(content(true));
    expect(view.getAllByTestId('snapshot-event-flag')).toHaveLength(1);
    expect(view.container.querySelector('[data-time-marker="125"]')).toBeNull();
    expect(view.getByTestId('snapshot-event-marker-axis')).toBeTruthy();
    view.rerender(content(false));
    expect(view.getAllByTestId('snapshot-event-flag')).toHaveLength(2);
    expect(line().style.left).toBe('300px');
    expect(flag().style.color).toBe('rgb(47, 168, 124)');
});

it('previews without committing and retains multiple independently selectable event markers', () => {
    const session = sessionForAxis();
    const select = jest.fn();
    const locate = jest.fn();
    const content = (eventIds: number[] = []): JSX.Element => <ThemeProvider theme={theme}>
        <SnapshotEventFlag session={session} markers={eventIds.map(eventId => ({
            id: eventId, eventId, coordinate: eventId, label: `Event ${eventId}`,
        }))} onSelect={select} onActivate={locate} />
    </ThemeProvider>;
    const view = render(content());
    const axis = view.getByRole('button', { name: 'frameTree.selectEventFromAxis' });
    jest.spyOn(axis, 'getBoundingClientRect').mockReturnValue(bounds());
    fireEvent.mouseMove(axis, { clientX: 300 });
    expect(view.getByTestId('snapshot-event-preview')).toBeTruthy();
    expect(select).not.toHaveBeenCalled();
    expect(view.queryByTestId('snapshot-event-flag')).toBeNull();
    fireEvent.click(axis, { clientX: 300, detail: 1 });
    expect(select).toHaveBeenLastCalledWith(125);
    view.rerender(content([125]));
    expect(view.getAllByTestId('snapshot-event-flag')).toHaveLength(1);
    expect(view.getByTestId('snapshot-event-flag').style.left).toBe('300px');
    fireEvent.click(axis, { clientX: 700, detail: 1 });
    expect(select).toHaveBeenLastCalledWith(145);
    view.rerender(content([125, 145]));
    expect(view.getAllByTestId('snapshot-event-flag').map(flag => flag.style.left)).toEqual(['300px', '700px']);
    fireEvent.click(view.getByRole('button', { name: /Event 125/ }));
    expect(locate).toHaveBeenCalledWith(125);
    fireEvent.mouseMove(axis, { clientX: 300 });
    expect(view.queryByTestId('snapshot-event-preview')).toBeNull();
    expect(view.container.querySelectorAll('svg')).toHaveLength(2);
    expect(select).toHaveBeenCalledTimes(2);
});

it('projects scaled marker surfaces and keyboard activation through the current viewport', () => {
    const session = sessionForAxis();
    const select = jest.fn();
    const view = render(<ThemeProvider theme={theme}><SnapshotEventFlag session={session} onSelect={select} /></ThemeProvider>);
    const axis = view.getByRole('button', { name: 'frameTree.selectEventFromAxis' });
    jest.spyOn(axis, 'getBoundingClientRect').mockReturnValue(bounds(500));
    fireEvent.click(axis, { clientX: 250, detail: 1 });
    expect(select).toHaveBeenLastCalledWith(135);
    fireEvent.click(axis, { detail: 0 });
    expect(select).toHaveBeenLastCalledWith(135);
    expect(select).toHaveBeenCalledTimes(2);
});

it('deletes only the targeted marker without selecting a point or activating a tab', () => {
    const select = jest.fn();
    const locate = jest.fn();
    const remove = jest.fn();
    const view = render(<ThemeProvider theme={theme}><SnapshotEventFlag session={sessionForAxis()}
        markers={[{ id: 125, eventId: 125, coordinate: 125, label: 'Checkpoint' }]}
        onSelect={select} onActivate={locate} onRemove={remove} /></ThemeProvider>);
    fireEvent.click(view.getByLabelText('frameTree.removeMarker'));
    expect(remove).toHaveBeenCalledWith(125);
    expect(select).not.toHaveBeenCalled();
    expect(locate).not.toHaveBeenCalled();
});

it('invalidates transient previews on pan and keeps the selection strip when the selected event is offscreen', () => {
    const session = sessionForAxis();
    const select = jest.fn();
    const view = render(<ThemeProvider theme={theme}><SnapshotEventFlag session={session}
        markers={[{ id: 1, eventId: 135, coordinate: 135, label: 'Event 135' }]} onSelect={select} /></ThemeProvider>);
    const axis = view.getByRole('button', { name: 'frameTree.selectEventFromAxis' });
    jest.spyOn(axis, 'getBoundingClientRect').mockReturnValue(bounds());
    fireEvent.mouseMove(axis, { clientX: 300 });
    expect(view.getByTestId('snapshot-event-preview')).toBeTruthy();
    act(() => runInAction(() => { session.leaksWorkerInfo.renderOptions.transform.x = 1000; }));
    expect(view.queryByTestId('snapshot-event-preview')).toBeNull();
    expect(view.queryByTestId('snapshot-event-flag')).toBeNull();
    expect(view.getByRole('button', { name: 'frameTree.selectEventFromAxis' })).toBeTruthy();
    fireEvent.click(axis, { clientX: 0, detail: 1 });
    expect(select).not.toHaveBeenCalled();
});
