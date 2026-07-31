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

import { expect, test, WebSocket } from '@playwright/test';
import { FrameworkPage } from '@/page-object';
import { clearAllData, importData, setupWebSocketListener, waitForResponse } from '@/utils';
import { FilePath } from '@/utils/constants';

let ws: Promise<WebSocket>;

test.describe('Framework', () => {
    test.beforeEach(async ({ page }) => {
        ws = setupWebSocketListener(page);

        const frameworkPage = new FrameworkPage(page);
        await frameworkPage.goto();
        await clearAllData(page);
    });

    test.afterEach(async ({ page }) => {
        await clearAllData(page);
    });

    test('toggle project delete mode', async ({ page }) => {
        const frameworkPage = new FrameworkPage(page);
        const { settingsBtn, deleteSelectedBtn, exitEditBtn, importDataBtn, projectList } = frameworkPage;
        const waitRes = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await importData(page, FilePath.DB);
        await waitRes;

        await settingsBtn.click();
        await expect(importDataBtn).toBeHidden();
        await expect(exitEditBtn).toBeVisible();
        await expect(deleteSelectedBtn).toHaveClass(/disabled/);
        await expect(page.getByText('All', { exact: true })).toBeVisible();
        const projectCheckbox = projectList.locator('.ant-tree-checkbox').first();
        await expect(projectCheckbox).not.toHaveClass(/ant-tree-checkbox-checked/);

        await projectCheckbox.click();
        await expect(projectCheckbox).toHaveClass(/ant-tree-checkbox-checked/);
        await expect(deleteSelectedBtn).not.toHaveClass(/disabled/);

        await exitEditBtn.click();
        await expect(importDataBtn).toBeVisible();
        await expect(settingsBtn).toBeVisible();
        await expect(projectList.locator('.ant-tree-checkbox')).toBeHidden();
    });

    // 切换主题
    test('toggle theme', async ({ page }) => {
        const frameworkPage = new FrameworkPage(page);
        await frameworkPage.switchThemeBtn.click();
        await page.mouse.move(0, 0);
        await expect(page).toHaveScreenshot('theme.png', { maxDiffPixels: 100 });
    });

    // 切换中英文
    test('toggle language', async ({ page }) => {
        const frameworkPage = new FrameworkPage(page);
        await frameworkPage.switchLanguageBtn.click();
        await page.mouse.move(0, 0);
        await expect(page).toHaveScreenshot('language.png', { maxDiffPixels: 100 });
    });

    // 查看帮助信息
    test('view help info', async ({ page }) => {
        const frameworkPage = new FrameworkPage(page);
        const { helpInfoDialog, shortcutsDialog } = frameworkPage;
        await frameworkPage.helpInfoBtn.click();
        await page.getByText('Keyboard shortcuts').click();
        await expect(shortcutsDialog).toBeVisible();
        await page.getByLabel('Close', { exact: true }).click();

        await frameworkPage.helpInfoBtn.click();
        await page.getByText('About').click();
        await expect(helpInfoDialog).toBeVisible();
    });

    // 设置项目为基线
    test('set project as baseline', async ({ page  }) => {
        const frameworkPage = new FrameworkPage(page);

        const waitFirstRes = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await importData(page, FilePath.DB);
        await waitFirstRes;
        const waitSecondRes = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        await importData(page, FilePath.MULTI_NODES);
        await waitSecondRes;
        await frameworkPage.projectList.getByText(FilePath.DB, { exact: true }).click({
            button: 'right',
        });
        await page.getByText('Set as Baseline Data').click();
        await page.waitForTimeout(1000);
        await page.mouse.move(0, 0);
        await expect(frameworkPage.projectList).toHaveScreenshot('set-project-baseline.png', { maxDiffPixels: 500 });
        await page.waitForTimeout(2000);
    });
});
