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
import { fireEvent, render } from '@testing-library/react';

jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@insight/lib/components', () => {
    const ReactModule = require('react');
    return {
        Button: ({ children, ...props }: any) => ReactModule.createElement('button', props, children),
        Input: ({ allowClear, ...props }: any) => ReactModule.createElement('input', props),
        Checkbox: ({ children, checked, onChange, ...props }: any) => ReactModule.createElement('label', null,
            ReactModule.createElement('input', { ...props, type: 'checkbox', checked, onChange }), children),
        CollapsiblePanel: ({ title, children }: any) => ReactModule.createElement('section', null, title, children),
    };
});

export const frameTreeTheme = {
    bgColorCommon: '#fff',
    bgColorLight: '#f5f5f5',
    borderColor: '#ccc',
    borderColorLight: '#ddd',
    textColorPrimary: '#111',
    textColorSecondary: '#666',
    primaryColor: '#0074d9',
} as any;

export const readyUnmerged = async (view: ReturnType<typeof render>): Promise<void> => {
    await view.findByTestId('frame-tree-ready');
    const toggle = view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }) as HTMLInputElement;
    if (toggle.checked) fireEvent.click(toggle);
};
