/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
import { ThemeProvider } from '@emotion/react';
// eslint-disable-next-line import/named
import { act, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { CategorySearch } from './CategorySearch';
import type { Session } from '../entity/session';
import jumpToUnitOperator from '../utils/jumpToUnitOperator';

jest.mock('../utils/jumpToUnitOperator', () => ({ __esModule: true, default: jest.fn() }));
jest.mock('mobx-react', () => ({ observer: (component: React.FC): React.FC => component }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string): string => key }) }));
jest.mock('@insight/lib/icon', () => ({ SearchIcon: (): null => null }));
jest.mock('./base/StyledButton', () => ({
    CustomButton: ({ onClick }: { onClick?: () => void }): JSX.Element => <button onClick={onClick}>Search</button>,
    StyledButton: (): null => null,
}));
jest.mock('@insight/lib/components', () => ({
    Tooltip: ({ children, title }: React.PropsWithChildren<{ title?: React.ReactNode }>): JSX.Element => <>{children}{title}</>,
    Select: (): null => null,
    Button: ({ children, onClick, disabled }: React.PropsWithChildren<{
        onClick?: () => void; disabled?: boolean;
    }>): JSX.Element => <button onClick={onClick} disabled={disabled}>{children ?? 'Page'}</button>,
    Input: ({ value, onChange, onPressEnter, disabled }: {
        value: string; onChange: React.ChangeEventHandler<HTMLInputElement>;
        onPressEnter?: () => void; disabled?: boolean;
    }): JSX.Element => <input value={value} onChange={onChange} disabled={disabled}
        onKeyDown={event => { if (event.key === 'Enter') { onPressEnter?.(); } }} />,
}));

const theme = {
    bgColorLight: '',
    textColorPrimary: '',
    buttonColor: { enableClickColor: '', disableClickColor: '', emphasizeColor: '', suspendBGColor: '', unSuspendBGColor: '' },
};
const view = (session: Session): JSX.Element => <ThemeProvider theme={theme}><CategorySearch session={session}/></ThemeProvider>;
const makeSession = (): Session => ({
    units: [{
        metadata: { cardId: '0', dbPath: 'card.db', dataSource: 'local' },
        isDisplay: true,
        children: [{
            metadata: { cardId: '0', dbPath: 'card.db', processId: '100', threadId: '1', metaType: 'TEXT' },
            isDisplay: true,
        }],
    }],
    lockUnit: [],
    unitsConfig: { offsetConfig: { timestampOffset: {} } },
    isTimeAnalysisMode: true,
    timeAnalysisRange: [10, 20],
} as unknown as Session);
const countResponse = { totalCount: 2, countList: [{ rankId: '0', dbPath: 'card.db', count: 2 }] };
const sliceResponse = { tid: '1', pid: '100', rankId: '0', dbPath: 'card.db', startTime: 12, duration: 1 };
const search = (): void => {
    fireEvent.change(screen.getAllByRole('textbox')[0], { target: { value: 'operator' } });
    fireEvent.keyDown(screen.getAllByRole('textbox')[0], { key: 'Enter' });
};

describe('time analysis search', () => {
    beforeEach(() => { jest.clearAllMocks(); });

    it('keeps the original locked selection parameters when time analysis is disabled', async () => {
        const session = makeSession();
        session.isTimeAnalysisMode = false;
        session.lockRange = [50, 80];
        session.lockUnit = [{
            metadata: { cardId: '0', dbPath: 'card.db', processId: '100', threadId: '9', metaType: 'TEXT' },
        }] as unknown as Session['lockUnit'];
        window.request = jest.fn().mockResolvedValueOnce(countResponse).mockResolvedValueOnce(sliceResponse);
        render(view(session));
        search();
        await waitFor(() => expect(jumpToUnitOperator).toHaveBeenCalledTimes(1));
        for (const [, request] of (window.request as jest.Mock).mock.calls) {
            expect(request.params.metadataList).toEqual([{
                tid: '9',
                pid: '100',
                metaType: 'TEXT',
                rankId: '0',
                dbPath: 'card.db',
                lockStartTime: 50,
                lockEndTime: 80,
            }]);
        }
    });

    it('invalidates old pagination when the analysis range changes', async () => {
        window.request = jest.fn().mockResolvedValueOnce(countResponse).mockResolvedValueOnce(sliceResponse);
        const session = makeSession();
        const result = render(view(session));
        search();
        await waitFor(() => expect(jumpToUnitOperator).toHaveBeenCalledTimes(1));
        expect(screen.getByText('Open in Find Window')).toBeInTheDocument();

        session.timeAnalysisRange = [30, 40];
        result.rerender(view(session));

        expect(screen.queryByText('Open in Find Window')).not.toBeInTheDocument();
        expect(window.request).toHaveBeenCalledTimes(2);
    });

    it('does not issue an unrestricted slice query when the target source has no tracks', async () => {
        window.request = jest.fn().mockResolvedValue({
            totalCount: 1, countList: [{ rankId: '0', dbPath: 'missing.db', count: 1 }],
        });
        render(view(makeSession()));
        search();
        await waitFor(() => expect(screen.getByText('Open in Find Window')).toBeInTheDocument());
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(jumpToUnitOperator).not.toHaveBeenCalled();
    });

    it('ignores a count response from an obsolete analysis range', async () => {
        let complete: (value: typeof countResponse) => void = () => {};
        window.request = jest.fn().mockReturnValue(new Promise(resolve => { complete = resolve; }));
        const session = makeSession();
        const result = render(view(session));
        search();
        session.timeAnalysisRange = [30, 40];
        result.rerender(view(session));
        await act(async () => { complete(countResponse); });
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(jumpToUnitOperator).not.toHaveBeenCalled();
        expect(screen.queryByText('Open in Find Window')).not.toBeInTheDocument();
    });

    it('ignores a slice response after time analysis is removed', async () => {
        let complete: (value: typeof sliceResponse) => void = () => {};
        window.request = jest.fn().mockResolvedValueOnce(countResponse)
            .mockReturnValueOnce(new Promise(resolve => { complete = resolve; }));
        const session = makeSession();
        const result = render(view(session));
        search();
        await waitFor(() => expect(window.request).toHaveBeenCalledTimes(2));
        session.isTimeAnalysisMode = false;
        result.rerender(view(session));
        await act(async () => { complete(sliceResponse); });
        expect(jumpToUnitOperator).not.toHaveBeenCalled();
        expect(screen.queryByText('Open in Find Window')).not.toBeInTheDocument();
    });

    it('does not include another rank sharing the same database in search parameters', async () => {
        const session = makeSession();
        session.units.push({
            ...session.units[0],
            metadata: { ...session.units[0].metadata, cardId: '1' },
            children: [{
                ...session.units[0].children?.[0],
                metadata: { ...session.units[0].children?.[0].metadata, cardId: '1', threadId: '2' },
            }],
        } as Session['units'][number]);
        window.request = jest.fn().mockResolvedValueOnce(countResponse).mockResolvedValueOnce(sliceResponse);
        render(view(session));
        search();
        await waitFor(() => expect(jumpToUnitOperator).toHaveBeenCalledTimes(1));
        for (const [, request] of (window.request as jest.Mock).mock.calls) {
            expect(request.params.metadataList).toHaveLength(1);
            expect(request.params.metadataList[0]).toEqual(expect.objectContaining({ rankId: '0', tid: '1' }));
        }
    });
});
