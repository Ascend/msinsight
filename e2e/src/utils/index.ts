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

import { expect, type FrameLocator, type Page, WebSocket } from '@playwright/test';
import { FrameworkPage, FileExploreDialogPage } from '@/page-object';
import { FilePath, WEBSOCKET_URL } from './constants';

// 导入数据
export async function importData(page: Page, filePath: string = FilePath.TEXT): Promise<void> {
    const frameworkPage = new FrameworkPage(page);
    const fileExploreDialogPage = new FileExploreDialogPage(page);
    const { importDataBtn, projectList } = frameworkPage;
    const { mainDialog, input, confirmBtn, fileTree } = fileExploreDialogPage;

    // 点击“导入数据”按钮
    await importDataBtn.click();

    // 确认弹窗已打开
    await expect(mainDialog).toBeVisible();
    const emptyBlock = fileTree.locator('.el-tree__empty-block');
    await emptyBlock.waitFor({ state: 'hidden', timeout: 2000 });
    await expect(input).toBeVisible({ timeout: 2000 });
    await input.fill(filePath);
    await input.press('Enter');
    await page.waitForTimeout(2000); // 等待项目目录树返回，并选中该项目
    await expect(confirmBtn).toBeEnabled({ timeout: 10000 });
    // 点击“确认”按钮
    await confirmBtn.click();
    await expect(mainDialog).toBeHidden({ timeout: 10000 });
    const currentProject = projectList.getByText(filePath, { exact: true }).first();
    await expect(currentProject).toBeVisible({ timeout: 30000 });
}

// 清除数据
export async function clearAllData(page: Page, ws?: Promise<WebSocket>): Promise<void> {
    await page.waitForTimeout(2000); // 在清空项目之前，等待2秒，确保之前的操作或请求都执行完成，否则容易断连

    const frameworkPage = new FrameworkPage(page);
    const { settingsBtn, deleteSelectedBtn, deleteProjectsDialog, deleteProjectsConfirmBtn, importDataBtn, projectList } = frameworkPage;
    const isSettingsBtnDisabled = await settingsBtn.evaluate((el) => el.classList.contains('disabled'));

    if (isSettingsBtnDisabled) {
        return;
    }
    await settingsBtn.click();
    await page.getByText('All', { exact: true }).click();

    await deleteSelectedBtn.click();
    await expect(deleteProjectsDialog).toBeVisible();
    await deleteProjectsConfirmBtn.click();

    if (ws) {
        await waitForResponse(await ws, (res) => res?.command === 'remote/reset');
    }

    await expect(projectList).toHaveText('');
    await expect(importDataBtn).toBeVisible();
    // 等待后端完成清理动作
    await page.waitForTimeout(2000);
}

// 等待ws返回指定数据
export async function waitForWebSocketEvent<T>(page: Page, matchCondition: (payload: any) => boolean): Promise<T> {
    return new Promise((resolve, reject) => {
        page.on('websocket', (ws) => {
            const url = ws.url();
            if (url !== WEBSOCKET_URL) {
                return;
            }
            ws.on('framereceived', (event) => {
                if (typeof event.payload !== 'string') {
                    return;
                }
                try {
                    const res = JSON.parse(event.payload);
                    if (matchCondition(res)) {
                        resolve(res);
                    }
                } catch (error) {
                    reject(new Error(`WebSocket 消息解析失败: ${error}`));
                }
            });
        });
    });
}

// 监听 websocket 连接，返回 ws 实例
export function setupWebSocketListener(page: Page): Promise<WebSocket> {
    return new Promise((resolve) => {
        page.on('websocket', (ws) => {
            const url = ws.url();
            if (url === WEBSOCKET_URL) {
                resolve(ws);
            }
        });
    });
}

// 使用 ws 实例监听特定接口返回
export function waitForResponse(ws: WebSocket, matchCondition: (payload: any) => boolean): Promise<any> {
    return new Promise((resolve, reject) => {
        const frameHandler = (event: any): void => {
            if (typeof event.payload !== 'string') {
                return;
            }
            try {
                const res = JSON.parse(event.payload);
                if (matchCondition(res)) {
                    ws.off('framereceived', frameHandler);
                    resolve(res);
                }
            } catch (error) {
                reject(new Error(`WebSocket 消息解析失败: ${error}`));
            }
        };

        ws.on('framereceived', frameHandler);
    });
}

export async function setCompare(
    page: Page,
    frame: FrameLocator,
    {
        baseline,
        comparison,
    }: {
        baseline: string;
        comparison: string;
    } = { baseline: FilePath.DB_RANK_0, comparison: FilePath.DB_RANK_1 },
): Promise<void> {
    const frameworkPage = new FrameworkPage(page);
    const rank1 = frameworkPage.getRankLocator(baseline);
    await rank1.click({ button: 'right' });
    const setBaselineBtn = frameworkPage.page.getByText('Set as Baseline Data');
    await setBaselineBtn.click();
    const rank2 = frameworkPage.getRankLocator(comparison);
    await rank2.click({ button: 'right' });
    const setComparisonBtn = frameworkPage.page.getByText('Set as Comparison Data');
    await setComparisonBtn.click();
    await page.mouse.move(0,0);
    await frame.getByText('loading').first().waitFor({ state: 'hidden' });
    await frameworkPage.mouseOut();
}

export async function waitForAllParsed(page: Page, tabPage: any): Promise<void> {
    const frameworkPage = new FrameworkPage(page);
    await frameworkPage.clickTab('timeline');
    const timelineFrame = page.frameLocator('#Timeline');
    const parsingProgress = timelineFrame.locator('.unit-info .ant-progress').first();
    await parsingProgress.waitFor({ state: 'hidden' });
    await tabPage.goto();
}

/**
 * 在页面上框选一段范围
 * @param page Playwright Page 实例
 * @param start 起点坐标 { x, y }
 * @param end 终点坐标 { x, y }
 * @param steps 鼠标移动的步数（越大越平滑，默认 20）
 */
export async function dragSelect(
    page: Page,
    start: { x: number; y: number },
    end: { x: number; y: number },
    steps = 10,
) {
    // 移动到起点
    await page.mouse.move(start.x, start.y);

    // 按下鼠标左键
    await page.mouse.down();

    // 拖动到终点
    await page.mouse.move(end.x, end.y, { steps });

    // 松开鼠标
    await page.mouse.up();
}
