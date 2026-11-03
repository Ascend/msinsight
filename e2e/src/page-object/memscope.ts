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

import { type Page, type Locator } from '@playwright/test';
import { MemoryAnalysisPage } from './memory-analysis';

export class MemScopePage extends MemoryAnalysisPage {
    readonly threadIdSelector: Locator;
    readonly funcsSelector: Locator;
    constructor(page: Page) {
        super(page, 'MemScope');
        this.threadIdSelector = this.memoryFrame.locator('#select-threadId');
        this.funcsSelector = this.memoryFrame.locator('#select-funcName');
    }
}
