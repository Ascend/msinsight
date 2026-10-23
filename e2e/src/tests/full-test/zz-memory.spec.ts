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

import { expect, test as baseTest, WebSocket, type Locator, type Page } from '@playwright/test';
import { FileExploreDialogPage, FrameworkPage, MemoryPage, TimelinePage } from '@/page-object';
import { clearAllData, dragSelect, importData, observeWebSocketRequests, setCompare, setupWebSocketListener, waitForResponse, waitForWebSocketEvent } from '@/utils';
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
    test('loadMemoryPageSuccess_when_dataParseSuccess', async ({ page, memoryPage, ws }) => {
        const { memoryFrame, rankIdSelector } = memoryPage;
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        const rankChanged = await rankIdSelect.getValue() !== '0';
        const websocket = await ws;
        const dataLoaded = rankChanged
            ? Promise.all([
                waitForResponse(websocket, res => res?.command === 'Memory/view/memoryUsage'),
                waitForResponse(websocket, res => res?.command === 'Memory/view/operator'),
            ])
            : Promise.resolve();
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await dataLoaded;
        // 解析完成事件不代表切卡后的曲线和表格已完成加载。
        await expect(memoryFrame.locator('.ant-spin-spinning')).toHaveCount(0);
        await page.mouse.move(0, 0);
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-single-rank-0.png', {
            maxDiffPixels: 500,
            timeout: 20_000,
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
        const { memoryFrame, rankIdSelector } = memoryPage;
        const websocket = await ws;

        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await memoryFrame.locator('.ant-spin-spinning').first().waitFor({ state: 'hidden' });
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
        const { memoryFrame, rankIdSelector } = memoryPage;
        const { fullPage } = new TimelinePage(page);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await page.waitForTimeout(2000);
        await memoryFrame.getByRole('row').filter({ has: memoryFrame.getByRole('cell', { name: 'aten::empty_strided', exact: true }) }).nth(1).getByRole('button', { name: 'Show in Timeline', exact: true }).click();

        await expect(fullPage.getByText('aten::empty_strided', { exact: true })).toBeVisible({ timeout: 20_000 });
        await expect(fullPage.getByText('NPU Metrics', { exact: true })).toBeVisible({ timeout: 20_000 });
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        try {
            await expect(fullPage).toHaveScreenshot('redirect-to-timeline.png', {
                maxDiffPixels: 500,
            });
        } catch (e) {
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
        await page.waitForTimeout(2000); // 等待图表坐标轴完成布局，避免采样点和刻度仍在变化
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('compare-rank.png', {
            maxDiffPixels: 4000,
        });
    });

    // 【case】memory中间调整区间，底部表格仅查看在选中时间区间分配或释放内存的数据时的结果展示
    test( 'query_memoryDetailTable_by_tableFilterConditionOnlyShowAllocatedOrReleasedWithinInterval', async ({ page, memoryPage, ws }) => {
        const { memoryFrame, rankIdSelector, isOnlyShowAllocatedOrReleasedWithinIntervalChecker } = memoryPage;
        const isOnlyShowAllocatedOrReleasedWithinIntervalCheckbox = new CheckboxHelpers(page, isOnlyShowAllocatedOrReleasedWithinIntervalChecker, memoryFrame);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await memoryFrame.locator('.ant-spin-spinning').first().waitFor({ state: 'hidden' });
        const totalNumListItem = memoryFrame.locator('.ant-spin-container > ul > li.ant-pagination-total-text');
        const initialTotal = Number((await totalNumListItem.innerText()).match(/\d+/)?.[0]);
        expect(initialTotal).toBeGreaterThan(0);

        const chart = memoryPage.usageChart;
        await page.waitForTimeout(1000);
        const chartInfo = await chart.boundingBox();

        // 等待echarts加载完成才能框选
        await page.waitForTimeout(1000);
        const { x: startX, y: startY } = chartInfo;
        await page.mouse.move(startX + 300, startY + 200);
        await page.mouse.down();
        await page.mouse.move(startX + 400, startY + 200);
        await page.mouse.up();

        const filteredPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
        await isOnlyShowAllocatedOrReleasedWithinIntervalCheckbox.click();
        await filteredPromise;
        expect(await isOnlyShowAllocatedOrReleasedWithinIntervalCheckbox.isChecked()).toBe(true);

        const spin = memoryFrame.locator('.panel-content .ant-spin-dot-spin');
        // 等待 loading 结束后端返回更新 totalNum
        await spin.waitFor({ state: 'detached' });
        await page.mouse.move(0, 0);

        await expect.poll(async () => Number((await totalNumListItem.innerText()).match(/\d+/)?.[0]))
            .toBeLessThan(initialTotal);

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
    test('test_pageDisplay_compare_rank_db', async ({ page, memoryPage, ws }) => {
        test.setTimeout(160000);
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
        const filtered = waitForResponse(await ws, res => res?.command === 'Memory/view/operator');
        await queryBtn.click();
        await filtered;
        await memoryFrame.locator('.ant-spin-spinning').first().waitFor({ state: 'hidden' });
        await memoryFrame.getByText('Difference').first().waitFor({ state: 'visible' });
        await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('memory-compare-rank-db-with-condition.png', {
            maxDiffPixels: 500,
        });
        const reset = waitForResponse(await ws, res => res?.command === 'Memory/view/operator');
        await resetBtn.click();
        await reset;
        await minSizeInput.expectValueToBe('-243840');
        await page.waitForTimeout(2000); // 对比场景需要加延时，确保稳定
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});

test.describe('Memory(Pytorch_SwitchProject)', () => {
    test.beforeEach(async ({ page, memoryPage, ws }) => {
        test.setTimeout(90_000);
        const textParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await memoryPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.TEXT);
        await textParsedPromise;
        const frameworkPage = new FrameworkPage(page);
        const dialog = new FileExploreDialogPage(page);
        const directoryRequests = observeWebSocketRequests<{ path: string }, unknown>(await ws, 'files/get');
        try {
            await frameworkPage.importDataBtn.click();
            await expect(dialog.mainDialog).toBeVisible();
            // 再次打开时会自动定位上次目录，等待其完成，避免覆盖新输入的路径。
            await directoryRequests.waitFor(params => params.path === FilePath.TEXT);
            const selectedPath = dialog.fileTree.locator('.ant-tree-node-selected .ant-tree-title > span[id]');
            await expect(selectedPath).toHaveAttribute('id', FilePath.TEXT);
            await expect(dialog.input).toHaveValue(FilePath.TEXT);
            await dialog.input.fill(FilePath.MULTI_NODES);
            await dialog.input.press('Enter');
            await expect(selectedPath).toHaveAttribute('id', FilePath.MULTI_NODES, { timeout: 30_000 });
        } finally {
            directoryRequests.dispose();
        }
        const dbParsedPromise = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await dialog.confirmBtn.click();
        await expect(dialog.mainDialog).toBeHidden();
        await dbParsedPromise;
        // Import completion can precede Memory's initial Rank selection and curve response.
        await frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_0).click();
        await expect(memoryPage.memoryFrame.getByTestId('memory-range-navigation'))
            .toHaveAttribute('data-rank-id', 'node18899436934890168541_0 0');
        await expect(memoryPage.memoryFrame.getByRole('button', { name: 'Show current range in Timeline', exact: true })).toBeEnabled();
    });

    // 测试从多机多卡db类型数据切换到单机多卡text类型数据
    test('test_pageDisplay_when_db_to_text', async ({ page, memoryPage, ws }) => {
        const frameworkPage = new FrameworkPage(page);
        const websocket = await ws;
        const dbRank0 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_0);
        await dbRank0.click();
        const textRank2 = frameworkPage.getRankLocator(FilePath.TEXT_RANK_2);
        const textLoadedPromise = waitForResponse(websocket, (res) => res?.command === 'Memory/view/memoryUsage');
        await textRank2.click();
        await textLoadedPromise;
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
        const curves = observeWebSocketRequests<{ rankId: string }, {
            lines: Array<Array<string | number>>;
        }>(await ws, 'Memory/view/memoryUsage');
        try {
            const textRank0 = frameworkPage.getRankLocator(FilePath.TEXT_RANK_0);
            await textRank0.click();

            const dbRank1 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_1);
            const allCardParsedPromise = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
            const searchPromise = waitForResponse(await ws, (res) => res?.command === 'Memory/view/operator');
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
            await searchPromise;
            const curve = await curves.waitFor(params => params.rankId === 'node18899436934890168541_0 1');
            expect(curve.response.result).toBe(true);
            const { lines } = curve.response.body;
            expect(lines.length).toBeGreaterThan(1);
            const navigation = memoryFrame.getByTestId('memory-range-navigation');
            await expect(navigation).toHaveAttribute('data-rank-id', curve.params.rankId);
            await expect(navigation).toHaveAttribute('data-start-ms', String(Number(lines[0][0])));
            await expect(navigation).toHaveAttribute('data-end-ms', String(Number(lines[lines.length - 1][0])));
            await expect(memoryFrame.getByRole('button', { name: 'Show current range in Timeline', exact: true })).toBeEnabled();
            // 等待 echarts 动画结束
            await page.waitForTimeout(2000);
            await expect(memoryFrame.locator('.mi-page')).toHaveScreenshot('pytorch-text-to-db.png', {
                maxDiffPixels: 500,
            });
        } finally {
            curves.dispose();
        }
    });

    test.afterEach(async ({ page, ws }) => {
        const dialog = new FileExploreDialogPage(page);
        if (await dialog.mainDialog.isVisible()) {
            await dialog.mainDialog.getByRole('button', { name: 'Cancel', exact: true }).click();
            await expect(dialog.mainDialog).toBeHidden();
        }
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
        const { memoryFrame, rankIdSelector } = memoryPage;
        const { fullPage } = new TimelinePage(page);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, memoryFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await page.waitForTimeout(2000);
        await memoryFrame.getByRole('row').filter({ has: memoryFrame.getByRole('cell', { name: 'aten::empty_strided', exact: true }) }).nth(1).getByRole('button', { name: 'Show in Timeline', exact: true }).click();
        await expect(fullPage.getByText('aten::empty_strided', { exact: true })).toBeVisible({ timeout: 20_000 });
        await expect(fullPage.getByText('NPU Metrics', { exact: true })).toBeVisible({ timeout: 20_000 });
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        try {
            await expect(fullPage).toHaveScreenshot('text_memory_redirectToTimeline.png', {
                maxDiffPixels: 500,
            });
        } catch (e) {
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

// Memory 曲线范围跳转回 Timeline 的 GUI 看护。
test.describe('Memory(RangeNavigation)', () => {
    type CurveParams = { rankId: string; start: string; end: string; isCompare: boolean };
    type Curve = { lines: Array<Array<string | number>>; rankOffsetNs: number };
    type Curves = ReturnType<typeof observeWebSocketRequests<CurveParams, Curve>>;
    type TraceRequests = ReturnType<typeof observeWebSocketRequests<{ cardId: string }, unknown>>;
    type CurveResult = Awaited<ReturnType<Curves['waitFor']>>;
    type RangeFixtures = { curves: Curves };

    const rangeTest = test.extend<RangeFixtures>({
        curves: async ({ page, ws }, use) => {
            await new FrameworkPage(page).goto();
            const curves = observeWebSocketRequests<CurveParams, Curve>(await ws, 'Memory/view/memoryUsage');
            try {
                await use(curves);
            } finally {
                curves.dispose();
                await page.mouse.move(0, 0);
                await clearAllData(page);
            }
        },
    });

    // DB data uses a host-qualified internal card ID, while the selectors display the short Rank label.
    function rankLabel(rank: string): string {
        return rank.split(' ').pop() ?? rank;
    }

    function card(timeline: TimelinePage, rank: string): Locator {
        return timeline.unitWrapperScroller.locator('.unit-info').filter({
            has: timeline.timelineFrame.getByText(rankLabel(rank), { exact: true }),
        });
    }

    async function setOffsets(timeline: TimelinePage, rank: string, host: number, device: number): Promise<void> {
        const info = card(timeline, rank);
        await info.hover();
        await info.getByTestId('offset-btn').click();
        const tooltip = timeline.timelineFrame.getByRole('tooltip');
        for (const [side, value] of [['Host', host], ['Device', device]] as const) {
            const input = tooltip.getByRole('textbox', { name: `${side} Offset`, exact: true });
            await input.fill(String(value));
            await input.press('Enter');
            await expect(input).toHaveValue(String(value));
        }
        await info.getByTestId('offset-btn').click();
        await expect(tooltip).toBeHidden();
    }

    async function selectRank(page: Page, rank: string): Promise<void> {
        const label = rankLabel(rank);
        const memory = new MemoryPage(page);
        const select = new SelectHelpers(page, memory.rankIdSelector, memory.memoryFrame);
        if (await select.getValue() !== label) {
            await select.open();
            await select.selectOption(label);
        }
        await expect(memory.memoryFrame.getByRole('button', { name: 'Show current range in Timeline', exact: true })).toBeEnabled();
        await expect.poll(async () => rankLabel(await memory.memoryFrame.getByTestId('memory-range-navigation')
            .getAttribute('data-rank-id') ?? '')).toBe(label);
    }

    async function zoomMemory(page: Page, curves: Curves, rank: string): Promise<CurveResult> {
        const memory = new MemoryPage(page);
        const chart = memory.usageChart;
        await chart.scrollIntoViewIfNeeded();
        const box = await chart.boundingBox();
        if (box === null) { throw new Error('Memory chart is missing'); }
        const before = curves.cursor;
        // Keep both ends within the plot grid, away from legends and toolbox controls.
        await dragSelect(page, { x: box.x + box.width * 0.35, y: box.y + box.height * 0.55 },
            { x: box.x + box.width * 0.65, y: box.y + box.height * 0.6 });
        const result = await curves.waitFor(params => rankLabel(params.rankId) === rankLabel(rank) && params.start !== '' && params.end !== '', before);
        expect(result.response.result).toBe(true);
        const { lines } = result.response.body;
        expect(lines.length).toBeGreaterThan(1);
        const navigation = memory.memoryFrame.getByTestId('memory-range-navigation');
        await expect(navigation).toHaveAttribute('data-start-ms', String(Number(lines[0][0])));
        await expect(navigation).toHaveAttribute('data-end-ms', String(Number(lines[lines.length - 1][0])));
        await expect(memory.memoryFrame.getByRole('button', { name: 'Show current range in Timeline', exact: true })).toBeEnabled();
        return result;
    }

    async function jumpAndCheck(page: Page, result: CurveResult, snapshotName: string, deviceOffset = 0,
        traces?: TraceRequests): Promise<void> {
        const { lines, rankOffsetNs } = result.response.body;
        const start = Number(lines[0][0]) * 1e6 + rankOffsetNs - deviceOffset;
        const end = Number(lines[lines.length - 1][0]) * 1e6 + rankOffsetNs - deviceOffset;
        expect(end).toBeGreaterThan(start);
        const memory = new MemoryPage(page);
        await page.mouse.move(0, 0);
        await expect(memory.memoryFrame.locator('.mi-collapsible-panel').first()).toHaveScreenshot(`${snapshotName}-memory.png`, {
            maxDiffPixels: 500,
        });
        await memory.memoryFrame.getByRole('button', { name: 'Show current range in Timeline', exact: true }).click();
        await expect(page.locator('#Timeline')).toBeVisible();
        const timeline = new TimelinePage(page);
        // Assert the rendered viewport and selection independently; a correct duration alone can hide a shifted range.
        for (const [attribute, expected] of [
            ['data-domain-start', start], ['data-domain-end', end],
            ['data-selection-start', start], ['data-selection-end', end],
        ] as const) {
            await expect.poll(async () => Number(await timeline.mainContainer.getAttribute(attribute))).toBeCloseTo(expected, 3);
        }
        await expect(card(timeline, result.params.rankId).getByRole('checkbox')).toBeChecked();
        await expect(card(timeline, result.params.rankId)).toBeInViewport();
        await expect(card(timeline, result.params.rankId).getByTestId('expand-btn')).toHaveClass(/insight-unit-expanded/);
        if (traces !== undefined) {
            await traces.waitFor(params => params.cardId === result.params.rankId);
            await expect.poll(() => traces.pendingCount, { message: 'Waiting for Timeline trace responses' }).toBe(0);
            // Expanded Rank headers appear before their asynchronous trace requests finish.
            // Require rendered trace pixels so an empty lane cannot become a valid baseline.
            await expect.poll(() => timeline.unitWrapperScroller.locator('canvas.drawCanvas').evaluateAll(canvases =>
                canvases.some(element => {
                    const canvas = element as HTMLCanvasElement;
                    const rect = canvas.getBoundingClientRect();
                    if (rect.bottom <= 0 || rect.top >= window.innerHeight || canvas.width === 0 || canvas.height === 0) {
                        return false;
                    }
                    const context = canvas.getContext('2d');
                    if (context === null) { return false; }
                    const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
                    return pixels.some((value, index) => index % 4 === 3 && value > 0);
                })), { message: 'Waiting for visible Timeline lanes to render trace data' }).toBe(true);
        }
        await page.mouse.move(0, 0);
        await expect(timeline.mainContainer).toHaveScreenshot(`${snapshotName}-timeline.png`, {
            maxDiffPixels: 500,
        });
    }

    for (const source of [
        { name: 'Text', path: FilePath.TEXT, otherRank: '2', baseline: FilePath.TEXT_RANK_0, comparison: FilePath.TEXT_RANK_2 },
        { name: 'DB', path: FilePath.DB, otherRank: '1', baseline: FilePath.DB_RANK_0, comparison: FilePath.DB_RANK_1 },
    ]) {
        const snapshotPrefix = `memory-range-${source.name.toLowerCase()}`;
        rangeTest.describe(`Memory range navigation (${source.name})`, () => {
            rangeTest.beforeEach(async ({ page, curves, ws }) => {
                await clearAllData(page);
                const parsed = waitForResponse(await ws, res => res?.event === 'allPagesSuccess');
                await importData(page, source.path);
                await parsed;
                await expect(new TimelinePage(page).timelineFrame.getByText('NPU Metrics', { exact: true })).toBeVisible();
                void curves;
            });

            rangeTest('uses twice-refined Memory range instead of the original Timeline marker', async ({ page, curves }) => {
                const timeline = new TimelinePage(page);
                await timeline.resetBtn.click();
                await expect(timeline.mainContainer).toHaveAttribute('data-domain-start', '0');
                const chart = timeline.timelineFrame.locator('.unit > .chart, .unit > .chart-selected').first();
                const box = await chart.boundingBox();
                if (box === null) { throw new Error('Timeline chart is missing'); }
                await dragSelect(page, { x: box.x + box.width * 0.1, y: box.y + 5 },
                    { x: box.x + box.width * 0.8, y: box.y + 15 });
                await expect(timeline.mainContainer).toHaveAttribute('data-selection-start', /\d/);
                await expect(timeline.mainContainer).toHaveAttribute('data-selection-end', /\d/);
                const originalStart = Number(await timeline.mainContainer.getAttribute('data-selection-start'));
                const originalEnd = Number(await timeline.mainContainer.getAttribute('data-selection-end'));
                expect(originalEnd).toBeGreaterThan(originalStart);
                await page.keyboard.press('k');
                await new FrameworkPage(page).clickTab('memory');
                await selectRank(page, '0');
                await expect(new MemoryPage(page).memoryFrame.getByTitle('range-0', { exact: true })).toBeVisible();
                const first = await zoomMemory(page, curves, '0');
                const second = await zoomMemory(page, curves, '0');
                expect(Number(second.params.start)).toBeGreaterThan(Number(first.params.start));
                expect(Number(second.params.end)).toBeLessThan(Number(first.params.end));
                await jumpAndCheck(page, second, `${snapshotPrefix}-refined`);
                const duration = Number(await timeline.mainContainer.getAttribute('data-domain-end')) -
                    Number(await timeline.mainContainer.getAttribute('data-domain-start'));
                expect(duration).toBeLessThan(originalEnd - originalStart);
            });

            rangeTest('uses the target Rank Device offset and ignores its Host offset', async ({ page, curves }) => {
                const timeline = new TimelinePage(page);
                await setOffsets(timeline, '0', 300_000_000, 100_000_000);
                await setOffsets(timeline, source.otherRank, -200_000_000, -50_000_000);
                await new FrameworkPage(page).clickTab('memory');
                await selectRank(page, '0');
                await jumpAndCheck(page, await zoomMemory(page, curves, '0'), `${snapshotPrefix}-device-offset-rank-0`, 100_000_000);
                await new FrameworkPage(page).clickTab('memory');
                await selectRank(page, source.otherRank);
                await jumpAndCheck(page, await zoomMemory(page, curves, source.otherRank),
                    `${snapshotPrefix}-device-offset-rank-${source.otherRank}`, -50_000_000);
            });

            rangeTest('scrolls to the Rank selected in Memory when it is outside the Timeline viewport', async ({ page, curves, ws }) => {
                const timeline = new TimelinePage(page);
                await timeline.timelineFrame.getByRole('button', { name: 'Expand all lanes', exact: true }).click();
                await expect.poll(() => card(timeline, source.otherRank).count()).toBe(0);
                await expect.poll(() => timeline.unitWrapperScroller.evaluate(el => el.scrollTop)).toBe(0);
                await new FrameworkPage(page).clickTab('memory');
                await selectRank(page, source.otherRank);
                const traces = observeWebSocketRequests<{ cardId: string }, unknown>(await ws, 'unit/threadTraces');
                try {
                    await jumpAndCheck(page, await zoomMemory(page, curves, source.otherRank), `${snapshotPrefix}-switch-rank`, 0, traces);
                } finally {
                    traces.dispose();
                }
                await expect.poll(() => timeline.unitWrapperScroller.evaluate(el => el.scrollTop)).toBeGreaterThan(0);
                await expect(card(timeline, source.otherRank)).toBeInViewport();
                await expect(card(timeline, source.otherRank).getByTestId('expand-btn')).toHaveClass(/insight-unit-expanded/);
            });

            rangeTest('uses the displayed comparison curve range and its Rank', async ({ page, curves }) => {
                await new FrameworkPage(page).clickTab('memory');
                const before = curves.cursor;
                await setCompare(page, new MemoryPage(page).memoryFrame, source);
                const compared = await curves.waitFor(params => params.isCompare, before);
                await selectRank(page, compared.params.rankId);
                const refined = await zoomMemory(page, curves, compared.params.rankId);
                expect(refined.params.isCompare).toBe(true);
                await jumpAndCheck(page, refined, `${snapshotPrefix}-comparison`);
            });
        });
    }
});
