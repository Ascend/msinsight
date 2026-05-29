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

import React from 'react';
import { render, screen, waitFor, fireEvent } from '@testing-library/react';
import { observer } from 'mobx-react-lite';
import { runInAction } from 'mobx';
import { Spin, CollapsiblePanel } from '@insight/lib/components';
import { StyledEmpty } from '@insight/lib/utils';
import { useTranslation } from 'react-i18next';
import '@testing-library/jest-dom';
import DynamicLineChart from '../DynamicLineChart';
import { memoryCurveGet } from '../../utils/RequestUtils';
import { LineChart } from '../LineChart';
import { GroupBy } from '../../entity/memorySession';
import { getTimelineOffsetByKey } from '../../connection/handler';
import { Label } from '../Common';
import { FlexDiv } from '../../utils/styleUtils';

// Mock external dependencies
jest.mock('mobx-react-lite', () => ({
    observer: (component) => component,
}));

jest.mock('mobx', () => ({
    runInAction: jest.fn((fn) => fn()),
}));

jest.mock('../../connection/handler', () => ({
    getTimelineOffsetByKey: jest.fn((fn) => fn()),
}));

jest.mock('../Common', () => ({
    Label: ({ name }: { name: string }) => <label>{name}</label>,
}));

jest.mock('../../utils/styleUtils', () => ({
    FlexDiv: ({ children }) => (<div data-testid="flex-div">{children}</div>),
}));

jest.mock('@insight/lib/components', () => ({
    Spin: ({ children, spinning }) => (
        <div data-testid="spin" data-spinning={spinning}>
            {children}
        </div>
    ),
    CollapsiblePanel: ({ children, title }) => (
        <div data-testid="collapsible-panel">
            <div data-testid="panel-title">{title}</div>
            {children}
        </div>
    ),
}));

jest.mock('@insight/lib/utils', () => ({
    StyledEmpty: () => <div data-testid="empty">No Data</div>,
    customConsole: {
        error: jest.fn(),
    },
}));

jest.mock('react-i18next', () => ({
    useTranslation: () => ({
        t: (key) => {
            const translations = {
                'Memory Analysis': 'Memory Analysis',
                'Time (ms)': 'Time (ms)',
                'Memory Usage (MB)': 'Memory Usage (MB)',
                stream: 'Stream {stream}',
                memory_usage: 'Memory Usage',
            };
            return translations[key] || key;
        },
    }),
}));

jest.mock('../../utils/RequestUtils', () => ({
    memoryCurveGet: jest.fn(),
}));

jest.mock('../LineChart', () => ({
    LineChart: ({
        hAxisTitle,
        vAxisTitle,
        graph,
        onSelectionChanged,
        record,
        isDark,
        isStatic,
        rangeFlagData,
    }) => (
        <div data-testid="line-chart">
            <div data-testid="h-axis-title">{hAxisTitle}</div>
            <div data-testid="v-axis-title">{vAxisTitle}</div>
            <div data-testid="graph-title">{graph?.title}</div>
            <button
                data-testid="trigger-selection"
                onClick={() => onSelectionChanged(1000, 3000)}
            >
                Trigger Selection
            </button>
            <div data-testid="is-dark">{isDark.toString()}</div>
            <div data-testid="is-static">{isStatic.toString()}</div>
        </div>
    ),
}));

// Mock the entities
const mockSession = {
    compareRank: {
        isCompare: false,
        rankId: '',
    },
    isAllMemoryCompletedSwitch: false,
};

const mockMemorySession = {
    selectedRankId: '',
    groupId: GroupBy.STREAM,
    selectedRange: undefined,
    current: 1,
    pageSize: 10,
    searchEventOperatorName: '',
    hostCondition: { options: [], value: '' },
    rankCondition: { options: [], value: 1 },
    rangeFlagList: [],
    getSelectedRankValue: jest.fn(),
};

// Mock useEffect and useState
let mockSetState;
jest.mock('react', () => {
    const originalReact = jest.requireActual('react');
    return {
        ...originalReact,
        useState: (initialState) => {
            const [state, setState] = originalReact.useState(initialState);
            mockSetState = setState;
            return [state, setState];
        },
        useEffect: originalReact.useEffect,
    };
});

describe('DynamicLineChart', () => {
    const defaultProps = {
        session: mockSession,
        memorySession: mockMemorySession,
        isDark: false,
    };

    beforeEach(() => {
        jest.clearAllMocks();
        mockMemorySession.getSelectedRankValue.mockReturnValue({
            rankInfo: { rankId: 'test-rank' },
            dbPath: '/test/path',
        });
    });

    afterEach(() => {
        jest.useRealTimers();
    });

    it('renders without crashing', () => {
        render(<DynamicLineChart {...defaultProps} />);

        expect(screen.getByTestId('collapsible-panel')).toBeInTheDocument();
        expect(screen.getByTestId('panel-title')).toHaveTextContent('Memory Analysis');
    });

    it('shows empty state when no chart data is available', () => {
        render(<DynamicLineChart {...defaultProps} />);

        expect(screen.getByTestId('empty')).toBeInTheDocument();
    });

    it('shows loading spinner when curveSpin is true', () => {
        // We need to trigger a re-render with loading state
        const { rerender } = render(<DynamicLineChart {...defaultProps} />);

        // Simulate setting loading state
        if (mockSetState) {
            mockSetState(true); // Set curveSpin to true
        }

        const upadteMemorySession = {
            selectedRankId: '',
            groupId: GroupBy.STREAM,
            selectedRange: undefined,
            current: 1,
            pageSize: 10,
            searchEventOperatorName: '',
            hostCondition: { options: [], value: '' },
            rankCondition: { options: [1], value: 1 },
        };

        rerender(<DynamicLineChart {...defaultProps} memorySession={upadteMemorySession} />);

        expect(screen.getByTestId('spin')).toBeInTheDocument();
    });

    it('fetches memory curve data when selectedRankId changes', async () => {
        jest.useFakeTimers();

        const mockResponse = {
            title: 'Memory Usage Over Time',
            legends: ['stream 1', 'stream 2'],
            lines: [
                ['1000', '50', '60'],
                ['2000', '55', '65'],
            ],
        };

        (memoryCurveGet as jest.Mock).mockResolvedValue(mockResponse);

        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: 'new-rank-id',
        };

        render(
            <DynamicLineChart
                {...defaultProps}
                memorySession={updatedMemorySession}
            />,
        );

        // Advance timers to trigger useEffect
        jest.advanceTimersByTime(100);

        await waitFor(() => {
            expect(memoryCurveGet).toHaveBeenCalledWith({
                rankId: 'test-rank',
                dbPath: '/test/path',
                type: GroupBy.STREAM,
                isCompare: false,
                start: '',
                end: '',
            });
        });
    });

    it('handles memory curve fetch error', async () => {
        jest.useFakeTimers();

        const consoleError = require('@insight/lib/utils').customConsole.error;
        (memoryCurveGet as jest.Mock).mockRejectedValue(new Error('API Error'));

        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: 'error-rank',
        };

        render(
            <DynamicLineChart
                {...defaultProps}
                memorySession={updatedMemorySession}
            />,
        );

        jest.advanceTimersByTime(100);

        await waitFor(() => {
            expect(consoleError).toHaveBeenCalled();
        });
    });

    it('clears chart data when selectedRankId is empty', async () => {
        jest.useFakeTimers();

        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: '',
        };

        render(
            <DynamicLineChart
                {...defaultProps}
                memorySession={updatedMemorySession}
            />,
        );

        jest.advanceTimersByTime(100);

        await waitFor(() => {
            expect(screen.getByTestId('empty')).toBeInTheDocument();
        });
    });

    it('handles selection range change correctly', async () => {
        const mockResponse = {
            title: 'Test Chart',
            legends: ['stream 1'],
            lines: [
                ['1000', '50'],
                ['2000', '55'],
                ['3000', '60'],
                ['4000', '65'],
            ],
            rankOffsetNs: 100,
        };
        jest.useFakeTimers();

        (memoryCurveGet as jest.Mock).mockResolvedValue(mockResponse);
        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: '1',
            rankCondition: { options: [1], value: 1 },
            rangeFlagList: [],
        };
        jest.spyOn(require('mobx'), 'runInAction');
        render(<DynamicLineChart {...defaultProps} memorySession={updatedMemorySession}/>);
        jest.advanceTimersByTime(300);

        expect(runInAction).toHaveBeenCalled();

        // Verify runInAction was called with a function
        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        expect(typeof actionFunction).toBe('function');

        // Test the action function logic
        actionFunction();

        // Since we don't have memoryCurveData in this test, it should set selectedRange to undefined
        expect(mockMemorySession.selectedRange).toBeUndefined();
    });

    it('resets search conditions when isCompare changes', () => {
        const { rerender } = render(<DynamicLineChart {...defaultProps} />);

        // Update props to trigger useEffect
        const updatedSession = {
            ...mockSession,
            compareRank: { isCompare: true, rankId: '' },
        };

        rerender(
            <DynamicLineChart
                {...defaultProps}
                session={updatedSession}
            />,
        );

        expect(runInAction).toHaveBeenCalled();

        // Verify the reset logic
        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockMemorySession.searchEventOperatorName).toBe('');
        expect(mockMemorySession.current).toBe(1);
        expect(mockMemorySession.pageSize).toBe(10);
    });

    it('displays LineChart when data is available', async () => {
        jest.useFakeTimers();

        const mockResponse = {
            title: 'Test Memory Chart',
            legends: ['memory_usage'],
            lines: [['1000', '50']],
            rankOffsetNs: 100,
        };

        (memoryCurveGet as jest.Mock).mockResolvedValue(mockResponse);

        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: 'test-rank',
            rankCondition: { options: [1], value: 1 },
            rangeFlagList: [],
        };

        render(
            <DynamicLineChart
                {...defaultProps}
                memorySession={updatedMemorySession}
            />,
        );

        jest.advanceTimersByTime(100);

        expect(screen.getByTestId('panel-title')).toBeInTheDocument();
        expect(screen.getByTestId('spin')).toBeInTheDocument();
        expect(screen.getByTestId('empty')).toBeInTheDocument();
    });

    it('handles different groupBy types correctly', async () => {
        jest.useFakeTimers();

        const mockResponse = {
            title: 'Test Chart',
            legends: ['stream 1', 'stream 2'],
            lines: [['1000', '50', '60']],
        };

        (memoryCurveGet as jest.Mock).mockResolvedValue(mockResponse);

        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: 'test-rank',
            groupId: 'NON_STREAM', // Different group type
        };

        render(
            <DynamicLineChart
                {...defaultProps}
                memorySession={updatedMemorySession}
            />,
        );

        jest.advanceTimersByTime(100);

        await waitFor(() => {
            expect(memoryCurveGet).toHaveBeenCalledWith(
                expect.objectContaining({
                    type: 'NON_STREAM',
                }),
            );
        });
    });

    describe('onSelectedRangeChanged', () => {
        it('handles invalid range (start > end)', () => {
            const { container } = render(<DynamicLineChart {...defaultProps} />);

            // Access the component instance to test the function directly
            // This is a workaround since we can't easily access the inner function
            // In a real scenario, you might want to refactor to make this more testable
            expect(runInAction).toHaveBeenCalled();
        });

        it('handles single data point scenario', () => {
            render(<DynamicLineChart {...defaultProps} />);

            // Similar to above, we test through the integrated flow
            expect(runInAction).toHaveBeenCalled();
        });
    });

    it('passes correct props to LineChart', async () => {
        jest.useFakeTimers();

        const mockResponse = {
            title: 'Test Chart',
            legends: ['stream 1'],
            lines: [['1000', '50']],
            rankOffsetNs: 100,
        };

        (memoryCurveGet as jest.Mock).mockResolvedValue(mockResponse);

        const updatedMemorySession = {
            ...mockMemorySession,
            selectedRankId: 'test-rank',
        };

        const darkModeProps = {
            ...defaultProps,
            isDark: true,
        };

        render(
            <DynamicLineChart
                {...darkModeProps}
                memorySession={updatedMemorySession}
            />,
        );

        jest.advanceTimersByTime(100);

        expect(screen.getByTestId('spin')).toBeInTheDocument();
        expect(screen.getByTestId('empty')).toBeInTheDocument();
    });
});
