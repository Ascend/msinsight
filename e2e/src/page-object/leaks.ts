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

import { expect, type FrameLocator, type Locator, type Page } from '@playwright/test';
import { FrameworkPage } from './framework';

export class LeaksPage {
    readonly page: Page;
    readonly leaksFrame: FrameLocator;
    readonly threadIdSelector: Locator;
    readonly funcsSelector: Locator;
    readonly deviceIdSelector: Locator;
    readonly typeSelector: Locator;

    constructor(page: Page) {
        this.page = page;
        this.leaksFrame = page.frameLocator('#MemScope');
        this.threadIdSelector = this.leaksFrame.locator('#select-threadId');
        this.funcsSelector = this.leaksFrame.locator('#select-funcName');
        this.deviceIdSelector = this.leaksFrame.locator('#select-deviceId');
        this.typeSelector = this.leaksFrame.locator('#select-type');
    }

    async goto(): Promise<void> {
        const frameworkPage = new FrameworkPage(this.page);
        await frameworkPage.clickTab('MemScope');
        await this.page.mouse.click(0, 0);
    }

    async waitForReady(): Promise<void> {
        const diagram = this.leaksFrame.getByTestId('blockDiagramSection');
        await expect(diagram).toHaveAttribute('data-loading-blocks', 'false', { timeout: 30000 });
        await expect(diagram).toHaveAttribute('data-blocking-spinner-visible', 'false');
        await expect(diagram).toHaveAttribute('data-progressive-loading-visible', 'false', { timeout: 30000 });
        await expect(diagram).toHaveAttribute('data-progressive-render-percent', '100', { timeout: 30000 });
        await expect(diagram).toHaveAttribute('data-graph-max-size', /[1-9]/);
        await expect(diagram.locator('canvas')).toBeVisible();
    }

    async openSystemView(): Promise<void> {
        await this.leaksFrame.locator('div[style*="width: 90px"][style*="height: 20px"]').click();
        await this.leaksFrame.getByRole('tab', { name: 'System View', exact: true }).click();
        await expect(this.leaksFrame.getByTestId('blocksTable').locator('tbody .locate-link').first()).toBeVisible();
    }

    async selectOption(input: Locator, option: string): Promise<void> {
        await input.locator('xpath=ancestor::*[contains(concat(" ", normalize-space(@class), " "), " ant-select ")][1]')
            .locator('.ant-select-selector').click();
        const menuId = await input.getAttribute('aria-controls');
        if (!menuId) throw new Error('Select input must reference its menu');
        const menu = this.leaksFrame.locator('.ant-select-dropdown')
            .filter({ has: this.leaksFrame.locator(`[id="${menuId}"]`) });
        await menu.getByTitle(option, { exact: true }).click();
        await input.press('Escape');
        await expect(menu).toBeHidden();
        await this.page.mouse.click(0, 0);
    }

    async selectedDetailPanel(): Promise<Locator> {
        const tab = this.leaksFrame.getByRole('tab', { name: /^#-?\d+$/ }).first();
        await expect(tab).toBeVisible({ timeout: 30000 });
        await tab.click();
        const panelId = await tab.getAttribute('aria-controls');
        if (!panelId) throw new Error('Detail tab must reference a panel');
        const panel = this.leaksFrame.locator(`[id="${panelId}"]`);
        await expect(panel).toBeVisible();
        const id = (await tab.innerText()).trim().replace(/^#/, '');
        await expect(panel.getByTitle('ID', { exact: true }).locator('xpath=following-sibling::*[1]')).toHaveText(id);
        const address = panel.getByTitle('Address', { exact: true }).locator('xpath=following-sibling::*[1]');
        await expect(address).toHaveText(/^(?:0x[\da-f]+|\d+)$/i);
        return panel;
    }

    async clickMemoryBlock(): Promise<void> {
        const canvas = this.leaksFrame.getByTestId('blockDiagramSection').locator('canvas');
        const bounds = await canvas.boundingBox();
        if (!bounds) throw new Error('Lifecycle canvas must be visible');
        // 点击图形下方的活动内存区域，避免旧坐标落在折线与内存块之间的空白处。
        await canvas.click({ position: { x: bounds.width * 0.5, y: bounds.height * 0.9 } });
        await this.page.mouse.move(0, 0);
    }

    async clearData(): Promise<void> {
        const framework = new FrameworkPage(this.page);
        await this.page.mouse.click(0, 0);
        await expect(framework.settingsBtn).not.toHaveClass(/disabled/);
        await framework.settingsBtn.click();
        await this.page.getByText('All', { exact: true }).click();
        await framework.deleteSelectedBtn.click();
        await expect(framework.deleteProjectsDialog).toBeVisible();
        // 确认浮层出现时可能仍在移动，键盘确认避免鼠标点击落在旧位置。
        await framework.deleteProjectsConfirmBtn.focus();
        await framework.deleteProjectsConfirmBtn.press('Enter');
        // 大数据清理可能超过公共辅助方法的 5 秒等待，必须等项目确实移除后再结束用例。
        await expect(framework.projectList).toHaveText('', { timeout: 30000 });
        await expect(framework.deleteProjectsDialog).toBeHidden();
        await expect(framework.importDataBtn).toBeVisible();
    }

    async addMemoryMarker(y = 120): Promise<void> {
        await this.leaksFrame.getByRole('button', {
            name: 'Click the marker axis to add a memory marker', exact: true,
        }).click({ position: { x: 10, y } });
        await this.page.mouse.move(0, 0);
    }

    async hoverMemoryBlock(): Promise<void> {
        const canvas = this.leaksFrame.getByTestId('blockDiagramSection').locator('canvas');
        const bounds = await canvas.boundingBox();
        if (!bounds) throw new Error('Lifecycle canvas must be visible');
        await canvas.focus();
        await canvas.hover({ position: { x: bounds.width * 0.5, y: bounds.height * 0.9 } });
        await expect(this.leaksFrame.getByTestId('blockMarkerShortcutHint')).toBeVisible();
    }
}
