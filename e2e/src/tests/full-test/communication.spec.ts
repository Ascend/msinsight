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

import { test as baseTest, expect, WebSocket } from '@playwright/test';
import { CommunicationPage, FrameworkPage, TimelinePage } from '@/page-object';
import { clearAllData, importData, setupWebSocketListener, waitForResponse, waitForWebSocketEvent } from '@/utils';
import { SelectHelpers, TableHelpers } from '@/components';
import { FilePath } from '@/utils/constants';
import { dragSelect } from '../../utils';

interface TestFixtures {
    communicationPage: CommunicationPage;
    ws: Promise<WebSocket>;
}
const test = baseTest.extend<TestFixtures>({
    communicationPage: async ({ page }, use) => {
        const communicationPage = new CommunicationPage(page);
        await use(communicationPage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});
let requestDurationListResp: Promise<unknown>;
let requestTableDataResp: Promise<unknown>;
let allPagesSuccessRes: Promise<unknown>;

test.describe('Communication', () => {
    test.beforeEach(async ({ page, communicationPage, ws }) => {
        requestDurationListResp = waitForWebSocketEvent(page, (res) => res?.command === 'communication/duration/list');
        requestTableDataResp = waitForWebSocketEvent(page, (res) => res?.command === 'communication/operatorDetails');
        allPagesSuccessRes = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');

        const { loadingDialog } = new FrameworkPage(page);
        await page.goto('/');
        await importData(page, FilePath.DB);
        await communicationPage.goto();
        await page.mouse.move(0,0);
        if (await loadingDialog.count()) {
            await loadingDialog.waitFor({ state: 'detached' });
        }
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // 【case】数据展示配置
    test('data display configuration', async ({ page, communicationPage }) => {
        test.setTimeout(90_000);
        const {
            fullPage,
            stepSelector,
            communicationFrame,
            communicationGroupSelector,
            operatorNameSelector,
        } = communicationPage;
        // 筛选迭代id
        const stepSelect = new SelectHelpers(page, stepSelector, communicationFrame);
        await stepSelect.open();
        await stepSelect.selectOption('13');
        // 筛选通信域
        const communicationGroupSelect = new SelectHelpers(page, communicationGroupSelector, communicationFrame);
        await communicationGroupSelect.open();
        await communicationGroupSelect.selectOption('dp_cp:(0, 1)');

        // 筛选算子名称
        const operatorNameSelect = new SelectHelpers(page, operatorNameSelector, communicationFrame);
        await operatorNameSelect.open();
        await operatorNameSelect.selectOption('allgather-bottom1');
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(fullPage).toHaveScreenshot('page-loaded.png');
        await allPagesSuccessRes;
    });

    // 【case】通信矩阵配置
    test('communication matrix configuration', async ({ page, communicationPage }) => {
        const {
            communicationFrame,
            communicationMatrixRadio,
            communicationMatrixTypeSelector,
            communicationMatrixMaxRangeInput,
            communicationMatrixMinRangeInput,
            matrixChart,
        } = communicationPage;
        // 通信矩阵类型下拉选择
        await communicationMatrixRadio.check();
        const communicationMatrixTypeSelect = new SelectHelpers(page, communicationMatrixTypeSelector, communicationFrame);
        await communicationMatrixTypeSelect.open();
        await communicationMatrixTypeSelect.selectOption('Bandwidth(GB/s)'); // 带宽
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(matrixChart).toHaveScreenshot('matrix-bandwidth.png');

        await communicationMatrixTypeSelect.open();
        await communicationMatrixTypeSelect.selectOption('Transit Size(MB)'); // 传输大小
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(matrixChart).toHaveScreenshot('matrix-transit-size.png');

        await communicationMatrixTypeSelect.open();
        await communicationMatrixTypeSelect.selectOption('Transport Type'); // 链路方式
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(matrixChart).toHaveScreenshot('matrix-transit-type.png');
        await expect(communicationMatrixMinRangeInput).toBeHidden(); // 切换到链路方式，筛选范围不可见

        await communicationMatrixTypeSelect.open();
        await communicationMatrixTypeSelect.selectOption('Transit Time(ms)'); // 传输时长
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(matrixChart).toHaveScreenshot('matrix-transit-time.png');

        // 显示卡内通信开关（默认：不显示）
        const showInnerCommunicationCheckbox = communicationFrame.getByTestId('showInnerCommunication');
        await expect(showInnerCommunicationCheckbox).not.toBeChecked();
        // 勾选显示卡内通信
        await showInnerCommunicationCheckbox.check();
        // 根据输入控制所显示数据的范围并截图对比
        await communicationMatrixMinRangeInput.press('ArrowUp');
        await communicationMatrixMaxRangeInput.press('ArrowDown');
        const confirmBtn = communicationFrame.locator('button.ant-btn-primary').first();
        await confirmBtn.click();
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(matrixChart).toHaveScreenshot('matrix-show-inner.png');
        // 图表上滚动鼠标滚轮可以放大缩小图表
        await page.mouse.move(1000, 500);
        // 放大图表
        for (let _ = 0; _ < 5; _++) {
            await page.mouse.wheel(0, -200);
            await page.waitForTimeout(200);
        }
        await expect(matrixChart).toHaveScreenshot('matrix-zoom-in.png');
        // 缩小图表
        for (let _ = 0; _ < 5; _++) {
            await page.mouse.wheel(0, 200);
            await page.waitForTimeout(200);
        }
        await expect(matrixChart).toHaveScreenshot('matrix-zoom-out.png');
    });

    // HCCL 图表、通信时长图表
    test('HCCL chart', async ({ page, communicationPage }) => {
        const { communicationMatrixRadio, durationAnalysisRadio, communicationFrame, switchDurationAnalysis } = communicationPage;
        await switchDurationAnalysis(communicationMatrixRadio, durationAnalysisRadio);
        const visualizedCommunicationTimeChart = communicationFrame.locator('#main');

        await page.mouse.move(0, 0);
        await expect(communicationFrame.locator('#hccl')).toHaveScreenshot('communication-hccl.png', { maxDiffPixels: 500 });
        // HCCL图中悬浮窗测试
        await communicationFrame.locator('#hccl').hover({
            position: {
                x: 1348,
                y: 97,
            },
        });
        await expect(communicationFrame.locator('#hccl')).toHaveScreenshot('communication-hccl-tooltip.png', { maxDiffPixels: 500 });
        // HCCL图中横轴测试
        const canvasBox = await communicationFrame.locator('#hccl').boundingBox();
        await dragSelect(page, { x: canvasBox.x + 1326, y: canvasBox.y + 417 }, { x: canvasBox.x + 1393, y: canvasBox.y + 417 });
        await expect(communicationFrame.locator('#hccl')).toHaveScreenshot('communication-hccl-x-axis.png', { maxDiffPixels: 500 });
        // HCCL图中竖轴测试
        await dragSelect(page, { x: canvasBox.x + 1519, y: canvasBox.y + 80 }, { x: canvasBox.x + 1519, y: canvasBox.y + 193 });
        await expect(communicationFrame.locator('#hccl')).toHaveScreenshot('communication-hccl-y-axis.png', { maxDiffPixels: 500 });
        // ctrl+滚轮放大缩小测试
        await page.mouse.move(canvasBox.x + 1368, canvasBox.y + 196);
        // 代码应该是接收滚轮指令次数而不是距离,故使用循环
        await page.keyboard.down('Control');
        for (let _ = 0; _ < 5; _++) {
            await page.mouse.wheel(0, -200);
            await page.waitForTimeout(200);
        }
        await expect(communicationFrame.locator('#hccl')).toHaveScreenshot('communication-hccl-zoom-y.png', { maxDiffPixels: 500 });
        for (let _ = 0; _ < 10; _++) {
            await page.mouse.wheel(0, 200);
            await page.waitForTimeout(200);
        }
        await expect(communicationFrame.locator('#hccl')).toHaveScreenshot('communication-hccl-zoom-x.png', { maxDiffPixels: 500 });
        await page.keyboard.up('Control');
        await expect(visualizedCommunicationTimeChart).toHaveScreenshot('visualized-communication-time.png');
    });

    // 【case】专家建议
    test('communication advice', async ({ page, communicationPage, ws }) => {
        const { communicationMatrixRadio, durationAnalysisRadio, communicationFrame, switchDurationAnalysis } = communicationPage;
        await switchDurationAnalysis(communicationMatrixRadio, durationAnalysisRadio);
        const advice = communicationFrame.getByTestId('communicationAdvice');
        await waitForResponse(await ws, (res) => res?.command === 'communication/advisor');
        await page.mouse.move(0, 0);
        await expect(advice).toHaveScreenshot('advice.png');
    });

    // 【case】通信时长数据分析：展示对应通信域和算子 表头排序
    test('table header sort', async ({ page, communicationPage }) => {
        const { communicationMatrixRadio, durationAnalysisRadio, communicationFrame, switchDurationAnalysis } = communicationPage;
        await switchDurationAnalysis(communicationMatrixRadio, durationAnalysisRadio);
        const tableLocator = communicationFrame.getByTestId('dataAnalysisTable').locator('.ant-table-container > .ant-table-content > table');
        const dataAnalysisTable = new TableHelpers(page, tableLocator, communicationFrame);
        await dataAnalysisTable.sortTableHead('Elapsed Time(ms)');
        await page.mouse.move(0, 0);
        await expect(tableLocator).toHaveScreenshot('data-analysis-table-sort.png');
    });

    // 【case】通信时长数据分析：展示对应通信域和算子 点击icon / button展开子表格
    test('expand sub table', async ({ page, communicationPage }) => {
        const { communicationMatrixRadio, durationAnalysisRadio, communicationFrame, switchDurationAnalysis } = communicationPage;
        await switchDurationAnalysis(communicationMatrixRadio, durationAnalysisRadio);
        await requestDurationListResp;
        const tableLocator = communicationFrame.getByTestId('dataAnalysisTable').locator('.ant-table-container > .ant-table-content > table').first();
        const dataAnalysisTable = new TableHelpers(page, tableLocator, communicationFrame);
        // 定位并点击展开icon
        const expandIconList = (await dataAnalysisTable.getCell(1, 1)).locator('div div');
        const expandIcon = expandIconList.first();
        await expandIcon.click();
        await requestTableDataResp;
        // 定位第一行数据的子表格 验证子表格可见
        const expandTable = communicationFrame.getByTestId('dataAnalysisTable').locator('.ant-table-container').locator('.ant-table-container');
        await expect(expandTable).toBeVisible();
        // 再次点击icon 收起子表格
        await expandIcon.click();
        // 定位并点击查看详情按钮 再次验证子表格是否可见
        const expandButton = (await dataAnalysisTable.getCell(1, 12)).locator('button');
        await expandButton.click();
        await expect(expandTable).toBeVisible();
    });

    // 【case】通信时长数据分析：展示对应通信域和算子 通信算子详情子表格排序和分页
    test('communication operator details sub table', async ({ page, communicationPage, ws }) => {
        const { communicationMatrixRadio, durationAnalysisRadio, communicationFrame, switchDurationAnalysis } = communicationPage;
        // 切换到通信耗时分析并点击icon展开子表格
        await switchDurationAnalysis(communicationMatrixRadio, durationAnalysisRadio);
        await requestDurationListResp;
        const tableLocator = communicationFrame.getByTestId('dataAnalysisTable').locator('.ant-table-container > .ant-table-content > table').first();
        const dataAnalysisTable = new TableHelpers(page, tableLocator, communicationFrame);
        const expandIconList = (await dataAnalysisTable.getCell(1, 1)).locator('div div');
        const expandIcon = expandIconList.first();
        const firstRequest = waitForResponse(await ws, (res) => res?.command === 'communication/operatorDetails');
        await expandIcon.click();
        await firstRequest;
        // 定位到子表格元素
        const expandTableLocator = communicationFrame.getByTestId('dataAnalysisTable').locator('.ant-table-container').locator('.ant-table-container');
        const expandTable = new TableHelpers(page, expandTableLocator, communicationFrame);
        // 表格滚动到可视区域并点击表头排序
        const secondRequest = waitForResponse(await ws, (res) => res?.command === 'communication/operatorDetails');
        await expandTable.sortTableHead('Elapsed Time(ms)');
        await secondRequest;
        await page.mouse.move(0, 0);
        await expect(expandTableLocator).toHaveScreenshot('data-analysis-subtable-sort.png', { maxDiffPixels: 100 });
    });

    // 【case】点击“带宽分析”列的查看更多可以跳转至所选rank的对应算子带宽分析页
    test('jump to the bandwidth analysis page', async ({ page, communicationPage }) => {
        const { communicationMatrixRadio, durationAnalysisRadio, communicationFrame, switchDurationAnalysis, toOperatorpage } = communicationPage;
        await switchDurationAnalysis(communicationMatrixRadio, durationAnalysisRadio);
        await toOperatorpage(page, communicationFrame);
        const operatorPage = communicationFrame.getByTestId('operators');
        await operatorPage.waitFor({
            state: 'visible',
        });
        await page.waitForTimeout(1000);
        await expect(operatorPage).toHaveScreenshot('page-bandwidth-analysis.png');
    });

    // 右键点击 HCCL 图表，跳转至Timeline
    test('test_redirectToTimeline_when_rightClickHCCLChart', async ({ page, communicationPage }) => {
        const { communicationFrame, hcclChart } = communicationPage;
        const { fullPage } = new TimelinePage(page);

        await communicationFrame.getByText('Communication Duration Analysis').hover();
        await communicationFrame.getByText('Communication Duration Analysis').click();
        await page.waitForTimeout(500);
        await hcclChart.click({
            button: 'right',
            position: {
                x: 130,
                y: 96,
            },
        });
        await communicationFrame.getByText('Find in Timeline').click();
        await page.waitForTimeout(1000);
        try {
            await expect(fullPage).toHaveScreenshot('redirect-to-timeline.png', { maxDiffPixels: 500 });
        } catch (e) {
            await expect(fullPage).toHaveScreenshot('redirect-to-timeline-1.png', { maxDiffPixels: 500 });
        }
    });
});
