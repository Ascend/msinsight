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
import { LeaksPage } from '@/page-object';
import { clearAllData, importData, setupWebSocketListener, waitForWebSocketEvent } from '@/utils';
import { FilePath } from '@/utils/constants';
import { SelectHelpers } from '@/components';

interface TestFixtures {
    leaksPage: LeaksPage;
    ws: Promise<WebSocket>;
}

const test = baseTest.extend<TestFixtures>({
    leaksPage: async ({ page }, use) => {
        const leaksPage = new LeaksPage(page);
        await use(leaksPage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});

test.describe('Leaks(MemScope)', () => {
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, leaksPage, ws }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/leaks/allocations');
        await page.goto('/');
        await importData(page, FilePath.LEAKS_DUMP);
        await leaksPage.goto();
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // 切换线程
    test('test_change_ThreadID', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, threadIdSelector } = leaksPage;
        await page.mouse.click(0, 0);
        await page.waitForTimeout(1000);
        const threadIdSelect = new SelectHelpers(page, threadIdSelector, leaksFrame);
        await threadIdSelect.open();
        await threadIdSelect.selectOption('2622898');
        await page.mouse.move(0, 0);
        await expect(leaksFrame.locator('#funcContent')).toHaveScreenshot('funcContent.png', { maxDiffPixels: 100, animations: 'disabled', timeout: 10000 });
    });

    // 搜索调用栈
    test('test_search_func', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, funcsSelector } = leaksPage;
        const funcsSelect = new SelectHelpers(page, funcsSelector, leaksFrame);
        await funcsSelect.open();
        await funcsSelect.selectOption('<built-in method randn of type object at 0xffff8471f048>');
        await page.mouse.move(0, 0);
        await page.mouse.click(0, 0);
        await page.waitForTimeout(3000);
        await expect(leaksFrame.locator('#funcContent')).toHaveScreenshot('funcSearchContent.png', { maxDiffPixels: 100, animations: 'disabled' });
    });

    // 切换设备ID
    test('test_change_DeviceID', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, deviceIdSelector } = leaksPage;
        const deviceIdSelect = new SelectHelpers(page, deviceIdSelector, leaksFrame);
        await deviceIdSelect.open();
        await deviceIdSelect.selectOption('1');
        await page.mouse.move(0, 0);
        await page.mouse.click(0, 0);
        await page.waitForTimeout(1000);
        const screenshot = await leaksFrame.locator('#barContent').screenshot();
        expect(screenshot).toMatchSnapshot('barContent_deviceId.png', { maxDiffPixels: 100 });
    });

    // 切换类型
    test('test_change_type', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, typeSelector } = leaksPage;
        const typeSelect = new SelectHelpers(page, typeSelector, leaksFrame);
        await typeSelect.open();
        await typeSelect.selectOption('HAL');
        await page.mouse.click(0, 0);
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        const screenshot = await leaksFrame.locator('#barContent').screenshot();
        expect(screenshot).toMatchSnapshot('barContent_type.png', { maxDiffPixels: 100 });
    });

    // 内存拆解图展示
    test('test_memorySlice_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);
        await leaksFrame.locator('#barContent').click({ position: { x: 526, y: 217 } });
        await page.waitForTimeout(1000);
        const screenshot = await leaksFrame.locator('#detailsContent').screenshot();
        expect(screenshot).toMatchSnapshot('detailsContent.png', { maxDiffPixels: 100 });
    });

    // 内存详情表内存块视图
    test('test_blocksTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await leaksFrame.getByText('System View').click();
        const blocksTable = leaksFrame.getByTestId('blocksTable');
        await page.waitForTimeout(1000);
        const screenshot = await blocksTable.screenshot();
        expect(screenshot).toMatchSnapshot('blocksTable.png', { maxDiffPixels: 100 });

        await blocksTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        const screenshotSoter = await blocksTable.screenshot();
        expect(screenshotSoter).toMatchSnapshot('blocksTableSorter.png', { maxDiffPixels: 100 });
    });

    // 内存详情表内存事件视图
    test('test_eventsTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await leaksFrame.getByText('System View').click();
        const eventViewRadio = leaksFrame.getByTestId('eventViewRadio');
        eventViewRadio.click();
        const eventsTable = leaksFrame.getByTestId('eventsTable');
        await page.waitForTimeout(1000);
        const screenshot = await eventsTable.screenshot();
        expect(screenshot).toMatchSnapshot('eventsTable.png', { maxDiffPixels: 100 });

        await eventsTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        const screenshotSoter = await eventsTable.screenshot();
        expect(screenshotSoter).toMatchSnapshot('eventsTableSorter.png', { maxDiffPixels: 100 });
    });

    // 内存块详情
    test('test_memoryBlock_info', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await page.waitForTimeout(1000);
        await leaksFrame.locator('#barContent').click({ position: { x: 526, y: 217 } });
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await page.waitForTimeout(1000);

        const result = ['3182', '20616964997120', '2359808', 'PTA@model@weight', '{"allocation_id":255}', '2621226', '2621226', '1', 'PTA'];
        const sliceInfo = await leaksFrame.locator('.sliceDetailValue').evaluateAll(elements => elements.map(el => el.textContent));
        sliceInfo.forEach((item, index) => {
            expect(item).toBe(result[index]);
        });
    });
});

test.describe('Leaks(snapshot)', () => {
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, leaksPage, ws }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/snapshot/allocations');
        await page.goto('/');
        await importData(page, FilePath.SNAPSHOT_PKL);
        await leaksPage.goto();
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // 内存块图
    test('test_blockDiagram', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await page.mouse.move(0, 0);
        await page.waitForTimeout(3000);
        const blockDiagram = leaksFrame.locator('#barContent');
        const screenshot = await blockDiagram.screenshot();
        expect(screenshot).toMatchSnapshot('snapshot-blockDiagram.png', { maxDiffPixels: 100 });
    });

    // 内存状态图
    test('test_stateDiagram', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        const screenshot = await leaksFrame.getByTestId('stateDiagram').screenshot();
        expect(screenshot).toMatchSnapshot('snapshot-stateDiagram.png', { maxDiffPixels: 100 });
    });

    // 内存详情表内存块视图
    test('test_blocksTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await leaksFrame.getByText('System View').click();
        const blocksTable = leaksFrame.getByTestId('blocksTable');
        await page.waitForTimeout(1000);
        const screenshot = await blocksTable.screenshot();
        expect(screenshot).toMatchSnapshot('snapshot-blocksTable.png', { maxDiffPixels: 100 });

        await blocksTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        const screenshotSoter = await blocksTable.screenshot();
        expect(screenshotSoter).toMatchSnapshot('snapshot-blocksTableSorter.png', { maxDiffPixels: 100 });
    });

    // 内存详情表内存事件视图
    test('test_eventsTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await leaksFrame.getByText('System View').click();
        const eventViewRadio = leaksFrame.getByTestId('eventViewRadio');
        eventViewRadio.click();
        const eventsTable = leaksFrame.getByTestId('eventsTable');
        await page.waitForTimeout(1000);
        const screenshot = await eventsTable.screenshot();
        expect(screenshot).toMatchSnapshot('snapshot-eventsTable.png', { maxDiffPixels: 100 });

        await eventsTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        const screenshotSoter = await eventsTable.screenshot();
        expect(screenshotSoter).toMatchSnapshot('snapshot-eventsTableSorter.png', { maxDiffPixels: 100 });
    });

    // 事件列表
    test('test_eventList', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        await page.waitForTimeout(3000);
        const { leaksFrame } = leaksPage;
        const eventList = leaksFrame.locator('.ant-table-body');
        const screenshot = await eventList.screenshot();
        expect(screenshot).toMatchSnapshot('snapshot-eventList.png', { maxDiffPixels: 100 });

        eventList.locator('td').nth(3).click();
        await page.waitForTimeout(500);
        const screenshotSelect = await eventList.screenshot();
        expect(screenshotSelect).toMatchSnapshot('snapshot-eventListSelect.png', { maxDiffPixels: 100 });
    });

    // 内存块详情
    test('test_memoryBlock_info', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        await page.waitForTimeout(3000);
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('#barContent').click({ position: { x: 670, y: 175 } });
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();

        const sliceDetail = leaksFrame.locator('.sliceDetailValue');
        const sliceInfoResult = ['2968', '3.634', '3.634', '20697727156224', '+ detail', '+ detail', '+ detail'];
        const sliceInfo = await sliceDetail.evaluateAll(elements => elements.map(el => el.textContent));
        sliceInfo.forEach((item, index) => {
            expect(item).toBe(sliceInfoResult[index]);
        });

        sliceDetail.last().locator('button').click();
        const completedEventInfo = await leaksFrame.locator('.sliceDetailValue .sliceDetailValue').evaluateAll(elements => elements.map(el => el.textContent));
        completedEventInfo.pop(); // 调用栈信息过长，不做对比
        const completedEventInfoResult = ['4478', 'free_completed', '0x12d310acc000', '3.634', '1276474240', '466.79', '466.79', '554'];
        completedEventInfo.forEach((item, index) => {
            expect(item).toBe(completedEventInfoResult[index]);
        });
    });

    // 事件详情
    test('test_event_info', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        await page.waitForTimeout(3000);
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('.ant-table-body').locator('td').nth(5).click();
        await page.waitForTimeout(500);
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();

        const sliceDetail = leaksFrame.locator('.sliceDetailValue');
        const sliceInfoResult = ['4', 'alloc', '0x12d307bc8000', '3.634', '1276474240', '134.423', '134.423', '148'];
        const sliceInfo = await sliceDetail.evaluateAll(elements => elements.map(el => el.textContent));
        sliceInfo.pop(); // 调用栈信息过长，不做对比
        sliceInfo.forEach((item, index) => {
            expect(item).toBe(sliceInfoResult[index]);
        });
    });
});
