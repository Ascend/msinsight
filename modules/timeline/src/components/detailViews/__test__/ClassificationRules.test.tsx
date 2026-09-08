/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { type ReactNode } from 'react';
import { fireEvent, render } from '@testing-library/react';
import { ClassificationRules } from '../ClassificationRules';
jest.mock('@insight/lib', () => ({
    LocalStorageKey: { OVERALL_METRICS_CLASSIFICATION_RULES: 'overall_metrics_classification_rules' },
    localStorageService: {
        getItem: (key: string) => JSON.parse(globalThis.localStorage.getItem(key) ?? 'null'),
        setItem: jest.fn(),
    },
}));
jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string, values?: Record<string, string>): string => values?.categories ? `${key}: ${values.categories}` : key }),
}));
jest.mock('antd', () => {
    const actual = jest.requireActual('antd');
    const react = jest.requireActual('react');
    return {
        ...actual,
        Modal: ({ open, title, children }: { open: boolean; title?: ReactNode; children?: ReactNode }) =>
            open ? react.createElement('div', null, title, children) : null,
    };
});
describe('ClassificationRules', () => {
    const localStorageService = jest.requireMock('@insight/lib').localStorageService;
    beforeEach(() => {
        localStorage.clear();
        localStorageService.setItem.mockClear();
        localStorageService.setItem.mockImplementation((key: string, value: unknown) => { globalThis.localStorage.setItem(key, JSON.stringify(value)); return true; });
    });
    it('persists and reloads an applied rule', () => {
        const onApply = jest.fn();
        const { getByLabelText, getByText, getByDisplayValue } = render(
            <ClassificationRules rules={[]} onApply={onApply}/>);
        fireEvent.click(getByLabelText('Classification Rules'));
        fireEvent.click(getByText('Add Rule'));
        fireEvent.change(getByLabelText('Category'), { target: { value: 'RMSNorm' } });
        fireEvent.change(getByLabelText('Keywords'), { target: { value: 'rms_norm, rmsnorm' } });
        fireEvent.click(getByText('Apply'));
        expect(onApply).toHaveBeenCalledWith([{ category: 'RMSNorm', keywords: ['rms_norm', 'rmsnorm'], splitByDirection: true }]);
        fireEvent.click(getByLabelText('Classification Rules'));
        expect(getByDisplayValue('rms_norm, rmsnorm')).toBeInTheDocument();
    });
});
