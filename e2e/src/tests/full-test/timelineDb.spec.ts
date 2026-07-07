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

import { expect, test as baseTest, WebSocket } from '@playwright/test';
import { CommunicationPage, SystemView, TimelinePage } from '@/page-object';
import { clearAllData, importData, setupWebSocketListener, waitForResponse } from '@/utils';
import { FilePath } from '@/utils/constants';
import { InputHelpers, SelectHelpers } from '@/components';

interface TestFixtures {
    timelinePage: TimelinePage;
    ws: Promise<WebSocket>;
}
const test = baseTest.extend<TestFixtures>({
    timelinePage: async ({ page }, use) => {
        const timelinePage = new TimelinePage(page);
        await use(timelinePage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});
let parseClusterCompletedRes;
test.describe('Timeline(DB)', () => {
    test.beforeEach(async ({ page, timelinePage, ws }) => {
        const { timelineFrame } = timelinePage;
        await timelinePage.goto();
        await clearAllData(page);
        const allPagesSuccessRes = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        parseClusterCompletedRes = waitForResponse(await ws, (res) => res?.event === 'parse/clusterCompleted');
        await importData(page, FilePath.DB);
        await allPagesSuccessRes;
        const secondLayerUnit = timelineFrame.locator('#main-container').getByText('process 3430895');
        await expect(secondLayerUnit).toBeVisible();
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // System View - Stats System View 数据展示
    test('test_db_StatsSystemViewDataDisplay_in_SystemView', async ({ page, timelinePage }) => {
        const { bottomPanel, timelineFrame } = timelinePage;
        const systemView = new SystemView(page);
        await systemView.goto();

        const statsSystemViewOptions = [
            'Overall Metrics',
            'Python API Summary',
            'CANN API Summary',
            'Ascend HardWare Task Summary',
            'Communication Summary',
            'Overlap Analysis',
            'Kernel Details',
        ];
        await page.waitForTimeout(2500);
        await expect(bottomPanel).toHaveScreenshot('StatsSystemView-Overall-Metrics.png', { maxDiffPixels: 400 });

        for (const item of statsSystemViewOptions) {
            const option = bottomPanel.getByText(item, { exact: true });
            await option.click();
            await timelineFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
            await expect(bottomPanel).toHaveScreenshot(`StatsSystemView-${item}.png`, { maxDiffPixels: 400 });
        }
    });

    // System View - Expert System View 数据展示
    test('test_db_ExpertSystemViewDataDisplay_in_SystemView', async ({ page, timelinePage }) => {
        const { bottomPanel, timelineFrame } = timelinePage;
        const systemViewPage = new SystemView(page);
        const systemViewSelector = new SelectHelpers(page, systemViewPage.selectSystemView, timelineFrame);
        await systemViewPage.goto();

        await systemViewSelector.open();
        await systemViewSelector.selectOption('Expert System View');

        const expertSystemViewOptions = ['Expert Analysis', 'Affinity API', 'Affinity Optimizer',
            'AICPU Operators', 'ACLNN Operators', 'Operators Fusion'];

        for (const item of expertSystemViewOptions) {
            const option = timelineFrame.getByText(item, { exact: true });
            await option.click();
            await timelineFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
            await expect(bottomPanel).toHaveScreenshot(`ExpertSystemView-${item}.png`, { maxDiffPixels: 400 });
        }
    });

    // 工具栏 - 算子搜索
    test('test_db_operatorSearch_when_EnterOperatorName', async ({ page, timelinePage }) => {
        const { searchBtn, timelineFrame, openInWindows } = timelinePage;
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('CtxGetOverflowAddr');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-operator.png', { maxDiffPixels: 200 });
    });

    // 工具栏 - 算子搜索在泳道较深的位置时能显示在div中
    test('test_db_deepOperatorSearch_when_EnterOperatorName', async ({ page, timelinePage }) => {
        const { searchBtn, timelineFrame, openInWindows } = timelinePage;
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('NpuSwigluBackward0');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-deep-operator.png', { maxDiffPixels: 200 });
    });

    // 工具栏 - 算子连线
    test('test_operatorLinkLine', async ({ page, timelinePage }) => {
        const { flowBtn, timelineFrame } = timelinePage;
        await timelineFrame.locator('div:nth-child(4) > .unit-info > .css-rdzxz6 > div > div > .insight-unit-fold').click();
        await page.waitForTimeout(100);
        await timelineFrame.locator('div:nth-child(15) > .unit-info > .css-rdzxz6 > div > div > .insight-unit-fold').click();
        const LinkLineType = [
            'HostToDevice',
            'async_npu',
        ];
        for (const item of LinkLineType) {
            const LinkTypeCheckbox = timelineFrame.getByLabel(item);
            await flowBtn.click();
            await LinkTypeCheckbox.check();
            await flowBtn.click();
            await page.mouse.move(0, 0);
            await page.waitForTimeout(2000);
            await expect(timelineFrame.locator('#main-container')).toHaveScreenshot(`operator-link-line-${item}.png`, { maxDiffPixels: 200 });
            await flowBtn.click();
            await LinkTypeCheckbox.uncheck();
            await flowBtn.click();
        }
    });

    //todo: 工具栏 连线async_task_queue  && fwdbwd


    // 工具栏 - 泳道(card)过滤
    test('test_db_cardFilter', async ({ page, timelinePage }) => {
        const { filterBtn, timelineFrame, selectCardFilterContent } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectCardFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.selectOption('3');
        await filterBtn.click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('card-filter.png', { maxDiffPixels: 200 });
    });

    // 工具栏 - 泳道(unit)过滤
    test('test_db_unitFilter', async ({ page, timelinePage }) => {
        const { filterBtn, timelineFrame, selectUnitFilterContent } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectUnitFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.setValue('Ascend Hardware');
        await filterContentSelector.selectOption('Ascend Hardware');
        await filterBtn.click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('units-filter.png', { maxDiffPixels: 100 });
    });

    // 右键菜单--右键点击通信算子跳转至通信页面
    test('test_db_redirectToCommunication_when_rightClickHCCLOperator', async ({ page, timelinePage }) => {
        await parseClusterCompletedRes;
        test.setTimeout(30_000);
        const { communicationFrame } = new CommunicationPage(page);
        const { filterBtn, timelineFrame, selectUnitFilterContent, unitWrapperScroller,
            searchBtn, openInWindows } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectUnitFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.setValue('Communication');
        await filterContentSelector.selectOption('Communication');
        await filterBtn.click();
        await page.mouse.move(0, 0);
        await unitWrapperScroller.locator('.insight-lane-info-name').getByText('Communication', { exact: true }).first().click();
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('hcom_batchSendRecv__128_4_1');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        const selectedCanvas = timelineFrame.locator('.chart-selected canvas.drawCanvas').first();
        await selectedCanvas.waitFor({ state: 'visible' });
        await searchBtn.click();
        await page.waitForTimeout(2000);

        await selectedCanvas.click({
            button: 'right',
            position: {
                x: 254,
                y: 5,
            },
            force: true,
        });
        await timelineFrame.getByText('Find in Communication').click({ force: true });
        const hcclChart = communicationFrame.locator('.panel-content').first();
        await hcclChart.waitFor({ state: 'visible' });

        await expect(hcclChart).toHaveScreenshot('redirect-to-communication.png', { maxDiffPixels: 100 });
    });

    // 同通信域泳道置顶
    test('test_db_same_communication_group', async ({ page, timelinePage }) => {
        const { filterBtn, timelineFrame, selectUnitFilterContent } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectUnitFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.setValue('Communication');
        await filterContentSelector.selectOption('Communication');
        await filterBtn.click();
        await page.waitForTimeout(3000);
        await timelineFrame.getByText('mp:Group group_name_13 Communication').click({
            button: 'right',
        });
        await timelineFrame.getByText('Pin Same-named Lane (mp:Group group_name_13 Communication)').click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('communication_group_pin.png', { maxDiffPixels: 100 });
        await timelineFrame.locator('#pinnedUnitWrapperScroller').getByText('mp:Group group_name_13 Communication_Communication (HCCL)_localhost.localdomain2152938157304401006_0 0').click({
            button: 'right',
        });
        await timelineFrame.getByText('Unpin Same-named Lane').click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('communication_group_unpin.png', { maxDiffPixels: 100 });
    });

    // 右键菜单--Show in Events View
    test('test_db_context_menu_click_ShowInEventsView', async ({ timelinePage }) => {
        const { timelineFrame, bottomPanel } = timelinePage;
        await timelineFrame.locator('#unitWrapperScroller').getByText('Ascend Hardware').click();
        await timelineFrame.getByText('Stream 2').click({
            button: 'right',
        });
        await timelineFrame.getByText('Show in Events View').click();
        await timelineFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
        await expect(bottomPanel).toHaveScreenshot('test-db-click-ShowInEventsView.png', { maxDiffPixels: 400 });
    });
});
