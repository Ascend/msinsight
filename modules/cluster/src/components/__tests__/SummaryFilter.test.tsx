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
import { render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import Filter from '../summary/Filter';
import { Session } from '../../entity/session';
import type { PerformanceChartConditions } from '../summary/Index';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({
        t: (key: string) => key,
    }),
}));

jest.mock('@insight/lib/components', () => ({
    Select: ({ id, disabled }: { id: string; disabled?: boolean }) => (
        <select data-testid={id} disabled={disabled} />
    ),
}));

jest.mock('../Common', () => ({
    Label: ({ name }: { name: string }) => <label>{name}</label>,
}));

jest.mock('../communication/Filter', () => ({
    FormItem: ({ content }: { content: React.ReactNode }) => <>{content}</>,
}));

const conditions: PerformanceChartConditions = {
    step: 'All',
    baselineStep: 'All',
    group: 'All',
    orderBy: 'rankId',
    top: 'All',
};

const renderFilter = (arrangementRankCount: number): void => {
    const session = new Session();
    session.arrangementRankCount = arrangementRankCount;

    render(
        <Filter
            session={session}
            conditions={conditions}
            isPipeline={false}
            onFilterChange={jest.fn()}
        />,
    );
};

describe('summary Filter TOP selector', () => {
    it('disables TOP when only one arrangement node is available', () => {
        renderFilter(1);

        expect(screen.getByTestId('select-top')).toBeDisabled();
    });

    it('enables TOP when multiple arrangement nodes are available', () => {
        renderFilter(2);

        expect(screen.getByTestId('select-top')).not.toBeDisabled();
    });
});
