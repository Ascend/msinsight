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

test.describe('Timeline(Trace_Json)', () => {
    test.beforeEach(async ({ page, timelinePage, ws }) => {
        const { timelineFrame } = timelinePage;
        await timelinePage.goto();
        await clearAllData(page);
        await importData(page, FilePath.OP_SIMULATOR_JSON);
        await timelineFrame.locator('.ant-progress-outer').waitFor({ state: 'hidden' });
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // 算子调优-图形化窗格-框选
    test('test_compute_timeline_selectUnitsRange', async ({ timelinePage, page }) => {
        //timeline功能框选有bug，待修复
        const { timelineFrame, fullPage } = timelinePage;
        const secondUnitInfo = timelineFrame.locator('.unit-info').nth(2);
        await secondUnitInfo.click();
        await page.waitForTimeout(1000);
        const chart = timelineFrame.locator('.ant-spin-container > .drawCanvas').nth(1);
        const boundingBox = await chart.boundingBox();
        if (!boundingBox) {
            return;
        }
        const { x: startX, y: startY } = boundingBox;
        await page.mouse.move(startX + 50, startY + 50);
        await page.mouse.down();
        await page.waitForTimeout(100); // 模拟人类操作停顿
        await page.mouse.move(startX + 200, startY + 100,{ steps: 20 });
        await page.mouse.up();
        await expect(fullPage).toHaveScreenshot('select-units-range.png', { maxDiffPixels: 100 });
    });

    // 算子调优-工具栏-搜索
    test('test_Search_when_EnterInstr', async ({ page, timelinePage }) => {
        const { searchBtn, timelineFrame, openInWindows } = timelinePage;
        const secondUnitInfo = timelineFrame.locator('.unit-info').nth(2);
        await secondUnitInfo.click();
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('LD');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-instr.png', { maxDiffPixels: 100 });
    });
});
