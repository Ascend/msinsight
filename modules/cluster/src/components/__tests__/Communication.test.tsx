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
import Filter, { AnalysisType, updateData } from '../communication/Filter';
import * as testingLibrary from '@testing-library/react';
import '@testing-library/jest-dom';
import React from 'react';
import { queryIterations, queryMatrixOperators, queryOperators, queryStages } from '../../utils/RequestUtils';
import CommunicationTimeChart from '../communication/CommunicationTimeChart';

jest.mock('../../utils/RequestUtils', () => ({
    queryIterations: jest.fn(),
    queryMatrixOperators: jest.fn(),
    queryOperators: jest.fn(),
    queryStages: jest.fn(),
}));

jest.mock('@insight/lib/icon', () => ({
    HelpIcon: () => null,
    ResetIcon: () => null,
}), { virtual: true });

beforeEach(() => {
    window.matchMedia = jest.fn().mockImplementation(query => ({
        matches: false,
        media: query,
        onchange: null,
        addListener: jest.fn(),
        removeListener: jest.fn(),
        addEventListener: jest.fn(),
        removeEventListener: jest.fn(),
        dispatchEvent: jest.fn(),
    }));
    session.selectedClusterPath = 'cluster';
    session.clusterList = [{
        name: 'cluster',
        path: 'cluster',
        parsed: true,
        durationParsed: true,
    }];
    (queryIterations as jest.Mock).mockResolvedValue({
        iterationOrRankId: { compare: ['1'], baseline: [] },
    });
    (queryStages as unknown as jest.Mock).mockResolvedValue({
        data: [{
            group: '(0, 1)',
            parallelStrategy: 'dp',
            groupIdHash: { compare: 'group', baseline: '' },
        }],
    });
    (queryMatrixOperators as jest.Mock).mockResolvedValue({ operatorName: ['Total Op Info'] });
    (queryOperators as jest.Mock).mockResolvedValue({ operatorName: ['Total Op Info'] });
});

it('testCommunicationFilterComponent', async() => {
    testingLibrary.render(<Filter session={session} handleFilterChange={(): void => {}}/>);

    expect(testingLibrary.screen.getByText('Step')).toBeInTheDocument();
    expect(testingLibrary.screen.getByText('Communication Group')).toBeInTheDocument();
    expect(testingLibrary.screen.getByText('Operator Name')).toBeInTheDocument();
    expect(await testingLibrary.screen.findByText('Communication Matrix')).toBeInTheDocument();
    expect(testingLibrary.screen.getByText('Communication Duration Analysis')).toBeInTheDocument();
});

it('shows loading while communication duration options are loading', async() => {
    let resolveOperators: (value: {operatorName: string[]}) => void = () => {};
    (queryOperators as jest.Mock).mockImplementation(async() => await new Promise(resolve => {
        resolveOperators = resolve;
    }));
    const onLoadingChange = jest.fn();

    testingLibrary.render(<Filter session={session} handleFilterChange={(): void => {}} onLoadingChange={onLoadingChange} />);

    const durationAnalysis = await testingLibrary.screen.findByText('Communication Duration Analysis');
    await testingLibrary.waitFor(() => expect(onLoadingChange).toHaveBeenLastCalledWith(false));
    onLoadingChange.mockClear();

    testingLibrary.fireEvent.click(durationAnalysis);

    expect(onLoadingChange).toHaveBeenCalledWith(true);
    resolveOperators({ operatorName: ['Total Op Info'] });
    await testingLibrary.waitFor(() => expect(onLoadingChange).toHaveBeenLastCalledWith(false));
});

it('clears loading when an earlier option request is superseded', async() => {
    let operatorRequestCount = 0;
    (queryOperators as jest.Mock).mockImplementation(async() => {
        operatorRequestCount += 1;
        if (operatorRequestCount === 1) {
            return await new Promise(() => {});
        }
        return { operatorName: ['Total Op Info'] };
    });
    const onLoadingChange = jest.fn();

    testingLibrary.render(<Filter session={session} handleFilterChange={(): void => {}} onLoadingChange={onLoadingChange} />);

    const durationAnalysis = await testingLibrary.screen.findByText('Communication Duration Analysis');
    await testingLibrary.waitFor(() => expect(onLoadingChange).toHaveBeenLastCalledWith(false));
    onLoadingChange.mockClear();

    testingLibrary.fireEvent.click(durationAnalysis);
    testingLibrary.fireEvent.click(durationAnalysis);

    await testingLibrary.waitFor(() => expect(queryOperators).toHaveBeenCalledTimes(2));
    await testingLibrary.waitFor(() => expect(onLoadingChange).toHaveBeenLastCalledWith(false));
});

it('applies timeline location conditions received before the filter mounts', async() => {
    const handleFilterChange = jest.fn();
    (queryOperators as jest.Mock).mockResolvedValue({ operatorName: ['hcom_allReduce_'] });
    updateData({
        iterationId: '1',
        stage: '(0, 1)',
        operatorName: 'hcom_allReduce_',
        type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
    });

    testingLibrary.render(<Filter session={session} handleFilterChange={handleFilterChange} />);

    await testingLibrary.waitFor(() => expect(handleFilterChange).toHaveBeenLastCalledWith(expect.objectContaining({
        iterationId: '1',
        stage: '(0, 1)',
        operatorName: 'hcom_allReduce_',
        type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
    })));
});

it('shows loading while communication duration data is loading', () => {
    const { container } = testingLibrary.render(<CommunicationTimeChart dataSource={[]} session={session} loading />);

    expect(container.querySelector('.ant-spin-spinning')).toBeInTheDocument();
});
