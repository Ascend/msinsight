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
import { CommunicationPage, FrameworkPage } from '@/page-object';
import { importData, setupWebSocketListener, waitForWebSocketEvent } from '@/utils';
import { FilePath } from '@/utils/constants';

interface TestFixtures {
    communicationPage: CommunicationPage;
    ws: Promise<WebSocket>;
}
const test = baseTest.extend<TestFixtures>({
    communicationPage: async ({ page }, use) => {
        const communicationPage = new CommunicationPage(page);
        await use(communicationPage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});
let allPagesSuccessRes: Promise<unknown>;
let clusterCompletedRes: Promise<unknown>;
test.describe('Communication', () => {
    test.beforeEach(async ({ page, communicationPage, ws }) => {
        allPagesSuccessRes = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');
        clusterCompletedRes = waitForWebSocketEvent(page, (res) => res?.event === 'parse/clusterCompleted');

        const { loadingDialog } = new FrameworkPage(page);
        await page.waitForTimeout(2000);
        await page.goto('/');
        await importData(page, FilePath.SMOKE_DATA);
        await allPagesSuccessRes;
        await clusterCompletedRes;
        await communicationPage.goto();
        await page.mouse.move(0,0);
        if (await loadingDialog.count()) {
            await loadingDialog.waitFor({ state: 'detached' });
        }
    });

    // 展示通信矩阵和HCCL图和通信耗时
    test('display communication matrix and HCCL graph and communication time', async ({ page, communicationPage }) => {
        await expect(page.locator('iframe[name="Communication"]').contentFrame().getByText('default_group:(0, 1, 2, 3)')).toBeVisible();
        await expect(page.locator('iframe[name="Communication"]').contentFrame().getByTestId('filters').getByText('Total Op Info')).toBeVisible();
        await expect(page.locator('iframe[name="Communication"]').contentFrame().locator('#matrixchart canvas')).toBeVisible();
        await page.locator('iframe[name="Communication"]').contentFrame().getByRole('radio', { name: 'Communication Duration' }).click();
        await expect(page.locator('iframe[name="Communication"]').contentFrame().locator('#hccl canvas[data-zr-dom-id]')).toBeVisible();
        await expect(page.locator('iframe[name="Communication"]').contentFrame().locator('#main canvas')).toBeVisible();
        await page.waitForTimeout(2000);
    });
});
