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

export class MemoryPage {
    readonly page: Page;
    readonly memoryFrame: FrameLocator;
    readonly usageChart: Locator;
    readonly hostSelector: Locator;
    readonly rankIdSelector: Locator;
    readonly groupIdSelector: Locator;
    readonly graphIdSelector: Locator;
    readonly nameInputor: Locator;
    readonly minSizeInputor: Locator;
    readonly maxSizeInputor: Locator;
    readonly isOnlyShowAllocatedOrReleasedWithinIntervalChecker: Locator;
    readonly queryBtn: Locator;
    readonly resetBtn: Locator;

    constructor(page: Page) {
        this.page = page;
        this.memoryFrame = page.frameLocator('#Memory');
        this.usageChart = this.memoryFrame.locator('.mi-collapsible-panel').first().locator('canvas').first();
        this.hostSelector = this.memoryFrame.locator('#select-host');
        this.rankIdSelector = this.memoryFrame.locator('#select-rankId');
        this.groupIdSelector = this.memoryFrame.locator('#select-groupId');
        this.graphIdSelector = this.memoryFrame.locator('#select-graphId');
        this.nameInputor = this.memoryFrame.locator('#input-name');
        this.minSizeInputor = this.memoryFrame.locator('#input-minSize');
        this.maxSizeInputor = this.memoryFrame.locator('#input-maxSize');
        this.isOnlyShowAllocatedOrReleasedWithinIntervalChecker =
            this.memoryFrame.locator('#input-onlyShowAllocatedOrReleased');
        this.queryBtn = this.memoryFrame.getByTestId('query-btn');
        this.resetBtn = this.memoryFrame.getByTestId('reset-btn');
    }

    async goto(): Promise<void> {
        const frameworkPage = new FrameworkPage(this.page);
        await frameworkPage.goto();
        await frameworkPage.clickTab('memory');
        await this.page.mouse.move(0,0);
    }

    async waitForRangeNavigation(): Promise<void> {
        await expect(this.memoryFrame.getByTestId('memory-range-navigation')).toHaveAttribute('data-can-locate', 'true');
    }

    async hoverRangeNavigation(): Promise<void> {
        await this.waitForRangeNavigation();
        await this.usageChart.scrollIntoViewIfNeeded();
        const box = await this.usageChart.boundingBox();
        if (box === null) { throw new Error('Memory chart is missing'); }
        // 跳转图标位于最右侧，工具栏为原生提示保留了 50px 的右边距。
        await this.usageChart.hover({ position: { x: box.width - 62.5, y: 32.5 } });
    }

    async navigateRangeToTimeline(): Promise<void> {
        await this.hoverRangeNavigation();
        const box = await this.usageChart.boundingBox();
        if (box === null) { throw new Error('Memory chart is missing'); }
        await this.usageChart.click({ position: { x: box.width - 62.5, y: 32.5 } });
    }
}
