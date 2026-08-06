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

import { type Locator, type FrameLocator, type Page } from '@playwright/test';
import { FrameworkPage } from './framework';

export class TimelinePage {
    readonly page: Page;
    readonly timelineFrame: FrameLocator;
    readonly fullPage: Locator;
    readonly mainContainer: Locator; // 主页面（不包括底部面板）
    readonly unitWrapperScroller: Locator; // 泳道容器（不包括置顶泳道）
    readonly markerBtn: Locator;
    readonly filterBtn: Locator;
    readonly searchBtn: Locator;
    readonly flowBtn: Locator;
    readonly resetBtn: Locator;
    readonly zoomInBtn: Locator;
    readonly zoomOutBtn: Locator;
    readonly drawerBtn: Locator; // 底部面板收缩按钮
    readonly bottomPanel: Locator; // 底部面板
    readonly selectCardFilterContent: Locator;
    readonly selectUnitFilterContent: Locator;
    readonly openInWindows: Locator;

    constructor(page: Page) {
        this.page = page;
        this.timelineFrame = page.frameLocator('#Timeline');
        this.fullPage = this.timelineFrame.locator('#root');
        this.mainContainer = this.timelineFrame.locator('#main-container');
        this.unitWrapperScroller = this.timelineFrame.locator('#unitWrapperScroller');
        this.markerBtn = this.timelineFrame.getByTestId('tool-marker');
        this.filterBtn = this.timelineFrame.getByTestId('tool-filter');
        this.searchBtn = this.timelineFrame.getByTestId('tool-search');
        this.flowBtn = this.timelineFrame.getByTestId('tool-flow');
        this.resetBtn = this.timelineFrame.getByTestId('tool-reset');
        this.zoomInBtn = this.timelineFrame.getByTestId('tool-zoom-in');
        this.zoomOutBtn = this.timelineFrame.getByTestId('tool-zoom-out');
        this.drawerBtn = this.timelineFrame.getByTestId('drawer-btn').nth(2);
        this.bottomPanel = this.timelineFrame.locator('.bottomPanelContainer');
        this.selectCardFilterContent = this.timelineFrame.locator('#select-card-filter-content');
        this.selectUnitFilterContent = this.timelineFrame.locator('#select-unit-filter-content');
        this.openInWindows = this.timelineFrame.getByRole('button', { name: 'Open in Find Window' });
    }

    async goto(): Promise<void> {
        const frameworkPage = new FrameworkPage(this.page);
        await frameworkPage.goto();
        await frameworkPage.clickTab('timeline');
        await this.page.mouse.move(0,0);
    }

    async expandUnit(unitLocator: Locator): Promise<void> {
        const expandBtn = unitLocator.getByTestId('expand-btn');
        const isExpanded = await expandBtn.evaluate((el) => el.classList.contains('insight-unit-expanded'));
        if (isExpanded) {
            return;
        }
        await expandBtn.click();
    }

    async collapseUnit(unitLocator: Locator): Promise<void> {
        const expandBtn = unitLocator.getByTestId('expand-btn');
        const isFold = await expandBtn.evaluate((el) => el.classList.contains('insight-unit-fold'));
        if (isFold) {
            return;
        }
        await expandBtn.click();
    }

    getUnitByName(name: string): Locator {
        const unitName = this.page.locator('.insight-lane-info-name').getByText(name, { exact: true });
        return this.unitWrapperScroller.locator('.unit').filter({ has: unitName });
    }

    async clickMenu(clickUnit: Locator, timelineFrame: FrameLocator, option: string): Promise<void> {
        await clickUnit.click({ button: 'right' });
        const options = timelineFrame.locator('.menu-item');
        await options.getByText(option).click();
    }
}

export class SystemView extends TimelinePage {
    readonly selectSystemView: Locator;

    constructor(page: Page) {
        super(page);
        this.selectSystemView = this.timelineFrame.locator('#select-system-view');
    }

    async goto(): Promise<void> {
        await this.drawerBtn.click();
        await this.timelineFrame.getByRole('tab', { name: 'System View' }).click();
    }
}
