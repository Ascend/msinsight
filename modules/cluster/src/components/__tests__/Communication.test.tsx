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
import Filter, { AnalysisType, prepareAnalysisType, updateData } from '../communication/Filter';
import * as testingLibrary from '@testing-library/react';
import '@testing-library/jest-dom';
import React from 'react';
import { queryIterations, queryMatrixOperators, queryOperators, queryStages } from '../../utils/RequestUtils';
import CommunicationTimeChart from '../communication/CommunicationTimeChart';
import i18n from '@insight/lib/i18n';

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

beforeEach(async() => {
    await i18n.changeLanguage('enUS');
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

it('keeps communication matrix analysis when matrix operators are available', async() => {
    const handleFilterChange = jest.fn();

    testingLibrary.render(<Filter session={session} handleFilterChange={handleFilterChange} />);

    await testingLibrary.waitFor(() => expect(handleFilterChange).toHaveBeenLastCalledWith(expect.objectContaining({
        type: AnalysisType.COMMUNICATION_MATRIX,
        operatorName: 'Total Op Info',
    })));
    expect(queryMatrixOperators).toHaveBeenCalledWith({
        iterationId: '1',
        stage: '(0, 1)',
        pgName: 'dp',
        groupIdHash: 'group',
    });
    expect(queryOperators).not.toHaveBeenCalled();
});

it('falls back to communication duration analysis when matrix operators are unavailable', async() => {
    const handleFilterChange = jest.fn();
    (queryMatrixOperators as jest.Mock).mockRejectedValue(new Error('matrix table does not exist'));
    (queryOperators as jest.Mock).mockResolvedValue({ operatorName: ['hcom_allReduce_'] });

    testingLibrary.render(<Filter session={session} handleFilterChange={handleFilterChange} />);

    await testingLibrary.waitFor(() => expect(handleFilterChange).toHaveBeenLastCalledWith(expect.objectContaining({
        type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
        operatorName: 'hcom_allReduce_',
    })));
    expect(queryOperators).toHaveBeenCalledWith({
        iterationId: '1',
        stage: '(0, 1)',
        pgName: 'dp',
        groupIdHash: 'group',
    });
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

it.each(['external update', 'user change'])('preserves an ordinary partial update before a subsequent %s', async(reason) => {
    const handleFilterChange = jest.fn();
    testingLibrary.render(<Filter session={session} handleFilterChange={handleFilterChange} />);
    await testingLibrary.waitFor(() => expect(handleFilterChange).toHaveBeenLastCalledWith(expect.objectContaining({ iterationId: '1' })));
    let resolveOld!: (value: { iterationOrRankId: { compare: string[]; baseline: string[] } }) => void;
    (queryIterations as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { resolveOld = resolve; }))
        .mockResolvedValue({ iterationOrRankId: { compare: ['1', '2'], baseline: [] } });
    (queryStages as jest.Mock).mockImplementation(async ({ iterationId }) => ({ data: [{
        group: iterationId === '2' ? '(2, 3)' : '(0, 1)', parallelStrategy: 'dp',
        groupIdHash: { compare: `group${iterationId}`, baseline: '' },
    }] }));

    testingLibrary.act(() => {
        updateData({ iterationId: '2' });
        if (reason === 'external update') {
            updateData({ type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS });
        } else {
            testingLibrary.fireEvent.click(testingLibrary.screen.getByText('Communication Duration Analysis'));
        }
    });
    await testingLibrary.waitFor(() => expect(handleFilterChange).toHaveBeenLastCalledWith(expect.objectContaining({
        iterationId: '2', stage: '(2, 3)', groupIdHash: 'group2', type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
    })));
    expect(queryOperators).toHaveBeenLastCalledWith({ iterationId: '2', stage: '(2, 3)', pgName: 'dp', groupIdHash: 'group2' });
    const committed = handleFilterChange.mock.calls[handleFilterChange.mock.calls.length - 1][0];
    await testingLibrary.act(async() => { resolveOld({ iterationOrRankId: { compare: ['1', '2'], baseline: [] } }); });
    expect(handleFilterChange).toHaveBeenLastCalledWith(committed);
});

it('preserves the selected group hash when names match during a pending partial update', async() => {
    const changed = jest.fn();
    const selectedGroup = '(0, 1)';
    (queryIterations as jest.Mock).mockResolvedValue({ iterationOrRankId: { compare: ['1', '2'], baseline: [] } });
    (queryStages as jest.Mock).mockResolvedValue({ data: ['(0, 1)', selectedGroup].map((group, index) => ({
        group, parallelStrategy: 'dp', groupIdHash: { compare: `group${index}`, baseline: '' },
    })) });
    testingLibrary.render(<Filter session={session} handleFilterChange={changed} />);
    await testingLibrary.waitFor(() => expect(changed).toHaveBeenLastCalledWith(expect.objectContaining({ iterationId: '1' })));
    let complete!: (value: { iterationOrRankId: { compare: string[]; baseline: string[] } }) => void;
    (queryIterations as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { complete = resolve; }));
    testingLibrary.act(() => { updateData({ iterationId: '2' }); });
    testingLibrary.fireEvent.mouseDown(document.querySelector('#communicationGroup-select') as HTMLElement);
    const options = await testingLibrary.screen.findAllByText(`dp:${selectedGroup}`, { selector: '.ant-select-item-option-content' });
    testingLibrary.fireEvent.click(options[options.length - 1]);
    await testingLibrary.waitFor(() => expect(changed).toHaveBeenLastCalledWith(expect.objectContaining({
        iterationId: '2', stage: selectedGroup, pgName: 'dp', groupIdHash: 'group1',
    })));
    const count = changed.mock.calls.length;
    await testingLibrary.act(async() => { complete({ iterationOrRankId: { compare: ['1', '2'], baseline: [] } }); });
    expect(changed).toHaveBeenCalledTimes(count);
});

const preparationContext = (): { signal: AbortSignal; deadline: number } => ({ signal: new AbortController().signal, deadline: Date.now() + 60000 });

it('resolves preparation only after normalized conditions are published', async() => {
    const changed = jest.fn();
    testingLibrary.render(<React.StrictMode><Filter session={session} handleFilterChange={changed} /></React.StrictMode>);
    await testingLibrary.waitFor(() => expect(changed).toHaveBeenLastCalledWith(expect.objectContaining({ iterationId: '1' })));
    (queryOperators as jest.Mock).mockResolvedValue({ operatorName: ['normalized_operator'] });
    let pending!: Promise<void>;
    testingLibrary.act(() => {
        pending = prepareAnalysisType(AnalysisType.COMMUNICATION_DURATION_ANALYSIS, preparationContext())
            .then(conditions => { expect(changed).toHaveBeenLastCalledWith(conditions); });
    });
    await testingLibrary.waitFor(() => expect(changed).toHaveBeenLastCalledWith(expect.objectContaining({
        type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS, operatorName: 'normalized_operator',
    })));
    await pending;
});

it('cancels pending preparation without publishing late options', async() => {
    const changed = jest.fn();
    const controller = new AbortController();
    testingLibrary.render(<Filter session={session} handleFilterChange={changed} />);
    await testingLibrary.waitFor(() => expect(changed).toHaveBeenLastCalledWith(expect.objectContaining({ iterationId: '1' })));
    let complete!: (value: { operatorName: string[] }) => void;
    (queryOperators as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { complete = resolve; }));
    const failed = jest.fn();
    testingLibrary.act(() => {
        void prepareAnalysisType(AnalysisType.COMMUNICATION_DURATION_ANALYSIS, { ...preparationContext(), signal: controller.signal }).catch(failed);
    });
    await testingLibrary.waitFor(() => expect(queryOperators).toHaveBeenCalled());
    testingLibrary.act(() => { controller.abort(); });
    await testingLibrary.waitFor(() => expect(failed).toHaveBeenCalledWith(expect.any(Error)));
    const count = changed.mock.calls.length;
    await testingLibrary.act(async() => { complete({ operatorName: ['late_operator'] }); });
    expect(changed).toHaveBeenCalledTimes(count);
});

it('shows loading while communication duration data is loading', () => {
    const { container } = testingLibrary.render(<CommunicationTimeChart dataSource={[]} session={session} loading />);

    expect(container.querySelector('.ant-spin-spinning')).toBeInTheDocument();
});
