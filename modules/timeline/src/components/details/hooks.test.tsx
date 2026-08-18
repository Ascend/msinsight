/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

import { renderHook, act } from '@testing-library/react';
import { useDetailUpdater, useSelectedParamsDetailUpdater, useExtraDataUpdater, useSelectedDataDetailUpdater } from './hooks';
import { EMPTY_TABLE_STATE } from './types';
import type { ColumnDef, DetailDescriptor, InsightUnit, SingleDataDesc } from '../../entity/insight';
import { FilterType, TabState } from '../../entity/tabDependency';
import { MetaDataBase } from '../../entity/data';

describe('hooks test', () => {
    const unit: InsightUnit = {
        collapsible: false,
        name: 'unit',
        height: () => 1,
        expandable: false,
        isExpanded: false,
        hasExpanded: false,
        isMultiDeviceHidden: false,
        isOverlapAnalysisLoading: true,
        isDisplay: true,
        isUnitVisible: true,
        isMerged: false,
        isSummaryLoading: false,
        isTraceLoading: false,
        type: 'basic',
        phase: 'download',
        isParseLoading: false,
        shouldParse: false,
        progress: 0,
        showProgress: true,
        metadata: {} as MetaDataBase,
    };
    const expectedTablestate = {
        dataSource: [],
        columns: [
            {
                title: 'name2',
                key: 'name2',
                colSpan: 1,
                ellipsis: {
                    showTitle: true,
                },
                width: 'max-content',
                fixed: undefined,
                render: (): void => { },
            },
        ],
        rowKey: undefined,
        onExpand: undefined,
        loading: false,
    };
    const col: ColumnDef<Record<string, unknown>> = ['name2', (): string => 'render2', 'max-content'];

    it('useDetailUpdater test', async () => {
        let detailDescriptor: DetailDescriptor<unknown> = {
            columns: [
                col,
            ],
            fetchData: jest.fn().mockResolvedValue([]),
        };
        const tabStateLocal: TabState | undefined = new TabState({
            filter: {
                type: FilterType.INPUT_FILTER,
                field: 'name2',
                filterKeys: ['name2'],
                options: [{ label: 'name', value: '' }],
            },
        });
        const tabDep: unknown = [tabStateLocal];
        const onDataLoadFinish = jest.fn();
        const { result, rerender } = renderHook(
            ({ session, detail, tabState, dep, onDataLoaded }) => useDetailUpdater(session, detail, tabState, dep, onDataLoaded),
            { initialProps: { session, detail: detailDescriptor, tabState: tabStateLocal, dep: tabDep, onDataLoaded: onDataLoadFinish } },
        );
        expect(JSON.stringify(result.current)).toEqual(JSON.stringify(expectedTablestate));

        session.selectedUnits = [unit];
        session.selectedRange = [2, 3];
        session.phase = 'download';
        await act(async () => {
            rerender({ session, detail: detailDescriptor, tabState: tabStateLocal as TabState, dep: tabDep, onDataLoaded: onDataLoadFinish });
        });
        expect(JSON.stringify(result.current)).toEqual(JSON.stringify(expectedTablestate));

        detailDescriptor = {
            columns: [
                col,
            ],
            fetchData: jest.fn().mockResolvedValue([]),
        };
        await act(async () => {
            rerender({ session, detail: detailDescriptor, tabState: {} as TabState, dep: tabDep, onDataLoaded: onDataLoadFinish });
        });
        expect(onDataLoadFinish).toBeCalled();

        detailDescriptor = {
            columns: [],
            fetchData: jest.fn().mockRejectedValue('error'),
        };
        await act(async () => {
            rerender({ session, detail: detailDescriptor, tabState: tabStateLocal as TabState, dep: tabDep, onDataLoaded: onDataLoadFinish });
        });
        expect(detailDescriptor.fetchData).toBeCalled();
        expect(result.current).toStrictEqual(EMPTY_TABLE_STATE);
    });

    it('useSelectedParamsDetailUpdater test', async () => {
        let detailDescriptor: DetailDescriptor<unknown> | undefined;
        const { result, rerender } = renderHook(
            ({ session, detail }) => useSelectedParamsDetailUpdater(session, detail),
            { initialProps: { session, detail: detailDescriptor } },
        );
        expect(result.current).toStrictEqual(EMPTY_TABLE_STATE);

        detailDescriptor = {
            columns: [
                col,
            ],
            fetchData: jest.fn().mockResolvedValue([]),
        };
        session.phase = 'download';
        session.selectedParams = { baseRawId: 1, curRawId: 1 };
        await act(async () => {
            rerender({ session, detail: detailDescriptor });
        });
        expect(detailDescriptor.fetchData).toBeCalled();
        expect(JSON.stringify(result.current)).toEqual(JSON.stringify(expectedTablestate));

        detailDescriptor = {
            columns: [],
            fetchData: jest.fn().mockRejectedValue('error'),
        };
        await act(async () => {
            rerender({ session, detail: detailDescriptor });
        });
        expect(detailDescriptor.fetchData).toBeCalled();
        expect(result.current).toStrictEqual(EMPTY_TABLE_STATE);
    });

    it('useExtraDataUpdater test', async () => {
        let detailDescriptor: DetailDescriptor<unknown> | undefined;
        const { result, rerender } = renderHook(
            ({ session, detail }) => useExtraDataUpdater(session, detail),
            { initialProps: { session, detail: detailDescriptor } },
        );
        expect(result.current).toStrictEqual({});

        detailDescriptor = {
            columns: [],
            fetchData: jest.fn(),
            fetchExtraData: jest.fn().mockResolvedValue('res'),
        };
        session.phase = 'download';
        await act(async () => {
            rerender({ session, detail: detailDescriptor });
        });
        expect(detailDescriptor.fetchExtraData).toBeCalled();
        expect(result.current).toStrictEqual({ result: 'res' });

        detailDescriptor = {
            columns: [],
            fetchData: jest.fn(),
            fetchExtraData: jest.fn().mockRejectedValue('error'),
        };
        await act(async () => {
            rerender({ session, detail: detailDescriptor });
        });
        expect(detailDescriptor.fetchExtraData).toBeCalled();
        expect(result.current).toStrictEqual({});
    });

    it('useSelectedDataDetailUpdater test', async () => {
        let dataDesc: SingleDataDesc<Record<string, unknown>, unknown> = {
            fetchData: jest.fn().mockResolvedValue({ result: '' }),
            renderFields: [],
        };
        const selectedRecord: Record<string, string> = { name: 'test' };
        session.phase = 'download';
        const { result, rerender } = renderHook(
            ({ session, detail, selectedData }) => useSelectedDataDetailUpdater(session, detail, selectedData),
            { initialProps: { session, detail: dataDesc, selectedData: undefined as Record<string, string> | undefined } },
        );
        expect(result.current).toStrictEqual({ renderFields: undefined, data: {} });

        session.selectedUnits = [unit];
        await act(async () => {
            rerender({ session, detail: dataDesc, selectedData: selectedRecord });
        });
        expect(dataDesc.fetchData).toBeCalled();
        expect(result.current).toStrictEqual({
            renderFields: [],
            data: { result: '' },
        });

        const expectedRes = {
            renderFields: [
                ['Name', 'Name'],
            ],
            data: { result: '' },
        };
        const render = jest.fn().mockReturnValue('Name');
        dataDesc = {
            fetchData: jest.fn().mockResolvedValue({ result: '' }),
            renderFields: [
                ['Name', render],
            ],
        };
        await act(async () => {
            rerender({ session, detail: dataDesc, selectedData: selectedRecord });
        });
        expect(dataDesc.fetchData).toBeCalled();
        expect(render).toBeCalled();
        expect(result.current).toStrictEqual(expectedRes);

        const hidden = jest.fn().mockReturnValue(false);
        dataDesc = {
            fetchData: jest.fn().mockResolvedValue({ result: '' }),
            renderFields: [
                ['Name', render, hidden],
            ],
        };
        await act(async () => {
            rerender({ session, detail: dataDesc, selectedData: selectedRecord });
        });
        expect(dataDesc.fetchData).toBeCalled();
        expect(hidden).toBeCalled();
        expect(result.current).toStrictEqual(expectedRes);

        dataDesc = {
            fetchData: jest.fn().mockRejectedValue('error'),
            renderFields: [],
        };
        await act(async () => {
            rerender({ session, detail: dataDesc, selectedData: selectedRecord });
        });
        expect(dataDesc.fetchData).toBeCalled();
        expect(result.current).toStrictEqual(expectedRes);
    });

    it('uses the selected slice source metadata instead of the first range-selected lane', async () => {
        const firstMetadata = { lane: 'first' } as unknown as MetaDataBase;
        const sourceMetadata = { lane: 'source' } as unknown as MetaDataBase;
        const firstUnit = { ...unit, metadata: firstMetadata };
        const sourceUnit = { ...unit, metadata: sourceMetadata };
        const fetchData = jest.fn().mockResolvedValue({ title: 'source slice' });
        const dataDesc: SingleDataDesc<Record<string, unknown>, unknown> = {
            fetchData,
            renderFields: [],
        };

        session.phase = 'download';
        session.selectedUnits = [firstUnit, sourceUnit];
        session.selectedDataUnit = sourceUnit;
        const selectedRecord = { name: 'source slice' };

        const { result } = renderHook(() => useSelectedDataDetailUpdater(session, dataDesc, selectedRecord));
        await act(async () => { await Promise.resolve(); });

        expect(fetchData).toHaveBeenCalledWith(session, sourceMetadata);
        expect(fetchData).not.toHaveBeenCalledWith(session, firstMetadata);
        expect(result.current.data).toEqual({ title: 'source slice' });
    });
});
