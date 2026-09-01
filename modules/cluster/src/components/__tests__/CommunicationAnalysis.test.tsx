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
import * as testingLibrary from '@testing-library/react';
import '@testing-library/jest-dom';
import CommunicationAnalysis from '../communication/CommunicationAnalysis';
import {
    getSlowRankList,
    queryCommunication,
    queryCommunicationOperatorLists,
    queryIterations,
    queryMatrixOperators,
    queryOperators,
    queryStages,
} from '../../utils/RequestUtils';

jest.mock('../../utils/RequestUtils', () => ({
    getSlowRankList: jest.fn(),
    queryCommunication: jest.fn(),
    queryCommunicationOperatorLists: jest.fn(),
    queryIterations: jest.fn(),
    queryMatrixOperators: jest.fn(),
    queryOperators: jest.fn(),
    queryStages: jest.fn(),
}));

jest.mock('@insight/lib/icon', () => ({
    HelpIcon: () => null,
}), { virtual: true });

jest.mock('../communication/CommunicationTimeAnalysisChart', () => {
    const { runInAction } = jest.requireActual('mobx');
    return {
        __esModule: true,
        default: ({ loading, onAlignmentChange, session }:
        {loading: boolean; onAlignmentChange?: (targetOperator?: any) => void; session: any}) => <>
            <button data-testid="align-operator" onClick={() => {
                runInAction(() => {
                    session.targetOperator = { name: 'AllReduce', rankId: 0, timestamp: 0, duration: 1 };
                });
                onAlignmentChange?.(session.targetOperator);
            }}>align</button>
            <button data-testid="restore-alignment" onClick={() => {
                runInAction(() => {
                    session.targetOperator = undefined;
                });
                onAlignmentChange?.();
            }}>restore</button>
            <div data-testid="operator-loading">{String(loading)}</div>
        </>,
    };
});

jest.mock('../communication/CommunicationTimeChart', () => ({
    __esModule: true,
    default: ({ loading }: {loading: boolean}) => <div data-testid="duration-loading">{String(loading)}</div>,
}));

jest.mock('../communication/CommunicationTimeTable', () => ({
    __esModule: true,
    default: () => null,
}));

jest.mock('../communication/CommunicationMatrix', () => ({
    __esModule: true,
    default: () => null,
}));

jest.mock('../communication/CommunicationDuration/AdviceLabel', () => ({
    __esModule: true,
    default: () => null,
}));

jest.mock('../communication/CommunicationDuration/Opertators', () => ({
    __esModule: true,
    default: () => null,
}));

jest.mock('../communication/DiffTimeTable', () => ({
    __esModule: true,
    default: () => null,
}));

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
    (getSlowRankList as jest.Mock).mockResolvedValue(null);
});

it('loads operator and duration data concurrently with independent loading states', async() => {
    let resolveOperatorData: (value: unknown) => void = () => {};
    let resolveDurationData: (value: unknown) => void = () => {};
    (queryCommunicationOperatorLists as jest.Mock).mockImplementation(async() => await new Promise(resolve => {
        resolveOperatorData = resolve;
    }));
    (queryCommunication as jest.Mock).mockImplementation(async() => await new Promise(resolve => {
        resolveDurationData = resolve;
    }));

    testingLibrary.render(<CommunicationAnalysis session={session} />);
    const durationAnalysis = await testingLibrary.screen.findByText('Communication Duration Analysis');
    testingLibrary.fireEvent.click(durationAnalysis);

    await testingLibrary.waitFor(() => {
        expect(queryCommunicationOperatorLists).toHaveBeenCalledTimes(1);
        expect(queryCommunication).toHaveBeenCalledTimes(1);
    });
    expect(testingLibrary.screen.getByTestId('operator-loading')).toHaveTextContent('true');
    expect(testingLibrary.screen.getByTestId('duration-loading')).toHaveTextContent('true');

    resolveOperatorData({ minTime: 0, maxTime: 1, data: [] });
    await testingLibrary.waitFor(() => {
        expect(testingLibrary.screen.getByTestId('operator-loading')).toHaveTextContent('false');
    });
    expect(testingLibrary.screen.getByTestId('duration-loading')).toHaveTextContent('true');

    resolveDurationData({ items: [], advice: [] });
    await testingLibrary.waitFor(() => {
        expect(testingLibrary.screen.getByTestId('duration-loading')).toHaveTextContent('false');
    });
});

it('only reloads operator data when changing the alignment', async() => {
    (queryCommunicationOperatorLists as jest.Mock).mockResolvedValue({ minTime: 0, maxTime: 1, data: [] });
    (queryCommunication as jest.Mock).mockResolvedValue({ items: [], advice: [] });

    testingLibrary.render(<CommunicationAnalysis session={session} />);
    testingLibrary.fireEvent.click(await testingLibrary.screen.findByText('Communication Duration Analysis'));
    await testingLibrary.waitFor(() => {
        expect(testingLibrary.screen.getByTestId('operator-loading')).toHaveTextContent('false');
        expect(queryCommunicationOperatorLists).toHaveBeenCalledTimes(1);
    });

    const optionRequestCounts = {
        iterations: (queryIterations as jest.Mock).mock.calls.length,
        stages: (queryStages as unknown as jest.Mock).mock.calls.length,
        operators: (queryOperators as jest.Mock).mock.calls.length,
        slowRanks: (getSlowRankList as jest.Mock).mock.calls.length,
    };
    let resolveOperatorData: (value: unknown) => void = () => {};
    (queryCommunicationOperatorLists as jest.Mock).mockImplementation(async() => await new Promise(resolve => {
        resolveOperatorData = resolve;
    }));

    testingLibrary.fireEvent.click(testingLibrary.screen.getByTestId('align-operator'));
    expect(testingLibrary.screen.getByTestId('operator-loading')).toHaveTextContent('true');
    await testingLibrary.waitFor(() => expect(queryCommunicationOperatorLists).toHaveBeenCalledTimes(2));
    expect(queryCommunicationOperatorLists).toHaveBeenLastCalledWith(expect.objectContaining({ targetOperatorName: 'AllReduce' }));
    expect(queryCommunication).toHaveBeenCalledTimes(1);
    expect(queryIterations).toHaveBeenCalledTimes(optionRequestCounts.iterations);
    expect(queryStages).toHaveBeenCalledTimes(optionRequestCounts.stages);
    expect(queryOperators).toHaveBeenCalledTimes(optionRequestCounts.operators);
    expect(getSlowRankList).toHaveBeenCalledTimes(optionRequestCounts.slowRanks);

    await testingLibrary.act(async() => {
        resolveOperatorData({ minTime: 0, maxTime: 1, data: [] });
        await Promise.resolve();
    });
    await testingLibrary.waitFor(() => {
        expect(testingLibrary.screen.getByTestId('operator-loading')).toHaveTextContent('false');
    });

    (queryCommunicationOperatorLists as jest.Mock).mockResolvedValue({ minTime: 0, maxTime: 1, data: [] });
    testingLibrary.fireEvent.click(testingLibrary.screen.getByTestId('restore-alignment'));
    await testingLibrary.waitFor(() => expect(queryCommunicationOperatorLists).toHaveBeenCalledTimes(3));
    expect(queryCommunicationOperatorLists).toHaveBeenLastCalledWith(expect.objectContaining({ targetOperatorName: '' }));
    expect(queryCommunication).toHaveBeenCalledTimes(1);
});
