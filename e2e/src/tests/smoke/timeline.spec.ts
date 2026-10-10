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

import { test as baseTest, expect, WebSocket } from '@playwright/test';
import { TimelinePage } from '@/page-object';
import { importData, setupWebSocketListener, waitForWebSocketEvent } from '@/utils';
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
