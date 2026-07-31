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

import { expect, type Locator, type Page } from '@playwright/test';

export type TabName = 'timeline' | 'memory' | 'operator' | 'summary' | 'communication' | 'Source' | 'details' | 'trace' | 'cache' | 'MemScope' | 'RL';
export class FrameworkPage {
    readonly page: Page;
    // 顶部功能按钮
    readonly settingsBtn: Locator;
    readonly deleteSelectedBtn: Locator;
    readonly exitEditBtn: Locator;
    readonly importDataBtn: Locator;
    readonly switchThemeBtn: Locator;
    readonly switchLanguageBtn: Locator;
    readonly helpInfoBtn: Locator;

    // 左侧项目列表
    readonly projectList: Locator;

    // 模块标签
    readonly timelineTab: Locator;
    readonly memoryTab: Locator;
    readonly operatorTab: Locator;
    readonly summaryTab: Locator;
    readonly communicationTab: Locator;
    readonly sourceTab: Locator;
    readonly detailsTab: Locator;
    readonly leaksTab: Locator;
    readonly reinforcementLearningTab: Locator;

    // 删除项目弹窗
    readonly deleteProjectsDialog: Locator;
    readonly deleteProjectsConfirmBtn: Locator;
    readonly deleteProjectsCancelBtn: Locator;

    // 版本信息弹窗
    readonly helpInfoDialog: Locator;

    // 快捷键弹窗
    readonly shortcutsDialog: Locator;

    readonly loadingDialog: Locator;

    constructor(page: Page) {
        this.page = page;
        this.settingsBtn = page.getByTestId('btn-set');
        this.deleteSelectedBtn = page.getByTestId('btn-delete');
        this.exitEditBtn = page.getByTestId('btn-exit');
        this.importDataBtn = page.locator('.btn-import');
        this.switchThemeBtn = page.locator('.switch-theme');
        this.switchLanguageBtn = page.getByTestId('switch-lng');
        this.helpInfoBtn = page.getByTestId('help-icon');
        this.timelineTab = page.getByRole('menuitem', { name: 'Timeline' });
        this.leaksTab = page.getByRole('menuitem', { name: 'Leaks' });
        this.memoryTab = page.getByRole('menuitem', { name: 'Memory' });
        this.operatorTab = page.getByRole('menuitem', { name: 'Operator' });
        this.summaryTab = page.getByRole('menuitem', { name: 'Summary' });
        this.communicationTab = page.getByRole('menuitem', { name: 'Communication' });
        this.sourceTab = page.getByRole('menuitem', { name: 'Source' });
        this.detailsTab = page.getByRole('menuitem', { name: 'Details' });
        this.reinforcementLearningTab = page.getByRole('menuitem', { name: 'RL' });
        this.deleteProjectsDialog = page.getByText('Are you sure to delete');
        this.deleteProjectsConfirmBtn = page.getByRole('button', { name: 'Yes' });
        this.deleteProjectsCancelBtn = page.getByRole('button', { name: 'No' });
        this.projectList = page.getByRole('tree');
        this.shortcutsDialog = page.getByLabel('Keyboard shortcuts');
        this.helpInfoDialog = page.getByLabel('About MindStudio Insight');
        this.loadingDialog = page.locator('.el-loading-mask');
    }

    async goto(): Promise<void> {
        await this.page.goto('/');
    }

    async clickTab(tabName: TabName): Promise<void> {
        const tab = this.page.getByRole('menuitem', { name: tabName });
        await tab.waitFor({ state: 'visible', timeout: 20000 });
        await tab.click();
    }

    async mouseOut(): Promise<void> {
        await this.page.mouse.move(0, 0);
    }

    getRankLocator(rankName: string): Locator {
        return this.projectList.locator('span.content-text').getByText(rankName);
    }
}

export class FileExploreDialogPage {
    readonly page: Page;
    readonly mainDialog: Locator;
    readonly cancelBtn: Locator;
    readonly confirmBtn: Locator;
    readonly closeBtn: Locator;
    readonly input: Locator;
    readonly fileTree: Locator;

    constructor(page: Page) {
        this.page = page;
        this.mainDialog = page.getByRole('dialog', { name: 'Drag or import a file by path' });
        this.cancelBtn = page.getByRole('button', { name: 'Cancel' });
        this.confirmBtn = page.getByRole('button', { name: 'Confirm' });
        this.closeBtn = page.getByLabel('Close this dialog');
        this.input = page.getByTestId('filePathInput');
        this.fileTree = this.mainDialog.getByRole('tree');
    }
}
