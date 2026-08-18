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

import { act, renderHook } from '@testing-library/react';
import { runInAction } from 'mobx';
import React from 'react';
import type { InsightUnit } from '../entity/insight';
import { useBottomPanelReactNodes } from './BottomPanel';

jest.mock('../pages/SessionPage', () => ({ BOTTOM_HEIGHT: 300 }));
jest.mock('./detailViews/DetailView', () => ({ getDetailViewItem: jest.fn() }));
jest.mock('./detailViews/FindInWindow', () => ({ useFindDetail: jest.fn() }));
jest.mock('@insight/lib', () => ({
    DragDirection: { RIGHT: 'RIGHT' },
    useDraggableContainer: jest.fn(),
}));

const SELECTED_DATA = 'SELECTED_DATA';
const SELECTED_RANGE = 'SELECTED_RANGE';

const createUnit = (label: string): { unit: InsightUnit; render: jest.Mock } => {
    const render = jest.fn(() => [
        { Detail: (): JSX.Element => <div>{`${label}-detail`}</div> },
        { Detail: (): JSX.Element => <div>{`${label}-list`}</div> },
    ]);
    const unit = {
        metadata: { label },
        bottomPanelRender: render,
    } as unknown as InsightUnit;
    return { unit, render };
};

describe('useBottomPanelReactNodes', () => {
    it('keeps the range renderer mounted when only the selected slice source changes', () => {
        const range = createUnit('range');
        const sourceA = createUnit('source-a');
        const sourceB = createUnit('source-b');
        session.selectedUnits = [range.unit];
        session.selectedRange = [1, 2];
        session.selectedDataUnit = sourceA.unit;

        const { rerender } = renderHook(() => useBottomPanelReactNodes(session, 300, SELECTED_RANGE));
        expect(range.render).toHaveBeenCalledTimes(1);

        act(() => {
            runInAction(() => {
                session.selectedDataUnit = sourceB.unit;
            });
            rerender();
        });

        expect(range.render).toHaveBeenCalledTimes(1);
        expect(sourceA.render).not.toHaveBeenCalled();
        expect(sourceB.render).not.toHaveBeenCalled();
    });

    it('updates the slice detail renderer when the selected slice source changes', () => {
        const range = createUnit('range');
        const sourceA = createUnit('source-a');
        const sourceB = createUnit('source-b');
        session.selectedUnits = [range.unit];
        session.selectedRange = [1, 2];
        session.selectedDataUnit = sourceA.unit;

        const { rerender } = renderHook(() => useBottomPanelReactNodes(session, 300, SELECTED_DATA));
        expect(sourceA.render).toHaveBeenCalledTimes(1);
        expect(sourceB.render).not.toHaveBeenCalled();

        act(() => {
            runInAction(() => {
                session.selectedDataUnit = sourceB.unit;
            });
            rerender();
        });

        expect(sourceA.render).toHaveBeenCalledTimes(1);
        expect(sourceB.render).toHaveBeenCalledTimes(1);
        expect(range.render).not.toHaveBeenCalled();
    });
});
