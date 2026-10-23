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

import type { FrameLocator, Locator, Page } from '@playwright/test';
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
}
