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
import { FrameworkPage, MemoryPage, TimelinePage } from '@/page-object';
import { clearAllData, importData, setCompare, setupWebSocketListener, waitForResponse, waitForWebSocketEvent } from '@/utils';
import { CheckboxHelpers, InputHelpers, SelectHelpers } from '@/components';
import { FilePath } from '@/utils/constants';

interface TestFixtures {
    memoryPage: MemoryPage;
    ws: Promise<WebSocket>;
}
const test = baseTest.extend<TestFixtures>({
    memoryPage: async ({ page }, use) => {
        const memoryPage = new MemoryPage(page);
        await use(memoryPage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});

test.describe('Memory(Pytorch_SingleMachineMultiRankData)', () => {
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.DB);
        await allCardParsedPromise;
    });

    // 【case】text非多机非对比memory界面加载
    test('loadMemoryPageSuccess_when_dataParseSuccess', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-single-rank-0.png', {
            maxDiffPixels: 500,
        });
    });

    // 【case】memory顶部过滤条件改变界面加载
    test('change_filterCondition', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector, groupIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        const groupIdSelect = new SelectHelpers(page, groupIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('2');
        await groupIdSelect.open();
        await groupIdSelect.selectOption('Stream');
        await memoryFrame.getByText('loading').first().waitFor({ state: 'hidden' });
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-single-rank-2.png', {
            maxDiffPixels: 500,
        });
    });

    // 【case】memory底部表格条件查询后结果加载
    test('query_memoryDetailTable_by_tableFilterCondition', async ({ page, memoryPage, ws }) => {
        const { memoryFrame } = memoryPage;
        const websocket = await ws;

        const nameSearchPromise = waitForResponse(websocket, (res) => res?.command === 'Memory/view/operator');
        await memoryFrame.getByLabel('Name').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Search Name').fill('aten::empty_strided');
        await memoryFrame.getByRole('button', { name: 'search Search' }).click();
        await nameSearchPromise;

        await memoryFrame.getByLabel('Size(KB)').getByRole('button').click();
        const filterDropdown = memoryFrame.locator('.ant-table-filter-dropdown').last();
        await filterDropdown.getByPlaceholder('Min').fill('0');
        await filterDropdown.getByPlaceholder('Max').fill('30');
        const sizeSearchPromise = waitForResponse(websocket, (res) => res?.command === 'Memory/view/operator');
        await filterDropdown.getByRole('button', { name: 'Search', exact: true }).click();
        await sizeSearchPromise;
        await page.waitForTimeout(1000);

        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page').locator('.mi-collapsible-panel').nth(1)).toHaveScreenshot('pytorch-single-table-data.png', {
            maxDiffPixels: 500,
        });
    });

    // 【case】memory底部表格条件重置后结果加载
    test('reset_memoryDetailTable_by_tableFilterCondition', async ({ page, memoryPage, ws }) => {
        const { memoryFrame, resetBtn } = memoryPage;

        const tableWrapper = memoryFrame.locator('.mi-page').locator('.panel-content').nth(1);

        await page.mouse.move(0, 0);
        await expect(tableWrapper).toHaveScreenshot('pytorch-single-reset.png', {
            maxDiffPixels: 500,
        });

        await memoryFrame.getByLabel('Size(KB)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Max').fill('30');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await page.mouse.move(0, 0);
        await expect(tableWrapper).toHaveScreenshot('pytorch-single-reset-change.png', {
            maxDiffPixels: 500,
        });

        await resetBtn.click();
        await page.mouse.move(0, 0);
        await expect(tableWrapper).toHaveScreenshot('pytorch-single-reset.png', {
            maxDiffPixels: 500,
        });
    });

    // 表格跳转至Timeline
    test('test_redirectToTimeline', async ({ page, memoryPage }) => {
        const { memoryFrame } = memoryPage;
        const { fullPage } = new TimelinePage(page);
        await page.waitForTimeout(2000);
        await memoryFrame.locator('tr:nth-child(3) > td:nth-child(15)').getByRole('button').click();

        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        try {
            await expect(fullPage).toHaveScreenshot('redirect-to-timeline.png', {
                maxDiffPixels: 500,
            });
        }catch (e) {
            await expect(fullPage).toHaveScreenshot('redirect-to-timeline-1.png', {
                maxDiffPixels: 500,
            });
        }
    });

    // 对比数据
    test('memory_compare_rank_text', async ({ page, memoryPage }) => {
        const { memoryFrame } = memoryPage;
        await setCompare(page, memoryFrame, { baseline: FilePath.DB_RANK_0, comparison: FilePath.DB_RANK_1 });
        await memoryFrame.getByText('Difference').first().waitFor({ state: 'visible' });
        await memoryFrame.locator('.ant-spin-spinning').first().waitFor({ state: 'hidden' });
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('compare-rank.png', {
            maxDiffPixels: 4000,
        });
        await page.waitForTimeout(2000); // 对比场景需要加延时，确保稳定
    });

    // 【case】memory中间调整区间，底部表格仅查看在选中时间区间分配或释放内存的数据时的结果展示
    test( 'query_memoryDetailTable_by_tableFilterConditionOnlyShowAllocatedOrReleasedWithinInterval', async ({ page, memoryPage, ws }) => {
        const { memoryFrame, isOnlyShowAllocatedOrReleasedWithinIntervalChecker } = memoryPage;
        const isOnlyShowAllocatedOrReleasedWithinIntervalCheckbox = new CheckboxHelpers(page, isOnlyShowAllocatedOrReleasedWithinIntervalChecker, memoryFrame);

        const chart = memoryFrame.locator('.ant-spin-container > div > div:nth-child(2) > div:nth-child(1) > canvas');
        await page.waitForTimeout(1000);
        const chartInfo = await chart.boundingBox();

        console.log(chart, chartInfo);
        // 等待echarts加载完成才能框选
        await page.waitForTimeout(1000);
        const { x: startX, y: startY } = chartInfo;
        await page.mouse.move(startX + 300, startY + 200);
        await page.mouse.down();
        await page.mouse.move(startX + 400, startY + 200);
        await page.mouse.up();

        await isOnlyShowAllocatedOrReleasedWithinIntervalCheckbox.click();
        expect(await isOnlyShowAllocatedOrReleasedWithinIntervalCheckbox.isChecked()).toBe(true);

        const spin = memoryFrame.locator('.panel-content .ant-spin-dot-spin');
        // 等待 loading 结束后端返回更新 totalNum
        await spin.waitFor({ state: 'detached' });
        await page.mouse.move(0, 0);

        const totalNumListItem = memoryFrame.locator('.ant-spin-container > ul > li.ant-pagination-total-text');
        expect(await totalNumListItem.innerText()).toBe('Total 315 items');

        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-interval-only-show.png', {
            maxDiffPixels: 500,
        });
    });

    // 【case】memory底部表格联合筛选后结果加载
    test('query_memoryDetailTable_by_jointFiltering', async ({ page, memoryPage, ws }) => {
        const { memoryFrame } = memoryPage;

        const searchPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
        await page.waitForTimeout(1000);
        await memoryFrame.getByLabel('Name').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Search Name').fill('aten::empty_strided');
        await memoryFrame.getByRole('button', { name: 'search Search' }).click();
        await searchPromise;

        await page.waitForTimeout(1000);
        await memoryFrame.getByLabel('Size(KB)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Min').fill('0');
        await memoryFrame.getByPlaceholder('Max').fill('30');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await searchPromise;
        await page.waitForTimeout(1000);

        await memoryFrame.getByLabel('Allocation Time(ms)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Min').nth(1).fill('0');
        await memoryFrame.getByPlaceholder('Max').nth(1).fill('200');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await searchPromise;
        await page.waitForTimeout(1000);

        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page').locator('.mi-collapsible-panel').nth(1)).toHaveScreenshot('pytorch-single-table-joint-filtering.png', {
            maxDiffPixels: 500,
        });
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});

test.describe('Memory(MindSpore)', () => {
    test.skip();
    test.describe.configure({ timeout: 240_000 });
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.MIND_SPORE);
        await allCardParsedPromise;
    });

    // 测试图表加载完成
    test('test_staticChartDisplay_given_rank1', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('1');

        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);

        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('mindspore-loaded.png', {
            maxDiffPixels: 500,
        });
    });

    // 测试鼠标滚动、框选、查询
    test('test_staticChartDisplay_with_conditions', async ({ page, memoryPage }) => {
        const { memoryFrame, nameInputor, minSizeInputor, rankIdSelector, queryBtn } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        // 等待echarts加载完成再操作
        await page.waitForTimeout(1000);
        // 滚动到最下方，让graph折线图和表格显示出来
        await page.mouse.wheel(0, 800);
        await page.waitForTimeout(1000);
        // 框选
        const graphChart = memoryFrame.locator('canvas').nth(1);
        expect(graphChart).not.toBeNull();
        const chartInfo = await graphChart.boundingBox();
        const { x: startX, y: startY } = chartInfo;
        await page.mouse.move(startX + 300, startY + 200);
        await page.mouse.down();
        await page.mouse.move(startX + 400, startY + 200);
        await page.mouse.up();
        const nameInput = new InputHelpers(page, nameInputor, memoryFrame);
        const minSizeInput = new InputHelpers(page, minSizeInputor, memoryFrame);
        expect(await nameInput.expectValueToBe(''));
        expect(await minSizeInput.expectValueToBe('0'));
        await nameInput.setValue('attention');
        await minSizeInput.setValue('10000');
        await queryBtn.click();
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('mindspore-filter.png', {
            maxDiffPixels: 500,
        });
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});

test.describe('Memory(Pytorch_MultiMachinesMultiRanksData)', () => {
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.MULTI_NODES);
        await allCardParsedPromise;
    });

    // 多机多卡数据界面正常加载，切换卡序号正常显示
    test('test_pageDisplay_when_change_rank', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('1');

        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);

        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('memory-pytorch-multi-loaded.png', {
            maxDiffPixels: 500,
        });
    });

    // 多机多卡数据点击目录时会自动切换到点击的卡
    test('test_pageDisplay_when_click_rank', async ({ page, memoryPage }) => {
        const frameworkPage = new FrameworkPage(page);
        const { memoryFrame, hostSelector, rankIdSelector } = memoryPage;
        const hostSelect = new SelectHelpers(page, hostSelector, memoryFrame);

        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        const dbHost0Rank1 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_1);
        await dbHost0Rank1.click();
        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);
        expect(await hostSelect.getValue()).toBe('node18899436934890168541_0');
        expect(await rankIdSelect.getValue()).toBe('1');

        const dbHost1Rank1 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_1_RANK_1);
        await dbHost1Rank1.click();
        await page.waitForTimeout(1000);
        expect(await hostSelect.getValue()).toBe('ubuntu22044973785946912235777_0');
        expect(await rankIdSelect.getValue()).toBe('9');
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-multi-click-rank.png', {
            maxDiffPixels: 500,
        });
    });

    // 多机多卡数据切换机器时自动选中首张卡
    test('test_pageDisplay_when_change_host', async ({ page, memoryPage }) => {
        const { memoryFrame, hostSelector, rankIdSelector } = memoryPage;
        const hostSelect = new SelectHelpers(page, hostSelector, memoryFrame);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await hostSelect.open();
        await hostSelect.selectOption('ubuntu22044973785946912235777_0');
        expect(await rankIdSelect.getValue()).toBe('8');
        await hostSelect.open();
        await hostSelect.selectOption('node18899436934890168541_0');
        expect(await rankIdSelect.getValue()).toBe('0');
    });

    // 多机多卡db类型数据比对场景
    test('test_pageDisplay_compare_rank_db', async ({ page, memoryPage }) => {
        test.setTimeout(120000);
        const { minSizeInputor, maxSizeInputor, memoryFrame, queryBtn, resetBtn } = memoryPage;
        await setCompare(page, memoryFrame, { baseline: FilePath.MULTI_NODES_NODE_0_RANK_0, comparison: FilePath.MULTI_NODES_NODE_1_RANK_0 });
        await memoryFrame.getByText('Difference').first().waitFor({ state: 'visible' });
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('memory-compare-rank-db.png', {
            maxDiffPixels: 500,
        });
        const minSizeInput = new InputHelpers(page, minSizeInputor, memoryFrame);
        const maxSizeInput = new InputHelpers(page, maxSizeInputor, memoryFrame);
        await minSizeInput.expectValueToBe('-243840');
        await maxSizeInput.expectValueToBe('243840');
        await minSizeInput.setValue('0');
        await queryBtn.click();
        await memoryFrame.locator('.ant-spin-spinning').first().waitFor({ state: 'hidden' });
        await memoryFrame.getByText('Difference').first().waitFor({ state: 'visible' });
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('memory-compare-rank-db-with-condition.png', {
            maxDiffPixels: 500,
        });
        await resetBtn.click();
        await minSizeInput.expectValueToBe('-243840');
        await page.waitForTimeout(2000); // 对比场景需要加延时，确保稳定
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});

test.describe('Memory(Pytorch_SwitchProject)', () => {
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.TEXT);
        await allCardParsedPromise;
        await importData(page, FilePath.MULTI_NODES);
        await allCardParsedPromise;
    });

    // 测试从多机多卡db类型数据切换到单机多卡text类型数据
    test('test_pageDisplay_when_db_to_text', async ({ page, memoryPage }) => {
        const frameworkPage = new FrameworkPage(page);
        const dbRank0 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_0);
        await dbRank0.click();
        const textRank2 = frameworkPage.getRankLocator(FilePath.TEXT_RANK_2);
        await textRank2.click();
        const { memoryFrame, hostSelector, rankIdSelector } = memoryPage;
        await hostSelector.waitFor({ state: 'detached' });
        await page.waitForTimeout(1000);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        const selectedText = await rankIdSelect.getValue();
        expect(selectedText).toBe('2');
        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);
        await page.mouse.move(0,0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-db-to-text.png', {
            maxDiffPixels: 800,
        });
    });

    // 测试从单机多卡text类型数据切换到多机多卡db类型数据
    test('test_pageDisplay_when_text_to_db', async ({ page, memoryPage, ws }) => {
        const frameworkPage = new FrameworkPage(page);
        const textRank0 = frameworkPage.getRankLocator(FilePath.TEXT_RANK_0);
        await textRank0.click();

        const dbRank1 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_1);
        const allCardParsedPromise = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await dbRank1.click();
        await allCardParsedPromise;

        await page.mouse.move(0,0);
        const { memoryFrame, hostSelector, rankIdSelector } = memoryPage;
        await hostSelector.waitFor({ state: 'attached' });
        const hostSelect = new SelectHelpers(page, hostSelector, memoryFrame);
        const hostText = await hostSelect.getValue();
        expect(hostText).toBe('node18899436934890168541_0');
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        const selectedText = await rankIdSelect.getValue();
        expect(selectedText).toBe('1');
        const searchPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
        await searchPromise;
        // 等待 echarts 动画结束
        await page.waitForTimeout(2000);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-text-to-db.png', {
            maxDiffPixels: 500,
        });
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});

test.describe('Memory(Switch_Dynamic_and_Static)', () => {
    test.skip();
    test.describe.configure({ timeout: 240_000 });
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.MIND_SPORE);
        await allCardParsedPromise;
        await importData(page, FilePath.TEXT_330);
        await allCardParsedPromise;
    });

    // 测试在表头排序时从动态图和静态图数据切换时的界面展示
    test('test_pageDisplay_when_dynamic_to_static', async ({ page, memoryPage }) => {
        const frameworkPage = new FrameworkPage(page);
        const { memoryFrame, graphIdSelector, queryBtn } = memoryPage;
        const textRank0 = frameworkPage.getRankLocator(FilePath.TEXT_330_RANK_0);
        // 点击Name列排序
        await memoryFrame.getByRole('table').getByText('Name').click();
        const msRank0 = frameworkPage.getRankLocator(FilePath.MS_RANK_0);
        await msRank0.click();
        await graphIdSelector.waitFor({ state: 'attached' });
        const graphIdSelect = new SelectHelpers(page, graphIdSelector, memoryFrame);
        const graphIdText = await graphIdSelect.getValue();
        expect(graphIdText).toBe('1');
        // 等待表格加载完成
        await expect(queryBtn).toBeEnabled({ timeout: 20_000 });
        // 等待 echarts 动画结束
        await page.waitForTimeout(2000);
        // 点击Device ID表头排序，再切换
        await memoryFrame.getByRole('table').getByText('Device ID').click();
        await expect(queryBtn).toBeEnabled({ timeout: 20_000 });
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('mindspore_rank0_loaded.png', {
            maxDiffPixels: 500,
        });
        await textRank0.click();
        await graphIdSelector.waitFor({ state: 'detached' });
        // 等待 echarts 动画结束
        await page.waitForTimeout(1000);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-single-rank-0.png', {
            maxDiffPixels: 500,
        });
    });
});

test.describe('Memory(Pytorch_Group_By_Component)', () => {
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.TEXT);
        await allCardParsedPromise;
        const { memoryFrame, groupIdSelector } = memoryPage;
        const groupIdSelect = new SelectHelpers(page, groupIdSelector, memoryFrame);
        await groupIdSelect.open();
        await groupIdSelect.selectOption('Component');
        await page.waitForTimeout(1000);
    });

    // 组件级内存展示测试
    test('test_pageDisplay_group_by_Component', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('2');
        await page.waitForTimeout(1000);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-group-by-component.png', {
            maxDiffPixels: 800,
        });
    });

    // 组件级内存排序测试
    test('test_sort_group_by_Component', async ({ memoryPage, page }) => {
        const { memoryFrame } = memoryPage;
        await memoryFrame.getByRole('table').getByText('Peak Memory Reserved(MB)').click();
        await page.mouse.move(0,0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-sort-group-by-component.png', {
            maxDiffPixels: 500,
        });
    });

    // 组件级内存比对场景测试
    test('test_compare_group_by_Component', async ({ page, memoryPage }) => {
        const { memoryFrame } = memoryPage;
        await setCompare(page, memoryFrame, { baseline: FilePath.TEXT_RANK_0, comparison: FilePath.TEXT_RANK_2 });
        await memoryFrame.getByText('Difference').first().waitFor({ state: 'visible' });
        await memoryFrame.locator('.ant-spin-spinning').waitFor({ state: 'hidden' });
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-compare-group-by-component.png', {
            maxDiffPixels: 6000,
        });

        const table = memoryFrame.locator('.mi-page').locator('.mi-collapsible-panel').nth(1);
        await table.getByRole('button', { name: 'See more' }).nth(0).click();
        await table.getByRole('button', { name: 'See more' }).nth(1).click();
        await table.getByRole('button', { name: 'See more' }).nth(2).click();
        await expect(table).toHaveScreenshot('pytorch-compare-group-by-component-table.png');
        await page.waitForTimeout(2000); // 对比场景需要加延时，确保稳定
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});

test.describe('Memory(Text)', () => {
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.TEXT);
        await allCardParsedPromise;
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // memory界面加载
    test('loadMemoryPageSuccess_when_dataParseSuccess', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('text_memory_show.png', {
            maxDiffPixels: 500,
        });
    });

    // 顶部过滤条件改变界面加载
    test('change_filterCondition', async ({ page, memoryPage }) => {
        const { memoryFrame, rankIdSelector, groupIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        const groupIdSelect = new SelectHelpers(page, groupIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('2');
        await groupIdSelect.open();
        await groupIdSelect.selectOption('Stream');
        await memoryFrame.getByText('loading').first().waitFor({ state: 'hidden' });
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('text_memory_changeFilters.png', {
            maxDiffPixels: 500,
        });
    });

    // 底部表格条件查询后结果加载
    test('query_memoryDetailTable_by_tableFilterCondition', async ({ page, memoryPage, ws }) => {
        const { memoryFrame } = memoryPage;
        const searchPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
        await page.waitForTimeout(1000);
        await memoryFrame.getByLabel('Name').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Search Name').fill('aten::empty_strided');
        await memoryFrame.getByRole('button', { name: 'search Search' }).click();
        await searchPromise;

        await page.waitForTimeout(1000);
        await memoryFrame.getByLabel('Size(KB)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Min').fill('0');
        await memoryFrame.getByPlaceholder('Max').fill('30');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await searchPromise;
        await page.waitForTimeout(1000);

        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page').locator('.mi-collapsible-panel').nth(1)).toHaveScreenshot('text_memory_tableData.png', {
            maxDiffPixels: 500,
        });
    });

    // 底部表格条件重置后结果加载
    test('reset_memoryDetailTable_by_tableFilterCondition', async ({ page, memoryPage, ws }) => {
        const { memoryFrame, resetBtn } = memoryPage;
        const tableWrapper = memoryFrame.locator('.mi-page').locator('.panel-content').nth(1);
        await page.mouse.move(0, 0);
        await expect(tableWrapper).toHaveScreenshot('text_memory_table_reset.png', {
            maxDiffPixels: 500,
        });

        const searchPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
        await memoryFrame.getByLabel('Size(KB)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Max').fill('30');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await searchPromise;
        await page.mouse.move(0, 0);
        await expect(tableWrapper).toHaveScreenshot('text_memory_table_reset_change.png', {
            maxDiffPixels: 500,
        });

        await resetBtn.click();
        await searchPromise;
        await page.mouse.move(0, 0);
        await expect(tableWrapper).toHaveScreenshot('text_memory_table_reset.png', {
            maxDiffPixels: 500,
        });
    });

    // 表格跳转至Timeline
    test('test_redirectToTimeline_when_rightClickTable', async ({ page, memoryPage }) => {
        const { memoryFrame } = memoryPage;
        const { fullPage } = new TimelinePage(page);
        await page.waitForTimeout(2000);
        await memoryFrame.locator('tr:nth-child(3) > td:nth-child(15)').getByRole('button').click();
        await page.waitForTimeout(2000);
        await page.mouse.move(0, 0);
        try {
            await expect(fullPage).toHaveScreenshot('text_memory_redirectToTimeline.png', {
                maxDiffPixels: 500,
            });
        }catch (e) {
            await expect(fullPage).toHaveScreenshot('text_memory_redirectToTimeline-1.png', {
                maxDiffPixels: 500,
            });
        }
    });

    // 底部表格联合筛选后结果加载
    test('query_memoryDetailTable_by_jointFiltering', async ({ page, memoryPage, ws }) => {
        const { memoryFrame } = memoryPage;
        const searchPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
        await page.waitForTimeout(1000);
        await memoryFrame.getByLabel('Name').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Search Name').fill('aten::empty_strided');
        await memoryFrame.getByRole('button', { name: 'search Search' }).click();
        await searchPromise;

        await page.waitForTimeout(1000);
        await memoryFrame.getByLabel('Size(KB)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Min').fill('0');
        await memoryFrame.getByPlaceholder('Max').fill('30');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await searchPromise;
        await page.waitForTimeout(1000);

        await memoryFrame.getByLabel('Allocation Time(ms)').getByRole('button').click();
        await memoryFrame.getByPlaceholder('Min').nth(1).fill('0');
        await memoryFrame.getByPlaceholder('Max').nth(1).fill('50');
        await memoryFrame.getByRole('button', { name: 'Search' }).click();
        await searchPromise;
        await page.waitForTimeout(1000);

        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page').locator('.mi-collapsible-panel').nth(1)).toHaveScreenshot('text-memory-tableData-joint-filtering.png', {
            maxDiffPixels: 500,
        });
    });
});
