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
import { ThemeProvider } from '@emotion/react';
import { I18nextProvider } from 'react-i18next';
import i18n from 'i18next';
import { LineChart } from '../LineChart'; // 根据实际路径调整
import * as echarts from 'echarts';
import { binarySearch, useResizeEventDependency } from '../../utils/memoryUtils';
import * as CommonUtils from '../Common';
import '@testing-library/jest-dom';

// Mock 依赖模块
jest.mock('echarts');
jest.mock('../../utils/memoryUtils');
jest.mock('../Common');
jest.mock('react-i18next', () => ({
    ...jest.requireActual('react-i18next'),
    useTranslation: jest.fn(),
}));
jest.mock('@emotion/react', () => ({
    ...jest.requireActual('@emotion/react'),
    useTheme: jest.fn(),
}));
const mockZr = {
    on: jest.fn(),
    off: jest.fn(),
};
// Mock ECharts 实例
const mockEChartsInstance = {
    setOption: jest.fn(),
    dispatchAction: jest.fn(),
    on: jest.fn(),
    getZr: () => mockZr,
    dispose: jest.fn(),
    resize: jest.fn(),
};
beforeEach(() => {
    // Mock echarts.init
    (echarts.init as jest.Mock).mockReturnValue(mockEChartsInstance);
    (echarts.getInstanceByDom as jest.Mock).mockReturnValue(mockEChartsInstance);

    // Mock 自定义 hooks
    (useResizeEventDependency as jest.Mock).mockReturnValue([0]);
    (CommonUtils.useChartCharacter as jest.Mock).mockReturnValue('testCharacter');
});

// Mock 主题
const mockTheme = {
    textColor: '#000000',
};

// Mock 国际化
const mockT = jest.fn((key) => key);
const mockI18n = {
    language: 'en-US',
};

const mockUseTranslation = jest.requireMock('react-i18next').useTranslation;
mockUseTranslation.mockReturnValue({
    t: mockT,
    i18n: mockI18n,
});

// Mock useTheme
const mockUseTheme = jest.requireMock('@emotion/react').useTheme;
mockUseTheme.mockReturnValue(mockTheme);

// 测试数据
const mockGraph = {
    title: 'Test Chart Title',
    columns: ['Time', 'Series 1', 'Series 2', 'Baseline'],
    rows: [
        ['1000', '10', '20'],
        ['2000', '15', '25'],
        ['3000', '20', '30'],
    ],
};

const defaultProps = {
    graph: mockGraph,
    hAxisTitle: 'Time',
    vAxisTitle: 'Value',
    isDark: false,
    isStatic: false,
};

// 测试包装组件
const TestWrapper: React.FC<{ children: React.ReactNode }> = ({ children }) => (
    <I18nextProvider i18n={i18n}>
        <ThemeProvider theme={mockTheme}>
            {children}
        </ThemeProvider>
    </I18nextProvider>
);

describe('LineCharts', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        mockUseTranslation.mockReturnValue({
            t: mockT,
            i18n: mockI18n,
        });
        mockUseTheme.mockReturnValue(mockTheme);
    });

    it('should render without crashing', () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        expect(screen.getByText(/Test\s*Chart\s*Title/)).toBeInTheDocument();
        expect(screen.getByText(/testCharacter/)).toBeInTheDocument();
    });

    it('should initialize echarts chart on mount', () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        expect(echarts.init).toHaveBeenCalledWith(
            expect.any(HTMLDivElement),
            'customed',
            { locale: 'en' },
        );
    });

    it('should initialize with dark theme when isDark is true', () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} isDark={true} />
            </TestWrapper>,
        );

        expect(echarts.init).toHaveBeenCalledWith(
            expect.any(HTMLDivElement),
            'dark',
            { locale: 'en' },
        );
    });

    it('should set chart options correctly', async () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.setOption).toHaveBeenCalled();
        });

        const setOptionCall = mockEChartsInstance.setOption.mock.calls[0][0];
        expect(setOptionCall).toHaveProperty('series');
        expect(setOptionCall.series).toHaveLength(3); // columns.length - 1
    });

    it('should handle dataZoom event', async () => {
        const mockOnSelectionChanged = jest.fn();

        render(
            <TestWrapper>
                <LineChart {...defaultProps} onSelectionChanged={mockOnSelectionChanged} />
            </TestWrapper>,
        );

        // 模拟 dataZoom 事件处理函数被调用
        await waitFor(() => {
            expect(mockEChartsInstance.on).toHaveBeenCalledWith('dataZoom', expect.any(Function));
        });

        const dataZoomHandler = mockEChartsInstance.on.mock.calls.find(
            call => call[0] === 'dataZoom',
        )[1];

        const mockEvent = { batch: [{ startValue: 1000, endValue: 2000 }] };
        dataZoomHandler(mockEvent);

        expect(mockOnSelectionChanged).toHaveBeenCalledWith(1000, 2000);
    });

    it('should handle restore event', async () => {
        const mockOnSelectionChanged = jest.fn();

        render(
            <TestWrapper>
                <LineChart {...defaultProps} onSelectionChanged={mockOnSelectionChanged} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.on).toHaveBeenCalledWith('restore', expect.any(Function));
        });

        const restoreHandler = mockEChartsInstance.on.mock.calls.find(
            call => call[0] === 'restore',
        )[1];

        restoreHandler();
        expect(mockOnSelectionChanged).toHaveBeenCalledWith(0, -1);
    });

    it('should show native dataZoom back button when onZoomBack is not provided', async () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.setOption).toHaveBeenCalled();
        });

        const setOptionCall = mockEChartsInstance.setOption.mock.calls[0][0];
        expect(setOptionCall.toolbox.feature.dataZoom).toBeDefined();
        expect(setOptionCall.toolbox.feature.dataZoom.icon?.back).not.toBe('none');
        expect(setOptionCall.toolbox.feature.myZoomBack).toBeUndefined();
    });

    it('should show custom zoom-back toolbox button that calls onZoomBack', async () => {
        const mockOnZoomBack = jest.fn();

        render(
            <TestWrapper>
                <LineChart {...defaultProps} onZoomBack={mockOnZoomBack} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.setOption).toHaveBeenCalled();
        });

        const setOptionCall = mockEChartsInstance.setOption.mock.calls[0][0];
        expect(setOptionCall.toolbox.feature.dataZoom.icon.back).toBe('none');
        expect(setOptionCall.toolbox.feature.myZoomBack).toBeDefined();
        expect(setOptionCall.toolbox.feature.myZoomBack.title).toBe('Back');
        expect(setOptionCall.toolbox.feature.myZoomBack.icon).toBeTruthy();

        setOptionCall.toolbox.feature.myZoomBack.onclick();
        expect(mockOnZoomBack).toHaveBeenCalled();
        expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
            type: 'takeGlobalCursor',
            key: 'dataZoomSelect',
            dataZoomSelectActive: true,
        });
    });

    it('should use Chinese title for custom zoom-back button', async () => {
        mockUseTranslation.mockReturnValue({
            t: mockT,
            i18n: { language: 'zh-CN' },
        });
        const mockOnZoomBack = jest.fn();

        render(
            <TestWrapper>
                <LineChart {...defaultProps} onZoomBack={mockOnZoomBack} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.setOption).toHaveBeenCalled();
        });

        const setOptionCall = mockEChartsInstance.setOption.mock.calls[0][0];
        expect(setOptionCall.toolbox.feature.myZoomBack.title).toBe('回退上次状态');
    });

    it('should fully reset on restore even when onZoomBack is provided', async () => {
        const mockOnSelectionChanged = jest.fn();
        const mockOnZoomBack = jest.fn();

        render(
            <TestWrapper>
                <LineChart
                    {...defaultProps}
                    onSelectionChanged={mockOnSelectionChanged}
                    onZoomBack={mockOnZoomBack}
                />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.on).toHaveBeenCalledWith('restore', expect.any(Function));
        });

        const restoreHandler = mockEChartsInstance.on.mock.calls.find(
            call => call[0] === 'restore',
        )[1];

        restoreHandler();
        expect(mockOnSelectionChanged).toHaveBeenCalledWith(0, -1);
        expect(mockOnZoomBack).not.toHaveBeenCalled();
    });

    it('should call onZoomBack on contextmenu when provided', async () => {
        const mockOnZoomBack = jest.fn();

        render(
            <TestWrapper>
                <LineChart {...defaultProps} onZoomBack={mockOnZoomBack} />
            </TestWrapper>,
        );

        const zrOnCall = mockEChartsInstance.getZr().on.mock.calls.find(
            call => call[0] === 'contextmenu',
        );
        expect(zrOnCall).toBeDefined();
        zrOnCall[1]();

        expect(mockOnZoomBack).toHaveBeenCalled();
        expect(mockEChartsInstance.dispatchAction).not.toHaveBeenCalledWith({
            type: 'restore',
        });
        expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
            type: 'takeGlobalCursor',
            key: 'dataZoomSelect',
            dataZoomSelectActive: true,
        });
    });

    it('should handle click event for point selection', async () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.on).toHaveBeenCalledWith('click', expect.any(Function));
        });

        const clickHandler = mockEChartsInstance.on.mock.calls.find(
            call => call[0] === 'click',
        )[1];

        const mockClickEvent = {
            seriesId: 'series-1',
            dataIndex: 2,
        };

        clickHandler(mockClickEvent);

        expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
            type: 'unselect',
            seriesId: 'series-1',
            dataIndex: [],
        });

        expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
            type: 'select',
            seriesId: 'series-1',
            dataIndex: 2,
        });
    });

    it('should handle contextmenu event', async () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        const zrOnCall = mockEChartsInstance.getZr().on.mock.calls.find(
            call => call[0] === 'contextmenu',
        );

        expect(zrOnCall).toBeDefined();
    });

    it('should highlight points when record is provided', async () => {
        const mockRecord = {
            allocationTime: '1500',
            releaseTime: '2500',
        };

        // Mock binarySearch 返回有效索引
        (binarySearch as jest.Mock)
            .mockReturnValueOnce(1) // startId
            .mockReturnValueOnce(2); // endId

        render(
            <TestWrapper>
                <LineChart {...defaultProps} record={mockRecord} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
                type: 'highlight',
                seriesName: undefined,
                dataIndex: [1, 2],
            });
        });
    });

    it('should downplay points when record is not provided', async () => {
        render(
            <TestWrapper>
                <LineChart {...defaultProps} record={undefined} />
            </TestWrapper>,
        );

        await waitFor(() => {
            expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
                type: 'downplay',
                seriesName: undefined,
                dataIndex: [],
            });
        });
    });

    it('should handle resize events', () => {
        const mockResizeDependency = ['resize-event'];
        (useResizeEventDependency as jest.Mock).mockReturnValue([mockResizeDependency]);

        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        expect(echarts.getInstanceByDom).toHaveBeenCalled();
        expect(mockEChartsInstance.resize).toHaveBeenCalled();
    });

    it('should not render title when graph title is empty', () => {
        const propsWithoutTitle = {
            ...defaultProps,
            graph: { ...mockGraph, title: '' },
        };

        render(
            <TestWrapper>
                <LineChart {...propsWithoutTitle} />
            </TestWrapper>,
        );

        expect(screen.queryByText('Test Chart Title')).not.toBeInTheDocument();
    });

    it('should handle binary search edge cases', async () => {
        const mockRecord = {
            allocationTime: '500', // 小于最小时间
            releaseTime: '4000', // 大于最大时间
        };

        (binarySearch as jest.Mock)
            .mockReturnValueOnce(-1) // startId not found
            .mockReturnValueOnce(-1); // endId not found

        render(
            <TestWrapper>
                <LineChart {...defaultProps} record={mockRecord} />
            </TestWrapper>,
        );

        await waitFor(() => {
            // 应该只调用 downplay，不调用 highlight
            expect(mockEChartsInstance.dispatchAction).toHaveBeenCalledWith({
                type: 'downplay',
                seriesName: undefined,
                dataIndex: [],
            });
        });
    });

    it('should clean up chart on unmount', () => {
        const { unmount } = render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        unmount();

        expect(mockEChartsInstance.dispose).toHaveBeenCalled();
    });

    it('should handle different languages', () => {
        mockUseTranslation.mockReturnValue({
            t: mockT,
            i18n: { language: 'zh-CN' },
        });

        render(
            <TestWrapper>
                <LineChart {...defaultProps} />
            </TestWrapper>,
        );

        expect(echarts.init).toHaveBeenCalledWith(
            expect.any(HTMLDivElement),
            'customed',
            { locale: 'zh' },
        );
    });
});
