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

import { ThemeProvider } from '@emotion/react';
// eslint-disable-next-line import/named
import { render, screen } from '@testing-library/react';
import React from 'react';
import { ButtonGroup } from './ButtonGroup';
import type { InsightUnit } from '../entity/insight';
import type { Session } from '../entity/session';

jest.mock('react-i18next', () => ({
    useTranslation: (): { t: (key: string) => string } => ({ t: (key: string): string => key }),
}));
jest.mock('@insight/lib/components', () => ({
    Tooltip: ({ children }: React.PropsWithChildren): JSX.Element => <>{children}</>,
}));
jest.mock('@insight/lib/icon', () => ({
    ExpandAllLanesIcon: (): JSX.Element => <svg data-testid="expand-all-icon" />,
    CollapseAllLanesIcon: (): JSX.Element => <svg data-testid="collapse-all-icon" />,
}));
jest.mock('./TimeMakerButton', () => ({ TimeMakerButton: (): null => null }));
jest.mock('./UnitsFilter', () => ({ UnitsFilter: (): null => null }));
jest.mock('./CategorySearch', () => ({ CategorySearch: (): null => null }));
jest.mock('./FilterLinkLine', () => ({ FilterLinkLine: (): null => null }));

const theme = {
    textColorPrimary: '',
    bgColorLight: '',
    primaryColorHover: '',
    buttonColor: {
        enableClickColor: '', disableClickColor: '', emphasizeColor: '', suspendBGColor: '', unSuspendBGColor: '',
    },
};

const createUnit = (isExpanded = false, collapsible = true, children: InsightUnit[] = []): InsightUnit => ({
    name: 'Unit',
    children,
    isExpanded,
    collapsible,
    hasExpanded: true,
    metadata: {},
} as unknown as InsightUnit);

const createSession = (units: InsightUnit[]): Session => ({
    id: 'button-group-test',
    units,
    selectedUnits: [],
    threadsToFetch: new Map(),
    renderTrigger: true,
    isSimulation: false,
    areFlagEventsHidden: false,
    buttons: [],
} as unknown as Session);

const renderButtonGroup = (session: Session): ReturnType<typeof render> => render(
    <ThemeProvider theme={theme}>
        <ButtonGroup session={session} />
    </ThemeProvider>,
);

describe('ButtonGroup lane expansion toggle', () => {
    it('renders the icon and label for each toggle state', () => {
        const child = createUnit();
        const root = createUnit(false, true, [child]);
        const session = createSession([root]);
        const result = renderButtonGroup(session);
        const button = screen.getByTestId('tool-toggle-all-units');

        expect(screen.getByTestId('expand-all-icon')).toBeInTheDocument();
        expect(button).toHaveAttribute('aria-label', 'Expand all lanes');

        root.isExpanded = true;
        child.isExpanded = true;
        result.rerender(<ThemeProvider theme={theme}><ButtonGroup session={{ ...session } as Session} /></ThemeProvider>);
        expect(screen.getByTestId('collapse-all-icon')).toBeInTheDocument();
        expect(button).toHaveAttribute('aria-label', 'Collapse all lanes');

        result.rerender(<ThemeProvider theme={theme}><ButtonGroup session={createSession([createUnit(false, false)])} /></ThemeProvider>);
        expect(screen.getByTestId('tool-toggle-all-units')).toBeEnabled();
    });
});
