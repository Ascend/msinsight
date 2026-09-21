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

import { test as baseTest, WebSocket } from '@playwright/test';
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

// 定义测试数据集
const testDataCases = [
    { caseName: 'level0_text', dataFile: FilePath.DATA_01, expectTime: 5263 },
    { caseName: '02_test_level1_PipeUtilization', dataFile: FilePath.DATA_02, expectTime: 9312 },
    { caseName: 'level1_default_text', dataFile: FilePath.DATA_03, expectTime: 1312 },
    { caseName: 'level2_default_text', dataFile: FilePath.DATA_04, expectTime: 1298 },
    { caseName: 'test_ascend_pytorch_profiler_type_db', dataFile: FilePath.DATA_05, expectTime: 1295 },
    { caseName: 'level_none_mstx', dataFile: FilePath.DATA_06, expectTime: 21393 },
    { caseName: '08_7G', dataFile: FilePath.DATA_07, expectTime: 102435 }
];

// 定义测试套件工厂函数
function defineDataTestSuite(testCase) {
    test.describe(testCase.caseName, () => {
        let allPagesSuccessRes;

        test.beforeEach(async ({ page }) => {
            // 设置WebSocket事件监听器
            allPagesSuccessRes = waitForWebSocketEvent(page, (res) => res?.event === 'allPagesSuccess');

            // 初始化页面框架
            const { loadingDialog } = new FrameworkPage(page);

            // 导航到首页并导入测试数据
            await page.goto('/');
            await importData(page, testCase.dataFile);

            // 模拟鼠标移动
            await page.mouse.move(0, 0);

            // 等待加载对话框消失
            if (await loadingDialog.count()) {
                await loadingDialog.waitFor({ state: 'detached' });
            }
        });

        // 定义测试用例
        test(testCase.caseName, async ({}) => {
            const timestamp1 = new Date().getTime();
            await allPagesSuccessRes;
            const timestamp2 = new Date().getTime();
            const time = timestamp2 - timestamp1;
            let res = false;
            if (time < testCase.expectTime * 1.15) {
                res = true;
            }

            console.log(`${testCase.caseName} ${time} ${res}`);
        });
    });
}

// 为每个测试用例创建测试套件
testDataCases.forEach(testCase => {
    defineDataTestSuite(testCase);
});
