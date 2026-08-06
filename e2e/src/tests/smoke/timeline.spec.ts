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

import { cp } from 'fs/promises';
import { test as baseTest, expect, WebSocket } from '@playwright/test';
import { TimelinePage } from '@/page-object';
import { clearAllData, importData, setupWebSocketListener, waitForWebSocketEvent } from '@/utils';
import { FilePath } from '@/utils/constants';

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
let allPagesSuccessRes: Promise<unknown>;
test.describe('Timeline', () => {
    test.beforeEach(async ({ page, timelinePage, ws }) => {
        allPagesSuccessRes = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        await page.waitForTimeout(2000);
        await timelinePage.goto();
        await importData(page, FilePath.SMOKE_DATA);
        await allPagesSuccessRes;
    });

    // 展示标志
    test('display marker', async ({ page, timelinePage }) => {
        await expect(page.locator('iframe[name="Timeline"]').contentFrame().getByTestId('tool-marker')).toBeVisible();
        await page.waitForTimeout(2000);
    });
});

// 回归校验相同 processId 的 PyTorch 与 CANN 不会被错误合并，并确保 acl 子泳道正确挂载在 CANN 下
test.describe('Timeline metadata hierarchy', () => {
    test.beforeEach(async ({ page, timelinePage, ws }, testInfo) => {
        const allPagesSuccessRes = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        const testDataPath = testInfo.outputPath('rank_0_ascend_pt', 'ASCEND_PROFILER_OUTPUT');
        await cp(FilePath.SMOKE_DATA_RANK_0, testDataPath, { recursive: true });
        await timelinePage.goto();
        await ws;
        await clearAllData(page);
        await importData(page, testDataPath);
        await allPagesSuccessRes;
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    test('test_hostCannLaneHierarchy_when_expandThread', async ({ timelinePage }) => {
        const processUnit = timelinePage.getUnitByName('Process 3430895');
        await expect(processUnit).toHaveCount(1);
        await timelinePage.expandUnit(processUnit);

        const threadUnit = timelinePage.getUnitByName('Thread 3430895');
        await expect(threadUnit).toHaveCount(1);
        await timelinePage.expandUnit(threadUnit);

        const pytorchUnit = timelinePage.getUnitByName('PyTorch');
        const cannUnit = timelinePage.getUnitByName('CANN');
        await expect(pytorchUnit).toBeVisible();
        await expect(cannUnit).toBeVisible();

        const aclUnit = timelinePage.getUnitByName('acl');
        await expect(aclUnit).toBeHidden();
        await timelinePage.expandUnit(cannUnit);
        await expect(aclUnit).toBeVisible();
        await timelinePage.collapseUnit(cannUnit);
        await expect(aclUnit).toBeHidden();
    });
});
