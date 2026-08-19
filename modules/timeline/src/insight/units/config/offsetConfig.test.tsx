/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.cos.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React from 'react';
// The installed Testing Library exports screen at runtime, but this repository's ESLint resolver uses an older declaration.
// eslint-disable-next-line import/named
import { fireEvent, render, screen } from '@testing-library/react';
import type { CardMetaData } from '../../../entity/data';
import type { InsightUnit } from '../../../entity/insight';
import { CardOffsetConfig } from './offsetConfig';

jest.mock('react-i18next', () => ({
    useTranslation: (): { t: (key: string) => string } => ({
        t: (key: string): string => key,
    }),
}));

jest.mock('@emotion/react', () => ({
    useTheme: (): Record<string, any> => ({
        buttonColor: {},
    }),
}));

const metadata = { cardId: 'rank0' } as unknown as CardMetaData;

beforeEach((): void => {
    session.unitsConfig.offsetConfig.timestampOffset = {
        rank0__host: 100,
        rank0__device: 200,
    };
    session.selectedUnits = [{ metadata, alignStartTimestamp: 300 } as unknown as InsightUnit];
    session.units = [{ metadata, alignStartTimestamp: 300 } as unknown as InsightUnit];
});

function renderEditor(isHovered = true): void {
    render(<CardOffsetConfig session={session} metadata={metadata} isHovered={isHovered} />);
    fireEvent.click(screen.getByTestId('offset-btn'));
}

describe('CardOffsetConfig', () => {
    it('aligns the Host and Device inputs with equal-width label columns', () => {
        renderEditor();

        expect(screen.getByText('Host Offset(ns):')).toHaveStyle({ width: '110px', flexShrink: 0 });
        expect(screen.getByText('Device Offset(ns):')).toHaveStyle({ width: '110px', flexShrink: 0 });
    });

    it('shows independent Host and Device values', () => {
        renderEditor();

        expect(screen.getByLabelText('Host Offset')).toHaveValue('100');
        expect(screen.getByLabelText('Device Offset')).toHaveValue('200');
    });

    it('updates Host without changing Device', () => {
        renderEditor();
        const hostInput = screen.getByLabelText('Host Offset');

        fireEvent.change(hostInput, { target: { value: '150' } });
        fireEvent.blur(hostInput);

        expect(session.unitsConfig.offsetConfig.timestampOffset).toEqual({
            rank0__host: 150,
            rank0__device: 200,
        });
    });

    it('restores the committed value when invalid input loses focus', () => {
        renderEditor();
        const hostInput = screen.getByLabelText('Host Offset');

        fireEvent.change(hostInput, { target: { value: 'invalid' } });
        fireEvent.blur(hostInput);

        expect(hostInput).toHaveValue('100');
        expect(session.unitsConfig.offsetConfig.timestampOffset.rank0__host).toBe(100);
    });

    it('applies the card start offset to only the selected category on the first click', () => {
        renderEditor();

        fireEvent.click(screen.getByLabelText('Align Host to Start'));

        expect(screen.getByLabelText('Host Offset')).toHaveValue('300');
        expect(screen.getByLabelText('Device Offset')).toHaveValue('200');
        expect(session.unitsConfig.offsetConfig.timestampOffset).toEqual({
            rank0__host: 300,
            rank0__device: 200,
        });
    });

    it('shows the offset indicator when either category is nonzero', () => {
        render(<CardOffsetConfig session={session} metadata={metadata} />);

        expect(screen.getByTitle('Offset set')).toBeInTheDocument();
    });

    it('hides the entry when both categories are zero and card is inactive', () => {
        session.unitsConfig.offsetConfig.timestampOffset = {
            rank0__host: 0,
            rank0__device: 0,
        };

        render(<CardOffsetConfig session={session} metadata={metadata} />);

        expect(screen.queryByTestId('offset-btn')).not.toBeInTheDocument();
    });
});
