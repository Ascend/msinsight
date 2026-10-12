/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React from 'react';
import { render, screen } from '@testing-library/react';
import { MISelect } from './MISelect';

jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@emotion/styled', () => {
    const ReactModule = require('react');
    const styled: any = (component: unknown) => () => component;
    styled.div = () => ({ children }: any) => ReactModule.createElement('div', null, children);
    return { __esModule: true, default: styled };
});
jest.mock('antd', () => {
    const ReactModule = require('react');
    const Select: any = ({ children }: any) => ReactModule.createElement('div', null, children);
    Select.Option = ({ disabled, children }: any) => ReactModule.createElement('div', {
        'data-testid': 'select-option', 'data-disabled': String(disabled), 'data-disabled-type': typeof disabled,
    }, children);
    return { Select };
});

describe('Select source option normalization without changing target layout', () => {
    test.each([false, true])('normalizes each disabled option with custom renderer %s', custom => {
        render(<MISelect options={[
            { value: 'enabled', label: 'Enabled', disabled: false },
            { value: 'disabled', label: 'Disabled', disabled: true },
            { value: 'default', label: 'Default' },
        ]} optionRender={custom ? option => <span>{`Custom ${option.label}`}</span> : undefined} />);
        const options = screen.getAllByTestId('select-option');
        expect(options.map(option => option.getAttribute('data-disabled'))).toEqual(['false', 'true', 'false']);
        expect(options.map(option => option.getAttribute('data-disabled-type'))).toEqual(['boolean', 'boolean', 'boolean']);
        expect(options[0].textContent).toBe(custom ? 'Custom Enabled' : 'Enabled');
    });
});
