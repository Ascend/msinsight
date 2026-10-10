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

import { test as baseTest, expect } from '@playwright/test';
import { LeaksPage } from '@/page-object';
import { importData, waitForWebSocketEvent } from '@/utils';
import { FilePath } from '@/utils/constants';

interface TestFixtures {
    leaksPage: LeaksPage;
}

const test = baseTest.extend<TestFixtures>({
    leaksPage: async ({ page }, use) => {
        const leaksPage = new LeaksPage(page);
        await use(leaksPage);
    },
});

test.describe('Leaks(MemScope)', () => {
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, leaksPage }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/leaks/allocations');
        await page.goto('/');
        await importData(page, FilePath.LEAKS_DUMP);
        await leaksPage.goto();
        await getAllocationsSuccess;
        await leaksPage.waitForReady();
    });

    test.afterEach(async ({ leaksPage }, testInfo) => {
        if (testInfo.status !== testInfo.expectedStatus) {
            await testInfo.attach('leaks-ui', { body: await leaksPage.leaksFrame.locator('body').ariaSnapshot(), contentType: 'text/plain' });
        }
        await leaksPage.clearData();
    });

    // 切换线程
    test('test_change_ThreadID', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, threadIdSelector } = leaksPage;
        await page.mouse.click(0, 0);
        await page.waitForTimeout(1000);
        await leaksPage.selectOption(threadIdSelector, '2622898');
        await expect(leaksFrame.locator('.ant-select-selection-item').and(leaksFrame.getByTitle('2622898', { exact: true }))).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(leaksFrame.locator('#funcContent')).toHaveScreenshot('funcContent.png', { maxDiffPixels: 100, animations: 'disabled', timeout: 10000 });
    });

    // 搜索调用栈
    test('test_search_func', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, funcsSelector } = leaksPage;
        await leaksPage.selectOption(leaksPage.threadIdSelector, '2622898');
        await funcsSelector.click();
        const menu = leaksFrame.locator('.ant-select-dropdown')
            .filter({ has: leaksFrame.locator('#select-funcName_list') });
        const option = menu.locator('.ant-select-item-option').first();
        await expect(option).toBeVisible({ timeout: 30000 });
        const functionName = await option.getAttribute('title');
        if (!functionName) throw new Error('Function option must have a name');
        await funcsSelector.fill(functionName.split(': ').pop() ?? functionName);
        await funcsSelector.press('Enter');
        await expect(leaksFrame.locator('.ant-select-selection-item')
            .and(leaksFrame.getByTitle(functionName, { exact: true }))).toBeVisible();
        await page.mouse.move(0, 0);
        await page.mouse.click(0, 0);
        await page.waitForTimeout(3000);
        await expect(leaksFrame.locator('#funcContent')).toHaveScreenshot('funcSearchContent.png', { maxDiffPixels: 100, animations: 'disabled' });
    });

    // 当前测试数据只有设备 1，验证可选设备及选中后的图形。
    test('test_device_options', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, deviceIdSelector } = leaksPage;
        await leaksPage.selectOption(deviceIdSelector, '1');
        await expect(leaksFrame.locator('.ant-select-selection-item').and(leaksFrame.getByTitle('1', { exact: true }))).toBeVisible();
        await page.mouse.move(0, 0);
        await page.mouse.click(0, 0);
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(leaksFrame.locator('#barContent')).toHaveScreenshot('barContent_deviceId.png', { maxDiffPixels: 100 });
    });

    // 切换类型
    test('test_change_type', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame, typeSelector } = leaksPage;
        await leaksPage.selectOption(typeSelector, 'HAL');
        await expect(leaksFrame.locator('.ant-select-selection-item').and(leaksFrame.getByTitle('HAL', { exact: true }))).toBeVisible();
        await page.mouse.click(0, 0);
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(leaksFrame.locator('#barContent')).toHaveScreenshot('barContent_type.png', { maxDiffPixels: 100 });
    });

    // 内存拆解图展示
    test('test_memorySlice_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);
        await leaksFrame.locator('#barContent').click({ position: { x: 526, y: 217 } });
        await expect(leaksFrame.locator('#detailsContent canvas').first()).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(leaksFrame.locator('#detailsContent')).toHaveScreenshot('detailsContent.png', { maxDiffPixels: 100, animations: 'disabled' });
    });

    // 内存详情表内存块视图
    test('test_blocksTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksPage.openSystemView();
        const blocksTable = leaksFrame.getByTestId('blocksTable');
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
                .filter({ has: leaksFrame.locator('.locate-link') })
                .evaluateAll(rows => rows.map(row => {
                    const sizeCell = row.querySelector('.locate-link')?.closest('td')?.nextElementSibling?.nextElementSibling;
                    return Number(sizeCell?.textContent?.replace(/,/g, ''));
                }));
            return values.length > 1 && values.every((value, index) =>
                Number.isFinite(value) && (index === 0 || values[index - 1] >= value));
        }).toBe(true);
    });

    // 内存详情表内存事件视图
    test('test_eventsTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await leaksFrame.getByText('System View').click();
        const eventViewRadio = leaksFrame.getByTestId('eventViewRadio');
        await eventViewRadio.click();
        await expect(leaksFrame.getByTestId('eventsTable').locator('tbody tr.ant-table-row').first()).toContainText(/\d/);
        const eventsTable = leaksFrame.getByTestId('eventsTable');
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('eventsTable.png', { maxDiffPixels: 100 });

        await eventsTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('eventsTableSorter.png', { maxDiffPixels: 100 });

        await leaksFrame.getByTestId('blockViewRadio').click();
        await expect(leaksFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
    });

    // 内存块详情
    test('test_memoryBlock_info', async ({ leaksPage }) => {
        await leaksPage.clickMemoryBlock();
        const panel = await leaksPage.selectedDetailPanel();
        await expect(panel.getByTitle('Size(bytes)', { exact: true })).toBeVisible();
    });

    test('switch zoom mode', async ({ leaksPage }) => {
        const graph = leaksPage.leaksFrame.locator('#barContent');
        await graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true })).toBeVisible();
        await graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true })).toBeVisible();
    });

    test('toggle reserved memory legend', async ({ leaksPage, page }) => {
        await expect(leaksPage.leaksFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('memscope-reserved-line-visible.png', { maxDiffPixels: 100, timeout: 30000 });
        const legend = leaksPage.leaksFrame.getByTestId('allocationLineLegend').getByRole('button', { name: 'PTA Reserved', exact: true });
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'false');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(leaksPage.leaksFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('memscope-reserved-line-hidden.png', { maxDiffPixels: 100, timeout: 30000 });
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(leaksPage.leaksFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('memscope-reserved-line-visible.png', { maxDiffPixels: 100 });
    });

    test('locate block and show matching detail', async ({ leaksPage }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.openSystemView();
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

    test('cancel inefficient memory filter settings', async ({ leaksPage }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.openSystemView();
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

    test('toggle flame graph trimming', async ({ leaksPage, page }) => {
        const checkbox = leaksPage.leaksFrame.getByRole('checkbox', { name: 'Allow Trim', exact: true });
        await expect(checkbox).toBeChecked();
        await checkbox.uncheck();
        await expect(checkbox).not.toBeChecked();
        await page.mouse.move(0, 0);
        await expect(leaksPage.leaksFrame.locator('#funcContent')).toHaveScreenshot('memscope-flame-untrimmed.png', { maxDiffPixels: 100, timeout: 30000 });
        await checkbox.check();
        await expect(checkbox).toBeChecked();
        await page.mouse.move(0, 0);
        await expect(leaksPage.leaksFrame.locator('#funcContent')).toHaveScreenshot('memscope-flame-trimmed.png', { maxDiffPixels: 100, timeout: 30000 });
    });
});

test.describe('Leaks(snapshot)', () => {
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, leaksPage }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/snapshot/allocations');
        await page.goto('/');
        await importData(page, FilePath.SNAPSHOT_PKL);
        await leaksPage.goto();
        await getAllocationsSuccess;
        await leaksPage.waitForReady();
    });

    test.afterEach(async ({ leaksPage }, testInfo) => {
        if (testInfo.status !== testInfo.expectedStatus) {
            await testInfo.attach('leaks-ui', { body: await leaksPage.leaksFrame.locator('body').ariaSnapshot(), contentType: 'text/plain' });
        }
        await leaksPage.clearData();
    });

    // 内存块图
    test('test_blockDiagram', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await page.mouse.move(0, 0);
        await page.waitForTimeout(3000);
        const blockDiagram = leaksFrame.locator('#barContent');
        await page.mouse.move(0, 0);
        await expect(blockDiagram).toHaveScreenshot('snapshot-blockDiagram.png', { maxDiffPixels: 100 });
    });

    // 内存状态图
    test('test_stateDiagram', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(leaksFrame.getByTestId('stateDiagram')).toHaveScreenshot('snapshot-stateDiagram.png', { maxDiffPixels: 100 });
    });

    // 内存详情表内存块视图
    test('test_blocksTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksPage.openSystemView();
        const blocksTable = leaksFrame.getByTestId('blocksTable');
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(blocksTable).toHaveScreenshot('snapshot-blocksTable.png', { maxDiffPixels: 100 });

        const sizeHeader = blocksTable.getByRole('columnheader').filter({ hasText: /^Size/ });
        await sizeHeader.click();
        await expect(sizeHeader).toHaveAttribute('aria-sort', 'ascending');

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(blocksTable).toHaveScreenshot('snapshot-blocksTableSorter.png', { maxDiffPixels: 100 });

        await sizeHeader.click();
        await expect(sizeHeader).toHaveAttribute('aria-sort', 'descending');
        await expect.poll(async () => {
            const values = await blocksTable.locator('tbody tr[data-row-key]')
                .filter({ has: leaksFrame.locator('.locate-link') })
                .evaluateAll(rows => rows.map(row => {
                    const sizeCell = row.querySelector('.locate-link')?.closest('td')?.nextElementSibling?.nextElementSibling;
                    return Number(sizeCell?.textContent?.replace(/,/g, ''));
                }));
            return values.length > 1 && values.every((value, index) =>
                Number.isFinite(value) && (index === 0 || values[index - 1] >= value));
        }).toBe(true);
    });

    // 内存详情表内存事件视图
    test('test_eventsTable_show', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        const { leaksFrame } = leaksPage;
        await leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await leaksFrame.getByText('System View').click();
        const eventViewRadio = leaksFrame.getByTestId('eventViewRadio');
        await eventViewRadio.click();
        await expect(leaksFrame.getByTestId('eventsTable').locator('tbody tr.ant-table-row').first()).toContainText(/\d/);
        const eventsTable = leaksFrame.getByTestId('eventsTable');
        await page.waitForTimeout(2000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('snapshot-eventsTable.png', { maxDiffPixels: 100 });

        await eventsTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('snapshot-eventsTableSorter.png', { maxDiffPixels: 100 });

        await leaksFrame.getByTestId('blockViewRadio').click();
        await expect(leaksFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
    });

    // 事件列表
    test('test_eventList', async ({ leaksPage, page }) => {
        await getAllocationsSuccess;
        await page.waitForTimeout(3000);
        const { leaksFrame } = leaksPage;
        const eventList = leaksFrame.locator('.table-slice-list .ant-table-body');
        await page.mouse.move(0, 0);
        await expect(eventList).toHaveScreenshot('snapshot-eventList.png', { maxDiffPixels: 100 });

        const row = eventList.locator('tr.ant-table-row').nth(1);
        await row.click();
        await expect(row).toHaveClass(/click-select/);
        await expect(leaksFrame.getByRole('tab', { name: /^#\d+$/ }).first()).toBeVisible();
        // 选择事件会自动展开详情，收起后对比整个事件列表及其选中状态。
        const detailToggle = leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]');
        await detailToggle.click();
        await expect(detailToggle.locator('..')).toHaveCSS('height', '38px');
        await expect(row).toHaveClass(/click-select/);
        await page.mouse.move(0, 0);
        await page.waitForTimeout(500);
        await page.mouse.move(0, 0);
        await expect(eventList).toHaveScreenshot('snapshot-eventListSelect.png', { maxDiffPixels: 100 });
    });

    // 内存块详情
    test('test_memoryBlock_info', async ({ leaksPage }) => {
        await leaksPage.clickMemoryBlock();
        const panel = await leaksPage.selectedDetailPanel();
        await expect(panel.getByTitle('Size(MBytes)', { exact: true }).locator('xpath=following-sibling::*[1]')).toHaveText(/^\d+(?:\.\d+)?$/);
    });

    // 事件详情
    test('test_event_info', async ({ leaksPage, page }) => {
        const { leaksFrame } = leaksPage;
        const row = leaksFrame.locator('.table-slice-list tr.ant-table-row').nth(1);
        await expect(row).toBeVisible();
        const rowKey = await row.getAttribute('data-row-key');
        if (!rowKey) throw new Error('Event row must have an ID');
        const eventId = rowKey.split('_')[0];
        await row.click();
        await expect(row).toHaveClass(/click-select/);
        await page.mouse.move(0, 0);
        await expect(leaksFrame.getByRole('tab', { name: '#' + eventId, exact: true })).toBeVisible();
        const panel = await leaksPage.selectedDetailPanel();
        await expect(panel.getByTitle('Action', { exact: true }).locator('xpath=following-sibling::*[1]')).toHaveText('alloc');
    });

    test('switch zoom mode', async ({ leaksPage }) => {
        const graph = leaksPage.leaksFrame.locator('#barContent');
        await graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true })).toBeVisible();
        await graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true })).toBeVisible();
    });

    test('toggle reserved memory legend', async ({ leaksPage, page }) => {
        await expect(leaksPage.leaksFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('snapshot-reserved-line-visible.png', { maxDiffPixels: 100 });
        const legend = leaksPage.leaksFrame.getByTestId('allocationLineLegend').getByRole('button', { name: 'PTA Reserved', exact: true });
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'false');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(2000);
        await expect(leaksPage.leaksFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('snapshot-reserved-line-hidden.png', { maxDiffPixels: 100 });
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(leaksPage.leaksFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('snapshot-reserved-line-visible.png', { maxDiffPixels: 100 });
    });

    test('locate block and show matching detail', async ({ leaksPage }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.openSystemView();
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

    test('open and close window overview using keyboard', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        const expandButton = frame.getByRole('button', { name: 'Enlarge', exact: true });
        await expandButton.focus();
        await expandButton.press('Enter');
        await expect(expandButton).toHaveAttribute('aria-expanded', 'true');
        const dialog = frame.getByRole('dialog', { name: 'Memory window overview', exact: true });
        await expect(dialog).toBeVisible();
        await expect(dialog.getByTestId('overviewCanvas')).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(dialog.getByTestId('overviewCanvas')).toHaveScreenshot('snapshot-expanded-overview.png', { maxDiffPixels: 100 });
        await dialog.getByRole('button', { name: 'Close', exact: true }).click();
        await expect(dialog).toBeHidden();
    });

    test('toggle unreleased blocks filter', async ({ leaksPage }) => {
        await leaksPage.openSystemView();
        const checkbox = leaksPage.leaksFrame.getByRole('checkbox', { name: 'Auto Filter Unreleased in Range', exact: true });
        const initial = await checkbox.isChecked();
        await checkbox.setChecked(!initial);
        await expect(checkbox).toBeChecked({ checked: !initial });
        await expect(leaksPage.leaksFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
        await checkbox.setChecked(initial);
        await expect(checkbox).toBeChecked({ checked: initial });
    });

    test('open and close marker manager', async ({ leaksPage }) => {
        const frame = leaksPage.leaksFrame;
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await expect(frame.getByTestId('memoryMarkerManagerFloatingPanel')).toBeVisible();
        await frame.getByRole('button', { name: 'Close marker management', exact: true }).click();
        await expect(frame.getByTestId('memoryMarkerManagerFloatingPanel')).toBeHidden();
    });

    test('overview shows window boundaries and focused range', async ({ leaksPage, page }) => {
        const overview = leaksPage.leaksFrame.getByTestId('snapshotSliceOverview');
        await expect(overview).toBeVisible();
        await expect(overview.getByTestId('sliceAxisCoordinate')).toHaveCount(2);
        await expect(overview.getByRole('slider', { name: 'Range start', exact: true })).toBeAttached();
        await expect(overview.getByRole('slider', { name: 'Range end', exact: true })).toBeAttached();
        await expect(overview.getByRole('button', { name: /^Window 1 · Peak:/ })).toHaveAttribute('aria-pressed', 'true');
        await page.mouse.move(0, 0);
        await expect(overview).toHaveScreenshot('snapshot-window-overview.png', { maxDiffPixels: 100 });
    });

    for (const [label, attribute] of [
        ['Memory blocks', 'data-block-layer-visible'],
        ['Memory usage lines', 'data-overview-layer-visible'],
        ['Difference markers', 'data-marker-layer-visible'],
    ]) {
        test(`toggle ${label} layer`, async ({ leaksPage, page }) => {
            const frame = leaksPage.leaksFrame;
            const diagram = frame.getByTestId('blockDiagramSection');
            const isMarkerLayer = attribute === 'data-marker-layer-visible';
            if (isMarkerLayer) {
                await frame.getByRole('button', { name: 'Click the marker axis to add a memory marker', exact: true }).click();
                await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(1);
                await page.mouse.move(0, 0);
            }
            const graph = isMarkerLayer ? diagram : diagram.locator('canvas');
            await expect(graph).toHaveScreenshot(`snapshot-${attribute}-visible.png`, { maxDiffPixels: 100, timeout: 30000 });
            await frame.getByRole('button', { name: 'Layer management', exact: true }).click();
            const toggle = frame.getByTestId('lifecycleGraphLayerPanel').getByRole('button', { name: new RegExp(label) });
            await expect(toggle).toHaveAttribute('aria-pressed', 'true');
            await toggle.click();
            await expect(frame.getByTestId('blockDiagramSection')).toHaveAttribute(attribute, 'false');
            if (isMarkerLayer) await expect(frame.getByTestId('lifecycleMemoryMarkerOverlay')).toHaveCount(0);
            await frame.getByRole('button', { name: 'Layer management', exact: true }).click();
            await expect(frame.getByTestId('lifecycleGraphLayerPanel')).toBeHidden();
            await page.waitForTimeout(1000);
            await page.mouse.move(0, 0);
            await expect(graph).toHaveScreenshot(`snapshot-${attribute}-hidden.png`, { maxDiffPixels: 100 });
            await frame.getByRole('button', { name: 'Layer management', exact: true }).click();
            await toggle.click();
            await expect(frame.getByTestId('blockDiagramSection')).toHaveAttribute(attribute, 'true');
            if (isMarkerLayer) await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(1);
            await frame.getByRole('button', { name: 'Layer management', exact: true }).click();
            await expect(frame.getByTestId('lifecycleGraphLayerPanel')).toBeHidden();
            await page.waitForTimeout(1000);
            await page.mouse.move(0, 0);
            await expect(graph).toHaveScreenshot(`snapshot-${attribute}-visible.png`, { maxDiffPixels: 100 });
        });
    }

    test('open and close interaction guide', async ({ leaksPage }) => {
        const frame = leaksPage.leaksFrame;
        await frame.getByRole('button', { name: 'Lifecycle graph toolbar guide', exact: true }).click();
        await expect(frame.getByTestId('lifecycleGraphInteractionGuide')).toBeVisible();
        await frame.getByRole('button', { name: 'Close lifecycle graph toolbar guide', exact: true }).click();
        await expect(frame.getByTestId('lifecycleGraphInteractionGuide')).toBeHidden();
    });

    test('create two markers and show their memory difference', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.addMemoryMarker(120);
        await leaksPage.addMemoryMarker(280);
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(2);
        await expect(frame.getByTestId('memoryMarkerGapSegment')).toHaveCount(1);
        await expect(frame.getByTestId('blockDiagramSection')).toHaveScreenshot('snapshot-two-markers.png', { maxDiffPixels: 100 });
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        const manager = frame.getByTestId('memoryMarkerManagerFloatingPanel');
        await expect(manager.getByTestId('memoryMarkerManagerRow')).toHaveCount(2);
        await expect(manager.getByTestId('memoryMarkerManagerGap')).toContainText(/\d.*(?:MB|KB|B)/);
        await page.mouse.move(0, 0);
        await expect(manager).toHaveScreenshot('snapshot-marker-difference-manager.png', { maxDiffPixels: 100 });
    });

    test('rename marker and retain its name after reopening manager', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.addMemoryMarker();
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        const manager = frame.getByTestId('memoryMarkerManagerFloatingPanel');
        const name = manager.getByRole('textbox', { name: 'Rename Flag 1', exact: true });
        await name.fill('Peak checkpoint');
        await name.press('Enter');
        await frame.getByRole('button', { name: 'Close marker management', exact: true }).click();
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await expect(name).toHaveValue('Peak checkpoint');
        await page.mouse.move(0, 0);
        await expect(manager).toHaveScreenshot('snapshot-renamed-marker.png', { maxDiffPixels: 100 });
    });

    test('hide and restore one marker without deleting it', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.addMemoryMarker();
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        const manager = frame.getByTestId('memoryMarkerManagerFloatingPanel');
        await manager.getByRole('button', { name: 'Hide Flag 1', exact: true }).click();
        await expect(manager.getByTestId('memoryMarkerManagerRow')).toHaveCount(1);
        await expect(manager.getByRole('button', { name: 'Show Flag 1', exact: true })).toHaveAttribute('aria-pressed', 'true');
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(0);
        await frame.getByRole('button', { name: 'Close marker management', exact: true }).click();
        await page.mouse.move(0, 0);
        await expect(frame.getByTestId('blockDiagramSection')).toHaveScreenshot('snapshot-single-marker-hidden.png', { maxDiffPixels: 100 });
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await manager.getByRole('button', { name: 'Show Flag 1', exact: true }).click();
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(1);
        await frame.getByRole('button', { name: 'Close marker management', exact: true }).click();
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(frame.getByTestId('blockDiagramSection')).toHaveScreenshot('snapshot-single-marker-restored.png', { maxDiffPixels: 100 });
    });

    test('delete one marker and keep the other marker', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.addMemoryMarker(120);
        await leaksPage.addMemoryMarker(280);
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        const manager = frame.getByTestId('memoryMarkerManagerFloatingPanel');
        await manager.getByRole('button', { name: 'Delete memory marker: Flag 1', exact: true }).click();
        await expect(manager.getByTestId('memoryMarkerManagerRow')).toHaveCount(1);
        await expect(manager.getByRole('textbox', { name: 'Rename Flag 2', exact: true })).toBeVisible();
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(1);
        await expect(frame.getByTestId('memoryMarkerGapSegment')).toHaveCount(0);
        await frame.getByRole('button', { name: 'Close marker management', exact: true }).click();
        await page.mouse.move(0, 0);
        await expect(frame.getByTestId('blockDiagramSection')).toHaveScreenshot('snapshot-marker-after-delete.png', { maxDiffPixels: 100 });
    });

    test('cancel then confirm clearing all markers', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.addMemoryMarker(120);
        await leaksPage.addMemoryMarker(280);
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        const manager = frame.getByTestId('memoryMarkerManagerFloatingPanel');
        await manager.getByRole('button', { name: 'Clear all', exact: true }).click();
        const dialog = frame.getByRole('dialog').filter({ hasText: 'Clear all flags in this context?' });
        await dialog.getByRole('button', { name: 'Cancel', exact: true }).click();
        await expect(dialog).toBeHidden({ timeout: 15000 });
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(2);
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await manager.getByRole('button', { name: 'Clear all', exact: true }).click();
        await dialog.getByRole('button', { name: 'OK', exact: true }).click();
        await expect(dialog).toBeHidden({ timeout: 15000 });
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(0);
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await expect(manager).toContainText('No flags');
        await expect(manager.getByTestId('memoryMarkerManagerRow')).toHaveCount(0);
        await page.mouse.move(0, 0);
        await expect(manager).toHaveScreenshot('snapshot-empty-marker-manager.png', { maxDiffPixels: 100 });
    });

    test('press K on hovered block to create a linked marker', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.hoverMemoryBlock();
        await page.keyboard.press('k');
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(1);
        await expect(frame.getByRole('button', { name: /Flag 1, Linked block, Baseline:/ })).toBeVisible();
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(frame.getByTestId('blockDiagramSection')).toHaveScreenshot('snapshot-keyboard-block-marker.png', { maxDiffPixels: 100 });
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        const manager = frame.getByTestId('memoryMarkerManagerFloatingPanel');
        await expect(manager.getByTestId('memoryMarkerManagerRow')).toHaveCount(1);
        await expect(manager.getByText(/^Block -?\d+$/)).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(manager).toHaveScreenshot('snapshot-linked-block-marker-manager.png', { maxDiffPixels: 100 });
    });

    test('press K again on the same block to remove its marker', async ({ leaksPage, page }) => {
        const frame = leaksPage.leaksFrame;
        await leaksPage.hoverMemoryBlock();
        await page.keyboard.press('k');
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(1);
        await page.keyboard.press('k');
        await expect(frame.getByTestId('memoryMarkerBaselineGuide')).toHaveCount(0);
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(frame.getByTestId('blockDiagramSection')).toHaveScreenshot('snapshot-keyboard-marker-removed.png', { maxDiffPixels: 100 });
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await expect(frame.getByTestId('memoryMarkerManagerFloatingPanel')).toContainText('No flags');
    });
});
