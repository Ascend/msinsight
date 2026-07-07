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
import { TimelinePage } from '@/page-object';
import { clearAllData, importData, setupWebSocketListener } from '@/utils';
import { FilePath } from '@/utils/constants';
import { InputHelpers } from '@/components';

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

test.describe('Timeline(Operator)', () => {
    test.beforeEach(async ({ page, timelinePage, ws }) => {
        const { timelineFrame } = timelinePage;
        await timelinePage.goto();
        await clearAllData(page);
        await importData(page, FilePath.OP_SIMULATOR_BIN);
        await timelineFrame.locator('.ant-progress-outer').waitFor({ state: 'hidden' });
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // 算子调优-图形化窗格-框选
    test('test_compute_timeline_selectUnitsRange', async ({ timelinePage, page }) => {
        const { timelineFrame } = timelinePage;
        const secondUnitInfo = timelineFrame.locator('.unit-info').nth(1);
        await secondUnitInfo.click();
        const chart = timelineFrame.locator('.ant-spin-container > .drawCanvas').first();
        const boundingBox = await chart.boundingBox();
        if (!boundingBox) {
            return;
        }
        await page.waitForTimeout(1000);
        const { x: startX, y: startY } = boundingBox;
        await page.mouse.move(startX + 50, startY + 50);
        await page.mouse.down();
        await page.mouse.move(startX + 200, startX - 200);
        await page.mouse.up();
        await expect(timelineFrame.getByText('Wall Duration(ms)', { exact: true })).toBeVisible();
        await expect(timelineFrame.getByText('Self Time(ms)')).not.toBeVisible();
        await expect(timelineFrame.getByText('Average Wall Duration(ms)')).toBeVisible();
        await expect(timelineFrame.getByText('Max Wall Duration')).toBeVisible();
        await expect(timelineFrame.getByText('Min Wall Duration')).toBeVisible();
        const rows = await timelineFrame.locator('.ant-table-row').count();
        expect(rows).toBeGreaterThan(0);
    });

    // 算子调优-Slice List-框选-算子统计项
    test('test_compute_timeline_selectUnitsRange_slice_list_total', async ({ timelinePage, page }) => {
        const { timelineFrame } = timelinePage;
        const secondUnitInfo = timelineFrame.locator('.unit-info').nth(1);
        await secondUnitInfo.click();
        const chart = timelineFrame.locator('.ant-spin-container > .drawCanvas').first();
        const boundingBox = await chart.boundingBox();
        if (!boundingBox) {
            return;
        }
        await page.waitForTimeout(1000);
        const { x: startX, y: startY } = boundingBox;
        await page.mouse.move(startX + 50, startY + 50);
        await page.mouse.down();
        await page.mouse.move(startX + 200, startY + 50);
        await page.mouse.up();
        const tfoot = await timelineFrame.locator('tfoot');
        expect(await tfoot.locator('tr > td').nth(5).innerText()).toBe('7387');
        expect(await tfoot.locator('tr > td').nth(1).innerText()).toBe('0.022960');
        expect(await tfoot.locator('tr > td').nth(2).innerText()).toBe('0.000003');
        expect(await tfoot.locator('tr > td').nth(3).innerText()).toBe('0.000022');
        expect(await tfoot.locator('tr > td').nth(4).innerText()).toBe('0.000001');
    });

    // 算子调优-图形化窗格-点击算子
    test('test_compute_timeline_operator_click', async ({ page, timelinePage }) => {
        const { timelineFrame } = timelinePage;
        const secondUnitInfo = timelineFrame.locator('.unit-info').nth(1);
        await secondUnitInfo.click();
        await page.waitForTimeout(1000);
        await timelineFrame.getByText('SCALAR').click(); // 选中收起
        await timelineFrame.getByText('SCALAR').click(); // 展开

        const chart = timelineFrame.locator('.chart-selected .ant-spin-container > .drawCanvas').first();
        await chart.click({
            position: {
                x: 22,
                y: 26,
            },
        });
        await expect(timelineFrame.getByText('Wall Duration', { exact: true })).toBeVisible();
        await expect(timelineFrame.getByText('Title')).toBeVisible();
        await expect(timelineFrame.getByText('Start', { exact:true })).toBeVisible();
    });

    // 算子调优-工具栏-搜索
    test('test_Search_when_EnterInstr', async ({ page, timelinePage }) => {
        const { searchBtn, timelineFrame, openInWindows } = timelinePage;
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('ST_XD_XN');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-instr.png', { maxDiffPixels: 100 });
    });
});
