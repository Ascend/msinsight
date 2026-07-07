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

import { test as baseTest, expect, type FrameLocator, WebSocket } from '@playwright/test';
import { FrameworkPage, OperatorPage } from '@/page-object';
import { clearAllData, importData, setCompare, setupWebSocketListener, waitForResponse, waitForWebSocketEvent } from '@/utils';
import { SelectHelpers } from '@/components';
import { FilePath } from '@/utils/constants';

interface TestFixtures {
    operatorPage: OperatorPage;
    ws: Promise<WebSocket>;
}
const test = baseTest.extend<TestFixtures>({
    operatorPage: async ({ page }, use) => {
        const memoryPage = new OperatorPage(page);
        await use(memoryPage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});

const operatorImgMap = {
    loadOperatorDataSuccess: 'operator.png',
    expandOperatorDetailTableDataSuccess: 'operator-expand-detail.png',
    compareRankRes: 'operator-compare-rank.png',
};

test.describe('Operator(SingleMachine)', () => {
    test.beforeEach(async ({ page, operatorPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await operatorPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.DB);
        await allCardParsedPromise;
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // 【case】非对比非多机operator界面加载
    test('change_filterCondition', async ({ page, operatorPage }) => {
        const { operatorFrame, groupIdSelector, rankIdSelector, topSelector } = operatorPage;
        const groupIdSelect = new SelectHelpers(page, groupIdSelector, operatorFrame);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, operatorFrame);
        const topSelect = new SelectHelpers(page, topSelector, operatorFrame);
        await groupIdSelect.open();
        await groupIdSelect.selectOption('Computing Operator Type');
        await rankIdSelect.open();
        await rankIdSelect.selectOption('2');
        await topSelect.open();
        await topSelect.selectOption('15');
        await page.mouse.move(0, 0);
        await expect(operatorFrame.locator('.mi-page')).toHaveScreenshot(operatorImgMap.loadOperatorDataSuccess, {
            maxDiffPixels: 500,
        });
    });

    // 【case】非对比非多机see more表格加载
    test('expand_table_when_click_seeMoreCell', async ({ page, operatorPage }) => {
        const { operatorFrame, rankIdSelector } = operatorPage;
        const seeMoreBtn = operatorFrame.getByRole('button', { name: 'Details' }).first();
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, operatorFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('0');
        await seeMoreBtn.click();
        await page.mouse.move(0, 0);
        await expect(operatorFrame.locator('.mi-page')).toHaveScreenshot(operatorImgMap.expandOperatorDetailTableDataSuccess, {
            maxDiffPixels: 500,
        });
    });

    // 对比数据
    test('operator_compare_rank', async ({ page, operatorPage }) => {
        const { operatorFrame } = operatorPage;
        await setCompare(page, operatorFrame);
        await operatorFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
        await expect(operatorFrame.locator('.mi-page')).toHaveScreenshot(operatorImgMap.compareRankRes, {
            maxDiffPixels: 500,
        });
        await page.waitForTimeout(2000); // 对比场景需要加延时，确保稳定
    });
});

// 多机多卡测试
test.describe('Operator(MultiMachines)', () => {
    test.beforeEach(async ({ page, operatorPage, ws }) => {
        const allCardParsedPromise = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await operatorPage.goto();
        await clearAllData(page);
        await importData(page, FilePath.MULTI_NODES);
        await allCardParsedPromise;
    });

    // 多机多卡数据界面正常加载，切换卡序号正常显示
    test('operator_multi_machine_display', async ({ page, operatorPage }) => {
        const { operatorFrame, rankIdSelector } = operatorPage;
        const seeMoreBtn = operatorFrame.getByRole('button', { name: 'Details' }).first();
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, operatorFrame);
        await rankIdSelect.open();
        await rankIdSelect.selectOption('1');
        await seeMoreBtn.click();
        await page.mouse.move(0, 0);
        await expect(operatorFrame.locator('.mi-page')).toHaveScreenshot('operator-multi-machine.png', {
            maxDiffPixels: 500,
        });
    });

    // 多机多卡数据切换机器时自动选中首张卡
    test('test_pageDisplay_when_change_host', async ({ page, operatorPage }) => {
        const { operatorFrame, hostSelector, rankIdSelector } = operatorPage;
        const hostSelect = new SelectHelpers(page, hostSelector, operatorFrame);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, operatorFrame);
        await hostSelect.open();
        await hostSelect.selectOption('ubuntu22044973785946912235777_0');
        expect(await rankIdSelect.getValue()).toBe('8');
        await hostSelect.open();
        await hostSelect.selectOption('node18899436934890168541_0');
        expect(await rankIdSelect.getValue()).toBe('0');
    });

    // 点击侧边栏目录树切换到对应的卡
    test('test_pageDisplay_when_click_rank', async ({ page, operatorPage }) => {
        const frameworkPage = new FrameworkPage(page);
        const { operatorFrame, hostSelector, rankIdSelector } = operatorPage;
        const hostSelect = new SelectHelpers(page, hostSelector, operatorFrame);

        const rankIdSelect = new SelectHelpers(page, rankIdSelector, operatorFrame);
        const dbHost0Rank1 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_1);
        await dbHost0Rank1.click();
        expect(await hostSelect.getValue()).toBe('node18899436934890168541_0');
        expect(await rankIdSelect.getValue()).toBe('1');

        const dbHost1Rank1 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_1_RANK_0);
        await dbHost1Rank1.click();
        await page.waitForTimeout(1000);
        expect(await hostSelect.getValue()).toBe('ubuntu22044973785946912235777_0');
        expect(await rankIdSelect.getValue()).toBe('8');
        await page.mouse.move(0, 0);
        await expect(operatorFrame.locator('.mi-page')).toHaveScreenshot('operator-multi-click-rank.png', {
            maxDiffPixels: 500,
        });
    });

    // 切换工程测试
    test('test_switch_project', async ({ page, operatorPage, ws }) => {
        test.setTimeout(90_000);
        // 导入text类型数据，导入后会自动选中此数据0卡
        const allCardParsedPromise = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await importData(page, FilePath.TEXT);
        await allCardParsedPromise;

        const frameworkPage = new FrameworkPage(page);
        const { operatorFrame, hostSelector, rankIdSelector, fullPage } = operatorPage;
        const hostSelect = new SelectHelpers(page, hostSelector, operatorFrame);
        const rankIdSelect = new SelectHelpers(page, rankIdSelector, operatorFrame);

        // text切换到db数据
        // 选择db数据0卡，此时会重新加载
        const dbRank0 = frameworkPage.getRankLocator(FilePath.MULTI_NODES_NODE_0_RANK_0);
        const statisticPromise = waitForResponse(await ws, (res) => res?.command === 'operator/statistic');
        await dbRank0.click();

        const allCardParsedPromise2 = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await allCardParsedPromise2;

        await page.mouse.move(0, 0);
        await statisticPromise;
        await page.waitForTimeout(2000);

        await hostSelector.waitFor({ state: 'attached' });
        const hostText = await hostSelect.getValue();
        expect(hostText).toBe('node18899436934890168541_0');
        const selectedText = await rankIdSelect.getValue();
        expect(selectedText).toBe('0');
        await expect(fullPage).toHaveScreenshot('operator-text-to-db.png', {
            maxDiffPixels: 500,
        });

        // db切换到text数据
        const statisticPromise2 = waitForResponse(await ws, (res) => res?.command === 'operator/statistic');
        const textRank1 = frameworkPage.getRankLocator(FilePath.TEXT_RANK_2);
        await textRank1.click();
        await page.mouse.move(0, 0);
        await statisticPromise2;
        await page.waitForTimeout(1000);

        await hostSelector.waitFor({ state: 'detached' });
        await page.waitForTimeout(400);
        const selectedText2 = await rankIdSelect.getValue();
        expect(selectedText2).toBe('2');
        await expect(fullPage).toHaveScreenshot('operator-db-to-text.png', {
            maxDiffPixels: 500,
        });
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });
});
