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
import { MemSnapshotPage } from '@/page-object';
import { importData, waitForWebSocketEvent } from '@/utils';
import { FilePath } from '@/utils/constants';

interface TestFixtures {
    memSnapshotPage: MemSnapshotPage;
}

const test = baseTest.extend<TestFixtures>({
    memSnapshotPage: async ({ page }, use) => {
        await use(new MemSnapshotPage(page));
    },
});

test.describe('MemSnapshot', () => {
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, memSnapshotPage }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/snapshot/allocations');
        await page.goto('/');
        await importData(page, FilePath.SNAPSHOT_PKL);
        await memSnapshotPage.goto();
        await getAllocationsSuccess;
        await memSnapshotPage.waitForReady();
    });

    test.afterEach(async ({ memSnapshotPage }, testInfo) => {
        try {
            if (testInfo.status !== testInfo.expectedStatus) {
                await testInfo.attach('memsnapshot-ui', { body: await memSnapshotPage.memoryFrame.locator('body').ariaSnapshot(), contentType: 'text/plain' });
            }
        } finally {
            await memSnapshotPage.clearData();
        }
    });

    // 内存块图
    test('test_blockDiagram', async ({ memSnapshotPage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memSnapshotPage;
        await page.mouse.move(0, 0);
        await page.waitForTimeout(3000);
        const blockDiagram = memoryFrame.locator('#barContent');
        await page.mouse.move(0, 0);
        await expect(blockDiagram).toHaveScreenshot('snapshot-blockDiagram.png', { maxDiffPixels: 100 });
    });

    // 内存状态图
    test('test_stateDiagram', async ({ memSnapshotPage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memSnapshotPage;
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(memoryFrame.getByTestId('stateDiagram')).toHaveScreenshot('snapshot-stateDiagram.png', { maxDiffPixels: 100 });
    });

    // 内存详情表内存块视图
    test('test_blocksTable_show', async ({ memSnapshotPage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memSnapshotPage;
        await memSnapshotPage.openSystemView();
        const blocksTable = memoryFrame.getByTestId('blocksTable');
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
    test('test_eventsTable_show', async ({ memSnapshotPage, page }) => {
        await getAllocationsSuccess;
        const { memoryFrame } = memSnapshotPage;
        await memoryFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await memoryFrame.getByText('System View').click();
        const eventViewRadio = memoryFrame.getByTestId('eventViewRadio');
        await eventViewRadio.click();
        await expect(memoryFrame.getByTestId('eventsTable').locator('tbody tr.ant-table-row').first()).toContainText(/\d/);
        const eventsTable = memoryFrame.getByTestId('eventsTable');
        await page.waitForTimeout(2000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('snapshot-eventsTable.png', { maxDiffPixels: 100 });

        await eventsTable.locator('.ant-table-column-title').nth(3).click();

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(eventsTable).toHaveScreenshot('snapshot-eventsTableSorter.png', { maxDiffPixels: 100 });

        await memoryFrame.getByTestId('blockViewRadio').click();
        await expect(memoryFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
    });

    // 事件列表
    test('test_eventList', async ({ memSnapshotPage, page }) => {
        await getAllocationsSuccess;
        await page.waitForTimeout(3000);
        const { memoryFrame } = memSnapshotPage;
        const eventList = memoryFrame.locator('.table-slice-list .ant-table-body');
        await page.mouse.move(0, 0);
        await expect(eventList).toHaveScreenshot('snapshot-eventList.png', { maxDiffPixels: 100 });

        const row = eventList.locator('tr.ant-table-row').nth(1);
        await row.click();
        await expect(row).toHaveClass(/click-select/);
        await expect(memoryFrame.getByRole('tab', { name: /^#\d+$/ }).first()).toBeVisible();
        // 选择事件会自动展开详情，收起后对比整个事件列表及其选中状态。
        const detailToggle = memoryFrame.locator('div[style*="width: 90px"][style*="height: 20px"]');
        await detailToggle.click();
        await expect(detailToggle.locator('..')).toHaveCSS('height', '38px');
        await expect(row).toHaveClass(/click-select/);
        await page.mouse.move(0, 0);
        await page.waitForTimeout(500);
        await page.mouse.move(0, 0);
        await expect(eventList).toHaveScreenshot('snapshot-eventListSelect.png', { maxDiffPixels: 100 });
    });

    // 内存块详情
    test('test_memoryBlock_info', async ({ memSnapshotPage }) => {
        await memSnapshotPage.clickMemoryBlock();
        const panel = await memSnapshotPage.selectedDetailPanel();
        await expect(panel.getByTitle('Size(MBytes)', { exact: true }).locator('xpath=following-sibling::*[1]')).toHaveText(/^\d+(?:\.\d+)?$/);
    });

    // 事件详情
    test('test_event_info', async ({ memSnapshotPage, page }) => {
        const { memoryFrame } = memSnapshotPage;
        const row = memoryFrame.locator('.table-slice-list tr.ant-table-row').nth(1);
        await expect(row).toBeVisible();
        const rowKey = await row.getAttribute('data-row-key');
        if (!rowKey) throw new Error('Event row must have an ID');
        const eventId = rowKey.split('_')[0];
        await row.click();
        await expect(row).toHaveClass(/click-select/);
        await page.mouse.move(0, 0);
        await expect(memoryFrame.getByRole('tab', { name: '#' + eventId, exact: true })).toBeVisible();
        const panel = await memSnapshotPage.selectedDetailPanel();
        await expect(panel.getByTitle('Action', { exact: true }).locator('xpath=following-sibling::*[1]')).toHaveText('alloc');
    });

    test('switch zoom mode', async ({ memSnapshotPage }) => {
        const graph = memSnapshotPage.memoryFrame.locator('#barContent');
        await graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true })).toBeVisible();
        await graph.getByRole('button', { name: 'Switch to proportional zoom', exact: true }).click();
        await expect(graph.getByRole('button', { name: 'Switch to horizontal zoom', exact: true })).toBeVisible();
    });

    test('toggle reserved memory legend', async ({ memSnapshotPage, page }) => {
        await expect(memSnapshotPage.memoryFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('snapshot-reserved-line-visible.png', { maxDiffPixels: 100 });
        const legend = memSnapshotPage.memoryFrame.getByTestId('allocationLineLegend').getByRole('button', { name: 'PTA Reserved', exact: true });
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'false');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(2000);
        await expect(memSnapshotPage.memoryFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('snapshot-reserved-line-hidden.png', { maxDiffPixels: 100 });
        await legend.click();
        await expect(legend).toHaveAttribute('aria-pressed', 'true');
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(memSnapshotPage.memoryFrame.getByTestId('blockDiagramSection').locator('canvas')).toHaveScreenshot('snapshot-reserved-line-visible.png', { maxDiffPixels: 100 });
    });

    test('locate block and show matching detail', async ({ memSnapshotPage }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.openSystemView();
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

    test('open and close window overview using keyboard', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
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

    test('toggle unreleased blocks filter', async ({ memSnapshotPage }) => {
        await memSnapshotPage.openSystemView();
        const checkbox = memSnapshotPage.memoryFrame.getByRole('checkbox', { name: 'Auto Filter Unreleased in Range', exact: true });
        const initial = await checkbox.isChecked();
        await checkbox.setChecked(!initial);
        await expect(checkbox).toBeChecked({ checked: !initial });
        await expect(memSnapshotPage.memoryFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
        await checkbox.setChecked(initial);
        await expect(checkbox).toBeChecked({ checked: initial });
    });

    test('open and close marker manager', async ({ memSnapshotPage }) => {
        const frame = memSnapshotPage.memoryFrame;
        await frame.getByRole('button', { name: 'Marker management', exact: true }).click();
        await expect(frame.getByTestId('memoryMarkerManagerFloatingPanel')).toBeVisible();
        await frame.getByRole('button', { name: 'Close marker management', exact: true }).click();
        await expect(frame.getByTestId('memoryMarkerManagerFloatingPanel')).toBeHidden();
    });

    test('overview shows window boundaries and focused range', async ({ memSnapshotPage, page }) => {
        const overview = memSnapshotPage.memoryFrame.getByTestId('snapshotSliceOverview');
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
        test(`toggle ${label} layer`, async ({ memSnapshotPage, page }) => {
            const frame = memSnapshotPage.memoryFrame;
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

    test('open and close interaction guide', async ({ memSnapshotPage }) => {
        const frame = memSnapshotPage.memoryFrame;
        await frame.getByRole('button', { name: 'Lifecycle graph toolbar guide', exact: true }).click();
        await expect(frame.getByTestId('lifecycleGraphInteractionGuide')).toBeVisible();
        await frame.getByRole('button', { name: 'Close lifecycle graph toolbar guide', exact: true }).click();
        await expect(frame.getByTestId('lifecycleGraphInteractionGuide')).toBeHidden();
    });

    test('create two markers and show their memory difference', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.addMemoryMarker(120);
        await memSnapshotPage.addMemoryMarker(280);
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

    test('rename marker and retain its name after reopening manager', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.addMemoryMarker();
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

    test('hide and restore one marker without deleting it', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.addMemoryMarker();
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

    test('delete one marker and keep the other marker', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.addMemoryMarker(120);
        await memSnapshotPage.addMemoryMarker(280);
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

    test('cancel then confirm clearing all markers', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.addMemoryMarker(120);
        await memSnapshotPage.addMemoryMarker(280);
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

    test('press K on hovered block to create a linked marker', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.hoverMemoryBlock();
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

    test('press K again on the same block to remove its marker', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        await memSnapshotPage.hoverMemoryBlock();
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

test.describe('MemSnapshot slice windows', () => {
    test.describe.configure({ timeout: 180000 });
    let getAllocationsSuccess: Promise<unknown>;
    test.beforeEach(async ({ page, memSnapshotPage }) => {
        getAllocationsSuccess = waitForWebSocketEvent(page, (res) => res?.command === 'Memory/snapshot/allocationLines');
        await page.goto('/');
        await importData(page, FilePath.SNAPSHOT_PKL_SLICE_WINDOW);
        await memSnapshotPage.goto();
        await getAllocationsSuccess;
        await memSnapshotPage.waitForReady();
    });

    test.afterEach(async ({ memSnapshotPage }, testInfo) => {
        try {
            if (testInfo.status !== testInfo.expectedStatus) {
                await testInfo.attach('memsnapshot-ui', { body: await memSnapshotPage.memoryFrame.locator('body').ariaSnapshot(), contentType: 'text/plain' });
            }
        } finally {
            await memSnapshotPage.clearData();
        }
    });

    test('display snapshot window navigation', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        const windowSelect = frame.getByRole('combobox', { name: 'Window', exact: true });
        const overview = frame.getByTestId('snapshotSliceOverview');

        await expect(windowSelect).toBeVisible();
        await windowSelect.locator('xpath=ancestor::*[contains(concat(" ", normalize-space(@class), " "), " ant-select ")][1]')
            .locator('.ant-select-selector').click();
        const options = frame.getByRole('option', { name: /^Window \d+$/ });
        await expect(options).toHaveCount(2);
        await windowSelect.press('Escape');
        await expect(windowSelect).toHaveAttribute('aria-expanded', 'false');
        // 当前窗就绪不代表其它窗的缩略数据已经返回，截图需等待全局趋势完整。
        await expect(overview.getByRole('button', { name: /^Window \d+ · Peak:/ }))
            .toHaveCount(2, { timeout: 90000 });
        await expect(overview.getByRole('button', { name: /^Window 2 · Peak:/ })).toHaveAttribute('aria-pressed', 'true');
        await expect(overview.getByTestId('sliceAxisCoordinate')).toHaveCount(3);
        await page.mouse.move(0, 0);
        await expect(overview).toHaveScreenshot('snapshot-slice-window-overview.png', { maxDiffPixels: 100 });
    });

    test('switch snapshot window from selector', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        const windowSelect = frame.getByRole('combobox', { name: 'Window', exact: true });
        const overview = frame.getByTestId('snapshotSliceOverview');

        await windowSelect.locator('xpath=ancestor::*[contains(concat(" ", normalize-space(@class), " "), " ant-select ")][1]')
            .locator('.ant-select-selector').click();
        await windowSelect.press('ArrowUp');
        await windowSelect.press('Enter');
        await expect(overview.getByRole('button', { name: /^Window 1 · Peak:/ })).toHaveAttribute('aria-pressed', 'true');
        await expect(windowSelect.locator('xpath=ancestor::*[contains(concat(" ", normalize-space(@class), " "), " ant-select ")][1]')
            .locator('.ant-select-selection-item')).toHaveText('Window 1');
        const diagram = frame.getByTestId('blockDiagramSection');
        await expect(diagram).toHaveAttribute('data-progressive-render-percent', '100', { timeout: 90000 });
        await expect(diagram).toHaveAttribute('data-loading-blocks', 'false', { timeout: 30000 });
        await expect(diagram).toHaveAttribute('data-progressive-loading-visible', 'false', { timeout: 30000 });
        await page.mouse.move(0, 0);
        await expect(diagram).toHaveScreenshot('snapshot-slice-window-1.png', {
            maxDiffPixels: 100,
            timeout: 30000,
        });
    });

    test('switch snapshot window from overview', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        const overview = frame.getByTestId('snapshotSliceOverview');
        const windowSelect = frame.getByRole('combobox', { name: 'Window', exact: true });
        const firstWindow = overview.getByRole('button', { name: /^Window 1 · Peak:/ });

        await firstWindow.click();
        await expect(firstWindow).toHaveAttribute('aria-pressed', 'true');
        await expect(windowSelect.locator('xpath=ancestor::*[contains(concat(" ", normalize-space(@class), " "), " ant-select ")][1]')
            .locator('.ant-select-selection-item')).toHaveText('Window 1');
        const diagram = frame.getByTestId('blockDiagramSection');
        await expect(diagram).toHaveAttribute('data-progressive-render-percent', '100', { timeout: 90000 });
        await expect(diagram).toHaveAttribute('data-loading-blocks', 'false', { timeout: 30000 });
        await expect(diagram).toHaveAttribute('data-progressive-loading-visible', 'false', { timeout: 30000 });
        await page.mouse.move(0, 0);
        await expect(diagram).toHaveScreenshot('snapshot-overview-switch-window-1.png', {
            maxDiffPixels: 100,
            timeout: 30000,
        });
        await expect(frame.getByTestId('stateDiagram')).toHaveScreenshot('snapshot-overview-switch-state-window-1.png', {
            maxDiffPixels: 100,
            timeout: 30000,
        });
    });

    test('switch snapshot window from expanded overview', async ({ memSnapshotPage, page }) => {
        const frame = memSnapshotPage.memoryFrame;
        const overview = frame.getByTestId('snapshotSliceOverview');
        await expect(overview.getByRole('button', { name: /^Window \d+ · Peak:/ }))
            .toHaveCount(2, { timeout: 90000 });
        const expandButton = frame.getByRole('button', { name: 'Enlarge', exact: true });
        await expandButton.focus();
        await expandButton.press('Enter');
        const dialog = frame.getByRole('dialog', { name: 'Memory window overview', exact: true });
        await expect(dialog).toBeVisible();
        // 可见不代表展开转场结束，等待布局和控件稳定后再切窗。
        await expect(dialog).not.toHaveAttribute('data-opening', 'true', { timeout: 30000 });

        const switchWindowButton = dialog.getByRole('button', { name: 'Switch loaded window · 1', exact: true });
        await switchWindowButton.focus();
        await switchWindowButton.press('Enter');
        await expect(dialog.getByRole('img', { name: 'Currently loaded window', exact: true })).toBeVisible();
        const windowSelect = frame.getByRole('combobox', { name: 'Window', exact: true });
        await expect(windowSelect.locator('xpath=ancestor::*[contains(concat(" ", normalize-space(@class), " "), " ant-select ")][1]')
            .locator('.ant-select-selection-item')).toHaveText('Window 1');

        const diagram = frame.getByTestId('blockDiagramSection');
        await expect(diagram).toHaveAttribute('data-progressive-render-percent', '100', { timeout: 90000 });
        await expect(diagram).toHaveAttribute('data-loading-blocks', 'false', { timeout: 30000 });
        await expect(diagram).toHaveAttribute('data-progressive-loading-visible', 'false', { timeout: 30000 });
        await page.mouse.move(0, 0);
        await expect(dialog).toHaveScreenshot('snapshot-expanded-overview-window-1.png', {
            maxDiffPixels: 100,
            timeout: 30000,
            // 保留 Web Animations 的正常完成流程，不让截图强制结束转场动画。
            animations: 'allow',
        });
        await dialog.getByRole('button', { name: 'Close', exact: true }).click();
        await expect(dialog).toBeHidden({ timeout: 30000 });
        await page.mouse.move(0, 0);
        await expect(diagram).toHaveScreenshot('snapshot-expanded-overview-switch-window-1.png', {
            maxDiffPixels: 100,
            timeout: 30000,
        });
    });

    test('keep cross-window block details complete', async ({ memSnapshotPage }) => {
        const frame = memSnapshotPage.memoryFrame;
        const blockId = 485072;
        const queryBlockDetail = async (sliceIndex: number): Promise<Record<string, unknown>> => {
            return frame.locator('body').evaluate(async (_, params) => {
                const browserWindow = window as unknown as Window & {
                    request: (request: { command: string; params: Record<string, unknown> }) => Promise<Record<string, unknown>>;
                };
                return browserWindow.request({ command: 'Memory/snapshot/detail', params });
            }, { type: 'block', id: blockId, deviceId: '0', sliceIndex });
        };
        const expectCompleteDetail = (detail: Record<string, unknown>): void => {
            expect(detail.ID).toBe(blockId);
            expect(detail.Address).toBe('21058799960064');
            expect(detail['Size(MBytes)']).toBeTruthy();
            expect(detail['Alloc Event']).toEqual(expect.objectContaining({ ID: blockId }));
            expect(detail['Free Requested Event']).toEqual(expect.objectContaining({ ID: 563904 }));
            expect(detail['Free Completed Event']).toEqual(expect.objectContaining({ ID: 582452 }));
        };

        const secondWindowDetail = await queryBlockDetail(1);
        expectCompleteDetail(secondWindowDetail);
        const firstWindow = frame.getByTestId('snapshotSliceOverview')
            .getByRole('button', { name: /^Window 1 · Peak:/ });
        await firstWindow.click();
        await expect(firstWindow).toHaveAttribute('aria-pressed', 'true');
        const diagram = frame.getByTestId('blockDiagramSection');
        await expect(diagram).toHaveAttribute('data-progressive-render-percent', '100', { timeout: 90000 });
        await expect(diagram).toHaveAttribute('data-loading-blocks', 'false', { timeout: 30000 });

        const firstWindowDetail = await queryBlockDetail(0);
        expectCompleteDetail(firstWindowDetail);
        expect(firstWindowDetail).toEqual(secondWindowDetail);
    });
});
