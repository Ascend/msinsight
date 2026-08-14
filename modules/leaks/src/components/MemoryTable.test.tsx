/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import '@testing-library/jest-dom';
import { fireEvent, render, screen } from '@testing-library/react';
import { Session } from '../entity/session';
import MemoryTable from './MemoryTable';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string) => key }),
}));
jest.mock('@insight/lib/components', () => {
    const ReactModule = require('react');
    const Radio = ({ children, value, ...props }: any) => ReactModule.createElement(
        'label',
        null,
        ReactModule.createElement('input', { type: 'radio', value, ...props }),
        children,
    );
    Radio.Group = ({ children, value, onChange }: any) => ReactModule.createElement(
        'div',
        { onChange, 'data-value': value },
        children,
    );
    return {
        Radio,
        Button: ({ children, ...props }: any) => ReactModule.createElement('button', props, children),
        Checkbox: ({ children, ...props }: any) => ReactModule.createElement('label', null,
            ReactModule.createElement('input', { type: 'checkbox', ...props }), children),
    };
});
jest.mock('./BlocksTable', () => ({
    __esModule: true,
    default: ({ visible }: { visible: boolean }) => <div data-testid="blocks-agent-table">{String(visible)}</div>,
}));
jest.mock('./EventsTable', () => ({
    __esModule: true,
    default: ({ visible }: { visible: boolean }) => <div data-testid="events-agent-table">{String(visible)}</div>,
}));
jest.mock('./ThresholdModal', () => ({
    __esModule: true,
    default: () => null,
}));

describe('MemScope MemoryTable', () => {
    test('mounts only the active table and forwards System View visibility', () => {
        const session = new Session();
        session.module = 'memsnapshot';
        session.tableType = 'blocks';
        const { rerender } = render(<MemoryTable session={session} visible={false} />);

        expect(screen.getByTestId('blocks-agent-table')).toHaveTextContent('false');
        expect(screen.queryByTestId('events-agent-table')).not.toBeInTheDocument();

        rerender(<MemoryTable session={session} visible />);
        expect(screen.getByTestId('blocks-agent-table')).toHaveTextContent('true');

        fireEvent.click(screen.getByText('Event View'));
        expect(screen.queryByTestId('blocks-agent-table')).not.toBeInTheDocument();
        expect(screen.getByTestId('events-agent-table')).toHaveTextContent('true');
    });
});
