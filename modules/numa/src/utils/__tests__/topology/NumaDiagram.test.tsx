/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React from 'react';
import { fireEvent, render } from '@testing-library/react';
import i18n from '@insight/lib/i18n';
import type { NumaOverview } from '@/entities/numa/types';
import { NumaDiagram } from '@/features/topology/NumaDiagram';
import { numaOverviewFixture as data } from '@/testUtils/numaOverview.fixture';

describe('NumaDiagram', () => {
    beforeEach(async () => i18n.changeLanguage('enUS'));

    it('renders backend topology and selects the clicked item', () => {
        const onSelect = jest.fn();
        const { getByTestId } = render(
            <NumaDiagram data={data} selected={null} zoom={1} onSelect={onSelect} />,
        );

        expect(getByTestId('socket-0')).toBeTruthy();
        expect(getByTestId('numa-3')).toBeTruthy();
        expect(getByTestId('memory-2')).toBeTruthy();
        expect(getByTestId('connection-socket-link-0-1')).toBeTruthy();

        fireEvent.click(getByTestId('connection-numa-link-0-1'));
        expect(onSelect).toHaveBeenLastCalledWith(expect.objectContaining({ id: 'numa-link-0-1' }));
        fireEvent.click(getByTestId('numa-0'));
        expect(onSelect).toHaveBeenLastCalledWith(expect.objectContaining({ id: 'numa-0' }));
    });

    it.each([
        ['omitted', data.connections.filter((connection) => connection.type !== 'socket')],
        ['unavailable', data.connections.map((connection) => connection.type === 'socket'
            ? { ...connection, metrics: connection.metrics.map((metric) => ({ ...metric, hasValue: false })) }
            : connection)],
    ])('does not invent or render an %s socket link', (_caseName, connections) => {
        const overview: NumaOverview = { ...data, connections };
        const { queryByTestId } = render(
            <NumaDiagram data={overview} selected={null} zoom={1} onSelect={jest.fn()} />,
        );

        expect(queryByTestId('connection-socket-link-0-1')).toBeNull();
    });

    it('selects only memory connections supplied by the backend', () => {
        const onSelect = jest.fn();
        const rendered = render(
            <NumaDiagram data={data} selected={null} zoom={1} onSelect={onSelect} />,
        );
        fireEvent.click(rendered.getByTestId('memory-0'));
        expect(onSelect).toHaveBeenCalledWith(expect.objectContaining({ id: 'memory-0' }));

        const withoutMemory = {
            ...data,
            connections: data.connections.filter((connection) => connection.id !== 'memory-0'),
        };
        rendered.rerender(
            <NumaDiagram data={withoutMemory} selected={null} zoom={1} onSelect={onSelect} />,
        );
        onSelect.mockClear();
        fireEvent.click(rendered.getByTestId('memory-0'));
        expect(onSelect).not.toHaveBeenCalled();
    });
});
