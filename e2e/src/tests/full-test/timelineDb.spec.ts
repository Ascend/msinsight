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

import { expect, test as baseTest, type Locator, WebSocket } from '@playwright/test';
import { CommunicationPage, SystemView, TimelinePage } from '@/page-object';
import { clearAllData, importData, observeWebSocketRequests, setupWebSocketListener, waitForResponse } from '@/utils';
import { FilePath } from '@/utils/constants';
import { InputHelpers, SelectHelpers } from '@/components';

interface TestFixtures {
    timelinePage: TimelinePage;
    ws: Promise<WebSocket>;
}
const test = baseTest.extend<TestFixtures>({
    timelinePage: async ({ page }, use) => {
        const timelinePage = new TimelinePage(page);
        await use(timelinePage);
    },
    ws: async ({ page }, use) => {
        const ws = setupWebSocketListener(page);
        await use(ws);
    },
});
interface Offsets {
    host: number;
    device: number;
}

interface CommunicationSlice {
    rankId: string;
    rawStartTime: string;
    start: number;
    duration: number;
    chart: Locator;
    position: { x: number; y: number };
}

interface AlignmentParams {
    rankId: string;
    alignType: string;
    sliceName: string;
    startTime: string;
    pid: string;
    metaType: string;
}

interface AlignmentResponse {
    result: Array<{ rankId: string; offset: number }>;
    baseOffset: number;
}

const alignmentDirections = [
    { name: 'left', menu: 'Align Left', alignType: 'LEFT' },
    { name: 'right', menu: 'Align Right', alignType: 'RIGHT' },
] as const;
const communicationOperator = 'hcom_allReduce__170_1_1';
const communicationLaneName = 'embd:Group group_name_20 Communication';

function getUnitInfo(timelinePage: TimelinePage, name: string): Locator {
    return timelinePage.unitWrapperScroller.locator('.unit-info').filter({
        has: timelinePage.timelineFrame.getByText(name, { exact: true }),
    });
}

function getPinnedCommunicationLane(timelinePage: TimelinePage, rank: string): Locator {
    const name = timelinePage.timelineFrame.locator('.insight-lane-info-name')
        .filter({ hasText: `${communicationLaneName}_Communication (HCCL)_` })
        .filter({ hasText: new RegExp(` ${rank}$`) });
    return timelinePage.timelineFrame.locator('#pinnedUnitWrapperScroller .unit').filter({ has: name });
}

function getDetailValue(timelinePage: TimelinePage, label: string): Locator {
    return timelinePage.bottomPanel.locator('.sliceDetail').filter({
        has: timelinePage.timelineFrame.getByText(label, { exact: true }),
    }).locator('.sliceDetailMsg');
}

function parseNanoseconds(value: string): number {
    const units: Record<string, number> = { ms: 1000000, us: 1000, ns: 1 };
    const parts = Array.from(value.matchAll(/(-?\d+(?:\.\d+)?)(ms|us|ns)/g));
    expect(parts.length, `Expected a duration in the slice details: ${value}`).toBeGreaterThan(0);
    return parts.reduce((total, part) => total + Number(part[1]) * units[part[2]], 0);
}

async function readCardOffsets(timelinePage: TimelinePage, rank: string, cardId: string,
    expected: Partial<Offsets> = {}): Promise<Offsets> {
    const info = getUnitInfo(timelinePage, rank);
    await info.scrollIntoViewIfNeeded();
    await info.hover();
    const button = info.getByTestId('offset-btn');
    await button.click();
    // Other cards can retain their offset popups, so bind both inputs to this card's full ID.
    const host = timelinePage.timelineFrame.locator(`input[id="${cardId}-host-offset"]`);
    const device = timelinePage.timelineFrame.locator(`input[id="${cardId}-device-offset"]`);
    const tooltip = timelinePage.timelineFrame.locator('.ant-tooltip').filter({ has: host });
    await expect(host).toBeVisible();
    await expect(device).toBeVisible();
    if (expected.host !== undefined) { await expect(host).toHaveValue(String(expected.host)); }
    if (expected.device !== undefined) { await expect(device).toHaveValue(String(expected.device)); }
    const result = { host: Number(await host.inputValue()), device: Number(await device.inputValue()) };
    await button.click();
    // Wait for this popup's leave animation to finish before opening another card's popup.
    await expect(tooltip).toHaveClass(/(?:^|\s)ant-tooltip-hidden(?:\s|$)/);
    await expect(host).toBeHidden();
    return result;
}

async function selectCommunicationOperator(timelinePage: TimelinePage, socket: WebSocket,
    lane: Locator, rank: string): Promise<CommunicationSlice> {
    await lane.scrollIntoViewIfNeeded();
    const chart = lane.locator('canvas.drawCanvas').first();
    await expect(chart).toBeVisible();
    const box = await chart.boundingBox();
    if (box === null) { throw new Error('Communication canvas is missing'); }
    // Search centers this unique operator; the longer rank 2 operator also covers this point.
    const position = { x: box.width / 2, y: 8 };
    await expect.poll(async () => {
        await chart.hover({ position });
        return timelinePage.mainContainer.getByText(communicationOperator, { exact: true }).isVisible();
    }, { message: `Waiting for the rank ${rank} communication operator to be drawn` }).toBe(true);
    const details = observeWebSocketRequests<{ rankId: string; metaType: string; pid: string },
    { data: { rawStartTime: string; title: string; duration: number } }>(socket, 'unit/threadDetail');
    try {
        // Pinned drag containers inherit aria-disabled even though their canvases remain interactive.
        await chart.click({ position, force: true });
        const { params, response } = await details.waitFor(params => params.rankId.endsWith(` ${rank}`) && params.metaType === 'HCCL');
        expect(response.result).toBe(true);
        expect(params.pid).toBe('HCCL');
        expect(response.body.data.title).toBe(communicationOperator);
        await expect(getDetailValue(timelinePage, 'Start(Raw Timestamp)')).toHaveText(`${response.body.data.rawStartTime}ns`);
        await expect(getDetailValue(timelinePage, 'Title')).toHaveText(communicationOperator);
        const duration = parseNanoseconds(await getDetailValue(timelinePage, 'Wall Duration').innerText());
        expect(duration).toBe(response.body.data.duration);
        return {
            rankId: params.rankId,
            rawStartTime: response.body.data.rawStartTime,
            start: parseNanoseconds(await getDetailValue(timelinePage, 'Start').innerText()),
            duration,
            chart,
            position,
        };
    } finally {
        details.dispose();
    }
}

async function preparePinnedCommunication(timelinePage: TimelinePage, socket: WebSocket) {
    await parseClusterCompletedRes;
    await timelinePage.collapseUnit(getUnitInfo(timelinePage, '0'));
    await timelinePage.searchBtn.click();
    const input = timelinePage.timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
    await input.fill(communicationOperator);
    await input.press('Enter');
    await timelinePage.openInWindows.waitFor({ state: 'attached' });
    await timelinePage.searchBtn.click();
    await timelinePage.zoomInBtn.click();
    const selectedLane = timelinePage.unitWrapperScroller.locator('.unit').filter({
        has: timelinePage.timelineFrame.locator('.chart-selected'),
    });
    await expect(selectedLane.locator('.insight-lane-info-name')).toHaveText(communicationLaneName);
    const source = await selectCommunicationOperator(timelinePage, socket, selectedLane, '0');
    await selectedLane.locator('.unit-info').click({ button: 'right' });
    await timelinePage.timelineFrame.getByText(`Pin Same-named Lane (${communicationLaneName})`, { exact: true }).click();
    // This communication group contains rank 0 and rank 2, with one matching operator on each rank.
    const pinned = timelinePage.timelineFrame.locator('#pinnedUnitWrapperScroller');
    await expect(pinned.locator('.unit-info')).toHaveCount(2);
    const sourceLane = getPinnedCommunicationLane(timelinePage, '0');
    const targetLane = getPinnedCommunicationLane(timelinePage, '2');
    await expect(sourceLane).toBeVisible();
    await expect(targetLane).toBeVisible();
    await timelinePage.collapseUnit(getUnitInfo(timelinePage, '0'));
    const target = await selectCommunicationOperator(timelinePage, socket, targetLane, '2');
    const sourceBefore = await readCardOffsets(timelinePage, '0', source.rankId);
    const targetBefore = await readCardOffsets(timelinePage, '2', target.rankId);
    return { sourceLane, targetLane, sourceBefore, targetBefore };
}

async function chooseAlignment(timelinePage: TimelinePage, selected: CommunicationSlice,
    parent: string, direction: string): Promise<void> {
    await selected.chart.click({ button: 'right', position: selected.position, force: true });
    const menu = timelinePage.timelineFrame.getByTitle(parent, { exact: true });
    await expect(menu).toBeVisible();
    await menu.hover();
    const submenu = timelinePage.timelineFrame.locator('.sub-menu-container');
    await expect(submenu.getByTitle('Align Left', { exact: true })).toBeVisible();
    await expect(submenu.getByTitle('Align Right', { exact: true })).toBeVisible();
    await submenu.getByTitle(direction, { exact: true }).click();
    await expect(menu).toBeHidden();
}

function boundary(slice: CommunicationSlice, direction: typeof alignmentDirections[number]): number {
    return slice.start + (direction.alignType === 'RIGHT' ? slice.duration : 0);
}

async function expectCommunicationAlignmentScreenshot(timelinePage: TimelinePage, name: string): Promise<void> {
    await expect(timelinePage.timelineFrame.locator('#pinnedUnitWrapperScroller .unit-info')).toHaveCount(2);
    await timelinePage.page.mouse.move(0, 0);
    await expect(timelinePage.mainContainer).toHaveScreenshot(name, { maxDiffPixels: 100 });
}

let parseClusterCompletedRes: Promise<unknown>;
test.describe('Timeline(DB)', () => {
    test.beforeEach(async ({ page, timelinePage, ws }) => {
        const { timelineFrame } = timelinePage;
        await timelinePage.goto();
        await clearAllData(page);
        const allPagesSuccessRes = waitForResponse(await ws, (res) => res?.event === 'allPagesSuccess');
        parseClusterCompletedRes = waitForResponse(await ws, (res) => res?.event === 'parse/clusterCompleted');
        await importData(page, FilePath.DB);
        await allPagesSuccessRes;
        const secondLayerUnit = timelineFrame.locator('#main-container').getByText('process 3430895');
        await expect(secondLayerUnit).toBeVisible();
    });

    test.afterEach(async ({ page, ws }) => {
        await clearAllData(page, ws);
    });

    // System View - Stats System View 数据展示
    test('test_db_StatsSystemViewDataDisplay_in_SystemView', async ({ page, timelinePage }) => {
        const { bottomPanel, timelineFrame } = timelinePage;
        const systemView = new SystemView(page);
        await systemView.goto();

        const statsSystemViewOptions = [
            'Overall Metrics',
            'Python API Summary',
            'CANN API Summary',
            'Ascend HardWare Task Summary',
            'Communication Summary',
            'Overlap Analysis',
            'Kernel Details',
        ];
        await page.waitForTimeout(2500);
        await expect(bottomPanel).toHaveScreenshot('StatsSystemView-Overall-Metrics.png', { maxDiffPixels: 400 });

        for (const item of statsSystemViewOptions) {
            const option = bottomPanel.getByText(item, { exact: true });
            await option.click();
            await timelineFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
            await expect(bottomPanel).toHaveScreenshot(`StatsSystemView-${item}.png`, { maxDiffPixels: 400 });
        }
    });

    // System View - Expert System View 数据展示
    test('test_db_ExpertSystemViewDataDisplay_in_SystemView', async ({ page, timelinePage }) => {
        const { bottomPanel, timelineFrame } = timelinePage;
        const systemViewPage = new SystemView(page);
        const systemViewSelector = new SelectHelpers(page, systemViewPage.selectSystemView, timelineFrame);
        await systemViewPage.goto();

        await systemViewSelector.open();
        await systemViewSelector.selectOption('Expert System View');

        const expertSystemViewOptions = ['Expert Analysis', 'Affinity API', 'Affinity Optimizer',
            'AICPU Operators', 'ACLNN Operators', 'Operators Fusion'];

        for (const item of expertSystemViewOptions) {
            const option = timelineFrame.getByText(item, { exact: true });
            await option.click();
            await timelineFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
            await expect(bottomPanel).toHaveScreenshot(`ExpertSystemView-${item}.png`, { maxDiffPixels: 400 });
        }
    });

    // 工具栏 - 算子搜索
    test('test_db_operatorSearch_when_EnterOperatorName', async ({ page, timelinePage }) => {
        const { searchBtn, timelineFrame, openInWindows } = timelinePage;
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('CtxGetOverflowAddr');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        // 搜索定位后应正常展示 CANN label 泳道，沿用下方截图检查展示效果。
        const cannLane = timelinePage.unitWrapperScroller.locator('.insight-lane-info-name').filter({ hasText: /^CANN$/ });
        await expect(cannLane).toBeVisible();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-operator.png', { maxDiffPixels: 200 });
    });

    // 工具栏 - 算子搜索在泳道较深的位置时能显示在div中
    test('test_db_deepOperatorSearch_when_EnterOperatorName', async ({ page, timelinePage }) => {
        const { searchBtn, timelineFrame, openInWindows } = timelinePage;
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('NpuSwigluBackward0');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        await page.mouse.move(0, 0);
        await page.waitForTimeout(1000);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('search-deep-operator.png', { maxDiffPixels: 200 });
    });

    // 工具栏 - 算子连线
    test('test_operatorLinkLine', async ({ page, timelinePage }) => {
        const { flowBtn, timelineFrame } = timelinePage;
        await timelineFrame.locator('div:nth-child(4) > .unit-info > .css-rdzxz6 > div > div > .insight-unit-fold').click();
        await page.waitForTimeout(100);
        await timelineFrame.locator('div:nth-child(15) > .unit-info > .css-rdzxz6 > div > div > .insight-unit-fold').click();
        const LinkLineType = [
            'HostToDevice',
            'async_npu',
        ];
        for (const item of LinkLineType) {
            const LinkTypeCheckbox = timelineFrame.getByLabel(item);
            await flowBtn.click();
            await LinkTypeCheckbox.check();
            await flowBtn.click();
            await page.mouse.move(0, 0);
            await page.waitForTimeout(2000);
            await expect(timelineFrame.locator('#main-container')).toHaveScreenshot(`operator-link-line-${item}.png`, { maxDiffPixels: 200 });
            await flowBtn.click();
            await LinkTypeCheckbox.uncheck();
            await flowBtn.click();
        }
    });

    //todo: 工具栏 连线async_task_queue  && fwdbwd


    // 工具栏 - 泳道(card)过滤
    test('test_db_cardFilter', async ({ page, timelinePage }) => {
        const { filterBtn, timelineFrame, selectCardFilterContent } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectCardFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.selectOption('3');
        await filterBtn.click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('card-filter.png', { maxDiffPixels: 200 });
    });

    // 工具栏 - 泳道(unit)过滤
    test('test_db_unitFilter', async ({ page, timelinePage }) => {
        const { filterBtn, timelineFrame, selectUnitFilterContent } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectUnitFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.setValue('Ascend Hardware');
        await filterContentSelector.selectOption('Ascend Hardware');
        await filterBtn.click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('units-filter.png', { maxDiffPixels: 100 });
    });

    // 右键菜单--右键点击通信算子跳转至通信页面
    test('test_db_redirectToCommunication_when_rightClickHCCLOperator', async ({ page, timelinePage }) => {
        await parseClusterCompletedRes;
        test.setTimeout(30_000);
        const { communicationFrame } = new CommunicationPage(page);
        const { filterBtn, timelineFrame, selectUnitFilterContent, unitWrapperScroller,
            searchBtn, openInWindows } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectUnitFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.setValue('Communication');
        await filterContentSelector.selectOption('Communication');
        await filterBtn.click();
        await page.mouse.move(0, 0);
        await unitWrapperScroller.locator('.insight-lane-info-name').getByText('Communication', { exact: true }).first().click();
        await searchBtn.click();
        const inputLocator = timelineFrame.locator('.insight-category-search-overlay').getByPlaceholder('Please enter');
        const input = new InputHelpers(page, inputLocator, timelineFrame);
        await input.setValue('hcom_batchSendRecv__128_4_1');
        await input.press('Enter');
        await openInWindows.waitFor({ state: 'attached' });
        const selectedCanvas = timelineFrame.locator('.chart-selected canvas.drawCanvas').first();
        await selectedCanvas.waitFor({ state: 'visible' });
        await searchBtn.click();
        await page.waitForTimeout(2000);

        await selectedCanvas.click({
            button: 'right',
            position: {
                x: 254,
                y: 5,
            },
            force: true,
        });
        await timelineFrame.getByText('Find in Communication').click({ force: true });
        const hcclChart = communicationFrame.locator('.panel-content').first();
        await hcclChart.waitFor({ state: 'visible' });

        await expect(hcclChart).toHaveScreenshot('redirect-to-communication.png', { maxDiffPixels: 100 });
    });

    // 同通信域泳道置顶
    test('test_db_same_communication_group', async ({ page, timelinePage }) => {
        const { filterBtn, timelineFrame, selectUnitFilterContent } = timelinePage;
        const filterContentSelector = new SelectHelpers(page, selectUnitFilterContent, timelineFrame);

        await filterBtn.click();
        await filterContentSelector.open();
        await filterContentSelector.setValue('Communication');
        await filterContentSelector.selectOption('Communication');
        await filterBtn.click();
        await page.waitForTimeout(3000);
        await timelineFrame.getByText('mp:Group group_name_13 Communication').click({
            button: 'right',
        });
        await timelineFrame.getByText('Pin Same-named Lane (mp:Group group_name_13 Communication)').click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('communication_group_pin.png', { maxDiffPixels: 100 });
        await timelineFrame.locator('#pinnedUnitWrapperScroller').getByText('mp:Group group_name_13 Communication_Communication (HCCL)_localhost.localdomain2152938157304401006_0 0').click({
            button: 'right',
        });
        await timelineFrame.getByText('Unpin Same-named Lane').click();
        await page.mouse.move(0, 0);
        await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('communication_group_unpin.png', { maxDiffPixels: 100 });
    });

    // 右键菜单--Show in Events View
    test('test_db_context_menu_click_ShowInEventsView', async ({ timelinePage }) => {
        const { timelineFrame, bottomPanel } = timelinePage;
        await timelineFrame.locator('#unitWrapperScroller').getByText('Ascend Hardware').click();
        await timelineFrame.getByText('Stream 2').click({
            button: 'right',
        });
        await timelineFrame.getByText('Show in Events View').click();
        await timelineFrame.locator('.ant-spin').waitFor({ state: 'hidden' });
        await expect(bottomPanel).toHaveScreenshot('test-db-click-ShowInEventsView.png', { maxDiffPixels: 400 });
    });

    for (const direction of alignmentDirections) {
        // DB Communication：先置顶同名泳道，再在置顶区执行跨卡时间对齐。
        test(`test_db_operator_time_alignment_${direction.name}`, async ({ timelinePage, ws }) => {
            const socket = await ws;
            const { sourceLane, targetLane, sourceBefore, targetBefore } = await preparePinnedCommunication(timelinePage, socket);
            const source = await selectCommunicationOperator(timelinePage, socket, sourceLane, '0');
            const target = await selectCommunicationOperator(timelinePage, socket, targetLane, '2');
            expect(source.rankId).not.toBe(target.rankId);
            expect(source.duration).not.toBe(target.duration);
            expect(boundary(target, direction)).not.toBe(boundary(source, direction));
            const selected = await selectCommunicationOperator(timelinePage, socket, sourceLane, '0');
            const alignments = observeWebSocketRequests<AlignmentParams, AlignmentResponse>(socket, 'timeline/rankOffset');
            try {
                await chooseAlignment(timelinePage, selected, 'Time Alignment', direction.menu);
                const { params, response } = await alignments.waitFor(params => params.rankId === source.rankId);
                expect(params).toMatchObject({
                    alignType: direction.alignType,
                    sliceName: communicationOperator,
                    startTime: source.rawStartTime,
                    metaType: 'HCCL',
                    pid: 'HCCL',
                });
                expect(response.result).toBe(true);
                const alignedTarget = response.body.result.find(item => item.rankId === target.rankId);
                expect(alignedTarget, 'The matching communication operator must be found on rank 2').toBeDefined();
                if (alignedTarget === undefined) { throw new Error('Missing rank 2 alignment result'); }
                const expectedDevice = alignedTarget.offset + sourceBefore.device - response.body.baseOffset;
                expect(expectedDevice).not.toBe(targetBefore.device);
                await readCardOffsets(timelinePage, '2', target.rankId, { device: expectedDevice, host: targetBefore.host });
                expect(await readCardOffsets(timelinePage, '0', source.rankId)).toEqual(sourceBefore);
                const sourceAfter = await selectCommunicationOperator(timelinePage, socket, sourceLane, '0');
                const targetAfter = await selectCommunicationOperator(timelinePage, socket, targetLane, '2');
                expect(sourceAfter).toMatchObject({
                    rankId: source.rankId, rawStartTime: source.rawStartTime, start: source.start, duration: source.duration,
                });
                expect(targetAfter).toMatchObject({
                    rankId: target.rankId, rawStartTime: target.rawStartTime, duration: target.duration,
                });
                expect(boundary(targetAfter, direction)).toBe(boundary(sourceAfter, direction));
                await expectCommunicationAlignmentScreenshot(timelinePage, `test-db-operator-time-alignment-${direction.name}.png`);
            } finally {
                alignments.dispose();
            }
        });

        // 同一通信域内跨卡设基准，验证 Device 偏移及左右边界，Host 偏移保持不变。
        test(`test_db_set_base_slice_and_align_${direction.name}`, async ({ timelinePage, ws }) => {
            const socket = await ws;
            const { sourceLane, targetLane, sourceBefore, targetBefore } = await preparePinnedCommunication(timelinePage, socket);
            const source = await selectCommunicationOperator(timelinePage, socket, sourceLane, '0');
            await source.chart.click({ button: 'right', position: source.position, force: true });
            await timelinePage.timelineFrame.getByTitle('Set base slice', { exact: true }).click();
            await source.chart.click({ button: 'right', position: source.position, force: true });
            await expect(timelinePage.timelineFrame.getByTitle('Clear base slice', { exact: true })).toBeVisible();
            await expect(timelinePage.timelineFrame.getByTitle('Set base slice', { exact: true })).toHaveCount(0);
            await timelinePage.mainContainer.focus();
            const target = await selectCommunicationOperator(timelinePage, socket, targetLane, '2');
            expect(source.rankId).not.toBe(target.rankId);
            expect(source.duration).not.toBe(target.duration);
            const delta = boundary(target, direction) - boundary(source, direction);
            expect(delta, 'The two communication operators must have different boundaries before alignment').not.toBe(0);
            await chooseAlignment(timelinePage, target, 'Align to Base Slice', direction.menu);
            await readCardOffsets(timelinePage, '2', target.rankId, { device: targetBefore.device + delta, host: targetBefore.host });
            expect(await readCardOffsets(timelinePage, '0', source.rankId)).toEqual(sourceBefore);
            const sourceAfter = await selectCommunicationOperator(timelinePage, socket, sourceLane, '0');
            const targetAfter = await selectCommunicationOperator(timelinePage, socket, targetLane, '2');
            expect(sourceAfter).toMatchObject({
                rankId: source.rankId, rawStartTime: source.rawStartTime, start: source.start, duration: source.duration,
            });
            expect(targetAfter).toMatchObject({
                rankId: target.rankId, rawStartTime: target.rawStartTime, duration: target.duration,
            });
            expect(boundary(targetAfter, direction)).toBe(boundary(sourceAfter, direction));
            await expectCommunicationAlignmentScreenshot(timelinePage, `test-db-base-slice-alignment-${direction.name}.png`);

            await targetAfter.chart.click({ button: 'right', position: targetAfter.position, force: true });
            await timelinePage.timelineFrame.getByTitle('Clear base slice', { exact: true }).click();
            await targetAfter.chart.click({ button: 'right', position: targetAfter.position, force: true });
            await expect(timelinePage.timelineFrame.getByTitle('Clear base slice', { exact: true })).toHaveCount(0);
            await expect(timelinePage.timelineFrame.getByTitle('Align to Base Slice', { exact: true })).toHaveCount(0);
            await expect(timelinePage.timelineFrame.getByTitle('Time Alignment', { exact: true })).toBeVisible();
        });
    }
});
