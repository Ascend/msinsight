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

import { test as baseTest, expect } from '@playwright/test';
import { MemScopePage } from '@/page-object';
import { importData, waitForWebSocketEvent } from '@/utils';
import { FilePath } from '@/utils/constants';

interface TestFixtures {
    memScopePage: MemScopePage;
}

const test = baseTest.extend<TestFixtures>({
    memScopePage: async ({ page }, use) => {
        await use(new MemScopePage(page));
    },
});

test.describe('MemScope', () => {
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, memScopePage }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/leaks/allocations');
        await page.goto('/');
        await importData(page, FilePath.MEMSCOPE_DB);
        await memScopePage.goto();
        await getAllocationsSuccess;
        await memScopePage.waitForReady();
    });

    test.afterEach(async ({ memScopePage }, testInfo) => {
        try {
            if (testInfo.status !== testInfo.expectedStatus) {
                await testInfo.attach('memscope-ui', { body: await memScopePage.memoryFrame.locator('body').ariaSnapshot(), contentType: 'text/plain' });
            }
        } finally {
            await memScopePage.clearData();
        }
    });

    // 切换线程
    test('test_change_ThreadID', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame, threadIdSelector } = memScopePage;
        await page.mouse.click(0, 0);
        await page.waitForTimeout(1000);
        await memScopePage.selectOption(threadIdSelector, '2622898');
        await expect(memoryFrame.locator('.ant-select-selection-item').and(memoryFrame.getByTitle('2622898', { exact: true }))).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('#funcContent')).toHaveScreenshot('funcContent.png', { maxDiffPixels: 100, animations: 'disabled', timeout: 10000 });
    });

    // 搜索调用栈
    test('test_search_func', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame, funcsSelector } = memScopePage;
        await memScopePage.selectOption(memScopePage.threadIdSelector, '2622898');
        await funcsSelector.click();
        const menu = memoryFrame.locator('.ant-select-dropdown')
            .filter({ has: memoryFrame.locator('#select-funcName_list') });
        const option = menu.locator('.ant-select-item-option').first();
        await expect(option).toBeVisible({ timeout: 30000 });
        const functionName = await option.getAttribute('title');
        if (!functionName) throw new Error('Function option must have a name');
        await funcsSelector.fill(functionName.split(': ').pop() ?? functionName);
        await funcsSelector.press('Enter');
        await expect(memoryFrame.locator('.ant-select-selection-item')
            .and(memoryFrame.getByTitle(functionName, { exact: true }))).toBeVisible();
        await page.mouse.move(0, 0);
        await page.mouse.click(0, 0);
        await page.waitForTimeout(3000);
        await expect(memoryFrame.locator('#funcContent')).toHaveScreenshot('funcSearchContent.png', { maxDiffPixels: 100, animations: 'disabled' });
    });

    // 当前测试数据只有设备 1，验证可选设备及选中后的图形。
    test('test_device_options', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame, deviceIdSelector } = memScopePage;
        await memScopePage.selectOption(deviceIdSelector, '1');
        await expect(memoryFrame.locator('.ant-select-selection-item').and(memoryFrame.getByTitle('1', { exact: true }))).toBeVisible();
        await page.mouse.move(0, 0);
        await page.mouse.click(0, 0);
        await memScopePage.waitForReady();
        await page.mouse.move(0, 0);
        const overview = memoryFrame.getByTestId('memoryDataZoom');
        await expect(overview.locator('canvas')).toHaveCount(2);
        // 图表初始化后重新布局，补齐首次渲染可能遗漏的完成通知。
        await page.waitForTimeout(1000);
        await memoryFrame.locator('body').evaluate(() => window.dispatchEvent(new Event('resize')));
        await expect(overview).toHaveAttribute('data-loading', 'false', { timeout: 30000 });
        await expect(memoryFrame.locator('#barContent')).toHaveScreenshot('barContent_deviceId.png', {
            maxDiffPixels: 100,
            animations: 'disabled',
        });
    });

    // 切换类型
    test('test_change_type', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame, typeSelector } = memScopePage;
        await memScopePage.selectOption(typeSelector, 'HAL');
        await expect(memoryFrame.locator('.ant-select-selection-item').and(memoryFrame.getByTitle('HAL', { exact: true }))).toBeVisible();
        await page.mouse.click(0, 0);
        await page.mouse.move(0, 0);
        await memScopePage.waitForReady();
        await page.mouse.move(0, 0);
        const overview = memoryFrame.getByTestId('memoryDataZoom');
        await expect(overview.locator('canvas')).toHaveCount(2);
        // 图表初始化后重新布局，补齐首次渲染可能遗漏的完成通知。
        await page.waitForTimeout(1000);
        await memoryFrame.locator('body').evaluate(() => window.dispatchEvent(new Event('resize')));
        await expect(overview).toHaveAttribute('data-loading', 'false', { timeout: 30000 });
        await expect(memoryFrame.locator('#barContent')).toHaveScreenshot('barContent_type.png', {
            maxDiffPixels: 100,
            animations: 'disabled',
        });
    });

    // 内存拆解图展示
    test('test_memorySlice_show', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memScopePage;
        await memScopePage.clickMemoryBlock();
        await expect(memoryFrame.locator('#detailsContent canvas').first()).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('#detailsContent')).toHaveScreenshot('detailsContent.png', { maxDiffPixels: 100, animations: 'disabled' });
    });

    // 内存详情表内存块视图
    test('test_blocksTable_show', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memScopePage;
        await memScopePage.openSystemView();
        const blocksTable = memoryFrame.getByTestId('blocksTable');
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(blocksTable).toHaveScreenshot('blocksTable.png', { maxDiffPixels: 100 });

        const sizeHeader = blocksTable.getByRole('columnheader').filter({ hasText: /^Size/ });
        await sizeHeader.click();
        await expect(sizeHeader).toHaveAttribute('aria-sort', 'ascending');

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(blocksTable).toHaveScreenshot('blocksTableSorter.png', { maxDiffPixels: 100 });

        await sizeHeader.click();
        await expect(sizeHeader).toHaveAttribute('aria-sort', 'descending');
        await expect.poll(async () => {
            const values = await blocksTable.locator('tbody tr[data-row-key]')
                .filter({ has: memoryFrame.locator('.locate-link') })
                .evaluateAll(rows => rows.map(row => {
                    const sizeCell = row.querySelector('.locate-link')?.closest('td')?.nextElementSibling?.nextElementSibling;
                    return Number(sizeCell?.textContent?.replace(/,/g, ''));
                }));
            return values.length > 1 && values.every((value, index) =>
                Number.isFinite(value) && (index === 0 || values[index - 1] >= value));
        }).toBe(true);
    });

    // 内存详情表内存事件视图
    test('test_eventsTable_show', async ({ memScopePage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memScopePage;
        await memoryFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await memoryFrame.getByText('System View').click();
        const eventViewRadio = memoryFrame.getByTestId('eventViewRadio');
        await eventViewRadio.click();
        await expect(memoryFrame.getByTestId('eventsTable').locator('tbody tr.ant-table-row').first()).toContainText(/\d/);
        const eventsTable = memoryFrame.getByTestId('eventsTable');
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('eventsTable.png', { maxDiffPixels: 100 });

        await eventsTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('eventsTableSorter.png', { maxDiffPixels: 100 });

        await memoryFrame.getByTestId('blockViewRadio').click();
        await expect(memoryFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
    });

    // 内存块详情
    test('test_memoryBlock_info', async ({ memScopePage }) => {
        await memScopePage.clickMemoryBlock();
        const panel = await memScopePage.selectedDetailPanel();
        await expect(panel.getByTitle('Size(bytes)', { exact: true })).toBeVisible();
    });

    test('switch zoom mode', async ({ memScopePage }) => {
        const graph = memScopePage.memoryFrame.locator('#barContent');
        await graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true })).toBeVisible();
        await graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true })).toBeVisible();
    });

    test('toggle reserved memory legend', async ({ memScopePage, page }) => {
        await expect(memScopePage.memoryFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('memscope-reserved-line-visible.png', { maxDiffPixels: 100, timeout: 30000 });
        const legend = memScopePage.memoryFrame.getByTestId('allocationLineLegend').getByRole('button', { name: 'PTA Reserved', exact: true });
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'false');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(memScopePage.memoryFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('memscope-reserved-line-hidden.png', { maxDiffPixels: 100, timeout: 30000 });
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(memScopePage.memoryFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('memscope-reserved-line-visible.png', { maxDiffPixels: 100 });
    });

    test('locate block and show matching detail', async ({ memScopePage }) => {
        const frame = memScopePage.memoryFrame;
        await memScopePage.openSystemView();
        const link = frame.getByTestId('blocksTable').locator('tbody .locate-link').first();
        const id = (await link.innerText()).trim();
        const address = (await link.locator('xpath=ancestor::td/following-sibling::td[1]').innerText()).trim();
        expect(id).toMatch(/^-?\d+$/);
        expect(address).toMatch(/^\d+$/);
        await link.click();
        const tab = frame.getByRole('tab', { name: `#${id}`, exact: true });
        await expect(tab).toBeVisible({ timeout: 30000 });
        await tab.click();
        const panelId = await tab.getAttribute('aria-controls');
        if (!panelId) throw new Error('Block detail tab must reference a panel');
        await expect(frame.locator(`[id="${panelId}"]`)).toContainText(address);
    });

    test('cancel inefficient memory filter settings', async ({ memScopePage }) => {
        const frame = memScopePage.memoryFrame;
        await memScopePage.openSystemView();
        await frame.getByRole('button', { name: 'Filter Inefficient Memory Blocks', exact: true }).click();
        const modal = frame.getByTestId('thresholdModal').getByRole('dialog');
        await expect(modal).toBeVisible();
        const threshold = modal.getByTestId('lazyThrePer');
        const original = await threshold.inputValue();
        await threshold.fill('50');
        await modal.getByRole('button', { name: 'Cancel', exact: true }).click();
        await expect(modal).toBeHidden();
        await frame.getByRole('button', { name: 'Filter Inefficient Memory Blocks', exact: true }).click();
        await expect(modal.getByTestId('lazyThrePer')).toHaveValue(original);
        await modal.getByRole('button', { name: 'Cancel', exact: true }).click();
    });

    test('toggle flame graph trimming', async ({ memScopePage, page }) => {
        const checkbox = memScopePage.memoryFrame.getByRole('checkbox', { name: 'Allow Trim', exact: true });
        await expect(checkbox).toBeChecked();
        await checkbox.uncheck();
        await expect(checkbox).not.toBeChecked();
        await page.mouse.move(0, 0);
        await expect(memScopePage.memoryFrame.locator('#funcContent')).toHaveScreenshot('memscope-flame-untrimmed.png', { maxDiffPixels: 100, timeout: 30000 });
        await checkbox.check();
        await expect(checkbox).toBeChecked();
        await page.mouse.move(0, 0);
        await expect(memScopePage.memoryFrame.locator('#funcContent')).toHaveScreenshot('memscope-flame-trimmed.png', { maxDiffPixels: 100, timeout: 30000 });
    });
});
