/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React, { type ReactElement } from 'react';
import { render, type RenderOptions, type RenderResult } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { createInstance, type Resource } from 'i18next';
import { I18nextProvider } from 'react-i18next';
import { light } from '../lib/src/theme/light';

export function renderWithProviders(
    ui: ReactElement,
    { resources = {}, ...options }: Omit<RenderOptions, 'wrapper'> & { resources?: Resource } = {},
): RenderResult {
    const i18n = createInstance();
    void i18n.init({
        lng: 'enUS',
        fallbackLng: false,
        initImmediate: false,
        resources,
        interpolation: { escapeValue: false },
    });
    return render(ui, {
        wrapper: ({ children }) => (
            <I18nextProvider i18n={i18n as React.ComponentProps<typeof I18nextProvider>['i18n']}>
                <ThemeProvider theme={light}>{children}</ThemeProvider>
            </I18nextProvider>
        ),
        ...options,
    });
}
