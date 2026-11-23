/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React from 'react';
import { render } from '@testing-library/react';
import { FilledLineChartRenderer, type FilledLineChartRendererProps } from './FilledLineChart';

jest.mock('@emotion/react', () => ({ useTheme: () => ({ colorPalette: { blue: '#00f' } }) }));
jest.mock('./TooltipComp', () => ({ TooltipComponent: () => null }));
jest.mock('./hooks', () => ({
    useBatchedRender: (callback: () => void, dependencies: React.DependencyList): void => {
        const ReactHooks = jest.requireActual('react');
        ReactHooks.useEffect(callback, dependencies);
    },
    useRangeAndDomain: () => [[0, 100], [0, 100]],
    useHoverPosX: () => undefined,
}));

describe('filled line shared renderer lifecycle', () => {
    function props(): FilledLineChartRendererProps {
        return {
            session: { isIE: false },
            unit: {},
            metadata: {},
            title: 'counter',
            width: 100,
            height: 100,
            margin: 0,
            palette: ['blue'],
            dataListState: [[10, 5]],
            intervalEnds: [20],
        } as unknown as FilledLineChartRendererProps;
    }

    function context(): CanvasRenderingContext2D {
        return {
            setTransform: jest.fn(),
            scale: jest.fn(),
            clearRect: jest.fn(),
            save: jest.fn(),
            restore: jest.fn(),
            rect: jest.fn(),
            clip: jest.fn(),
            beginPath: jest.fn(),
            closePath: jest.fn(),
            moveTo: jest.fn(),
            lineTo: jest.fn(),
            fill: jest.fn(),
            stroke: jest.fn(),
            setLineDash: jest.fn(),
        } as unknown as CanvasRenderingContext2D;
    }

    it('draws synchronously available data on the first mounted canvas', () => {
        const drawing = context();
        jest.spyOn(HTMLCanvasElement.prototype, 'getContext').mockReturnValue(drawing);
        render(<FilledLineChartRenderer {...props()}/>);
        expect(drawing.clearRect).toHaveBeenCalledWith(0, 0, 100, 100);
        expect(drawing.fill).toHaveBeenCalledTimes(1);
        expect(drawing.lineTo).toHaveBeenLastCalledWith(20, 100);
    });

    it('clears previous pixels when a source change supplies an empty dataset', () => {
        const drawing = context();
        jest.spyOn(HTMLCanvasElement.prototype, 'getContext').mockReturnValue(drawing);
        const initial = props();
        const view = render(<FilledLineChartRenderer {...initial}/>);
        expect(drawing.fill).toHaveBeenCalledTimes(1);
        (drawing.clearRect as jest.Mock).mockClear();
        view.rerender(<FilledLineChartRenderer {...initial} dataListState={[]} intervalEnds={[]}/>);
        expect(drawing.clearRect).toHaveBeenCalledTimes(1);
        expect(drawing.clearRect).toHaveBeenCalledWith(0, 0, 100, 100);
        expect(drawing.fill).toHaveBeenCalledTimes(1);
    });
});
