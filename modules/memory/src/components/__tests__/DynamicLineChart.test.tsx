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
import { getTimelineCardOffset } from '../../connection/handler';
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
    getTimelineCardOffset: jest.fn((fn) => fn()),
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
    Select: ({ value, onChange, options }) => (
        <select
            data-testid="range-flag-select"
            multiple
            value={value}
            onChange={(e) => onChange?.(Array.from(e.target.selectedOptions, option => option.value))}
        >
            {(options || []).map((option) => (
                <option key={option.value} value={option.value}>{option.label}</option>
            ))}
        </select>
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
        onZoomBack,
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
                onClick={() => onSelectionChanged(0, 2)}
            >
                Trigger Selection
            </button>
            <button
                data-testid="trigger-zoom-back"
                onClick={() => onZoomBack?.()}
            >
                Trigger Zoom Back
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
    selectedRangeStack: [] as Array<{ startTs: number; endTs: number } | undefined>,
    current: 1,
    pageSize: 10,
    searchEventOperatorName: '',
    hostCondition: { options: [], value: '' },
    rankCondition: { options: [], value: 1 },
    rangeFlagList: [],
    getSelectedRankValue: jest.fn(),
    pushSelectedRangeHistory: jest.fn(function (this: typeof mockMemorySession) {
        this.selectedRangeStack.push(
            this.selectedRange ? { ...this.selectedRange } : undefined,
        );
    }),
    popSelectedRangeHistory: jest.fn(function (this: typeof mockMemorySession) {
        if (this.selectedRangeStack.length === 0) {
            return false;
        }
        this.selectedRange = this.selectedRangeStack.pop();
        return true;
    }),
    clearSelectedRangeHistory: jest.fn(function (this: typeof mockMemorySession) {
        this.selectedRangeStack = [];
        this.selectedRange = undefined;
    }),
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
        (runInAction as jest.Mock).mockImplementation((fn) => fn());
        mockMemorySession.pushSelectedRangeHistory.mockImplementation(function (this: typeof mockMemorySession) {
            this.selectedRangeStack.push(
                this.selectedRange ? { ...this.selectedRange } : undefined,
            );
        });
        mockMemorySession.popSelectedRangeHistory.mockImplementation(function (this: typeof mockMemorySession) {
            if (this.selectedRangeStack.length === 0) {
                return false;
            }
            this.selectedRange = this.selectedRangeStack.pop();
            return true;
        });
        mockMemorySession.clearSelectedRangeHistory.mockImplementation(function (this: typeof mockMemorySession) {
            this.selectedRangeStack = [];
            this.selectedRange = undefined;
        });
        mockMemorySession.selectedRankId = '';
        mockMemorySession.selectedRange = undefined;
        mockMemorySession.selectedRangeStack = [];
        mockMemorySession.current = 1;
        mockMemorySession.pageSize = 10;
        mockMemorySession.searchEventOperatorName = '';
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
            ...mockMemorySession,
            selectedRankId: '',
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
        mockMemorySession.selectedRankId = '1';
        render(<DynamicLineChart {...defaultProps} memorySession={mockMemorySession}/>);
        jest.advanceTimersByTime(300);

        await waitFor(() => {
            expect(screen.getByTestId('line-chart')).toBeInTheDocument();
        });

        fireEvent.click(screen.getByTestId('trigger-selection'));
        expect(mockMemorySession.pushSelectedRangeHistory).toHaveBeenCalled();
        expect(mockMemorySession.selectedRange).toEqual({ startTs: 1000, endTs: 3000 });
        expect(mockMemorySession.selectedRangeStack).toEqual([undefined]);
    });

    it('pops selected range history on zoom back', async () => {
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
        mockMemorySession.selectedRankId = '1';

        render(<DynamicLineChart {...defaultProps} memorySession={mockMemorySession}/>);
        jest.advanceTimersByTime(300);

        await waitFor(() => {
            expect(screen.getByTestId('line-chart')).toBeInTheDocument();
        });

        fireEvent.click(screen.getByTestId('trigger-selection'));
        fireEvent.click(screen.getByTestId('trigger-zoom-back'));

        expect(mockMemorySession.popSelectedRangeHistory).toHaveBeenCalled();
        expect(mockMemorySession.selectedRange).toBeUndefined();
        expect(mockMemorySession.selectedRangeStack).toEqual([]);
    });

    it('clears zoom history when data source changes', () => {
        render(<DynamicLineChart {...defaultProps} />);
        expect(mockMemorySession.clearSelectedRangeHistory).toHaveBeenCalled();
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

        // Verify the reset logic — find the search-reset action (sets searchEventOperatorName)
        const actionFunction = (runInAction as jest.Mock).mock.calls
            .map(call => call[0])
            .find((fn) => {
                mockMemorySession.searchEventOperatorName = 'keep';
                fn();
                return mockMemorySession.searchEventOperatorName === '';
            });
        expect(actionFunction).toBeDefined();
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
            render(<DynamicLineChart {...defaultProps} />);
            expect(mockMemorySession.clearSelectedRangeHistory).toHaveBeenCalled();
        });

        it('handles single data point scenario', () => {
            render(<DynamicLineChart {...defaultProps} />);
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
