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
        const processLane = timelinePage.unitWrapperScroller.locator('.unit').filter({
            has: timelineFrame.getByText('core0.cubecore0', { exact: true }),
        });
        await timelinePage.expandUnit(processLane);
        const scalarLane = timelinePage.unitWrapperScroller.locator('.unit').filter({
            has: timelineFrame.locator('[class~="core0.cubecore0_SCALAR"]'),
        });
        await expect(scalarLane.locator('.insight-lane-info-name')).toHaveText('SCALAR');
        await timelinePage.expandUnit(scalarLane);
        const chart = scalarLane.locator('.ant-spin-container > canvas.drawCanvas');
        await expect(chart).toBeVisible();
        await expect(scalarLane.locator('.ant-spin-spinning')).toBeHidden();

        const interactor = timelineFrame.locator('#chartInteractor');
        // timeScale 使用两个 overlay canvas 的 contentRect.width，已扣除滚动条和边框。
        // 此 fixture 的全览范围是 125818 ns；1305 px 下拖拽 50→200 px 查询 [4820, 19283] ns。
        // 布局改变时先核对范围和独立统计期望，不能只更新统计值使测试通过。
        await expect.poll(async () => interactor.locator(':scope > canvas').evaluateAll(canvases => canvases.map(canvas => {
            const style = getComputedStyle(canvas);
            return canvas.getBoundingClientRect().width - parseFloat(style.borderLeftWidth) - parseFloat(style.borderRightWidth) -
                parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
        })), { message: 'Recheck the selection range and fixture totals if the timeline drawing width changes' }).toEqual([1305, 1305]);
        const chartBox = await chart.boundingBox();
        const interactorBox = await interactor.boundingBox();
        if (!chartBox || !interactorBox) {
            throw new Error('The core0.cubecore0 SCALAR chart and timeline interactor must be visible');
        }
        // 鼠标原点是 interactor 的左边界，不额外加 canvas 的 1 px 左边框。
        const y = chartBox.y + Math.min(50, chartBox.height / 2);
        await page.mouse.move(interactorBox.x + 50, y);
        await page.mouse.down();
        await page.mouse.move(interactorBox.x + 200, y);
        await page.mouse.up();
        const tfoot = timelinePage.bottomPanel.locator('tfoot');
        // 独立查询 OP_SIMULATOR_BIN 的 SCALAR fixture 得到以下合计，不从页面/接口回填期望。
        await expect(tfoot.locator('tr > td').nth(5)).toHaveText('7457');
        await expect(tfoot.locator('tr > td').nth(1)).toHaveText('0.023115');
        await expect(tfoot.locator('tr > td').nth(2)).toHaveText('0.000003');
        await expect(tfoot.locator('tr > td').nth(3)).toHaveText('0.000022');
        await expect(tfoot.locator('tr > td').nth(4)).toHaveText('0.000001');
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
        await expect(openInWindows).toBeVisible();
        await expect(timelineFrame.locator('.searchResult').getByText(/^\/\s*4396$/)).toBeVisible();
        // 分页输入框共用占位符，按搜索词限定等待恢复启用的输入框。
        await expect(timelineFrame.locator('.insight-category-search-overlay input[value="ST_XD_XN"]')).toBeEnabled();
        const selectedLane = timelinePage.unitWrapperScroller.locator('.unit').filter({
            has: timelineFrame.locator('.chart-selected'),
        });
        await expect(selectedLane.locator('.insight-lane-info-name')).toHaveText('SCALAR');
        await expect(selectedLane.getByTestId('expand-btn')).toHaveClass(/insight-unit-expanded/);
        await expect(selectedLane.locator('canvas.drawCanvas').first()).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-instr.png', { maxDiffPixels: 100 });
    });
});
