/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
// eslint-disable-next-line import/named -- waitFor is re-exported from @testing-library/dom.
import { fireEvent, render, waitFor } from '@testing-library/react';
import { createInstance } from 'i18next';
import { initReactI18next } from 'react-i18next';
import { message } from 'antd';
import { BaseSummary } from '../SystemView';
import { affinityAPIColumns, exportAffinityAPI } from '../Common';

const resources = {
    zh: { timeline: jest.requireActual('../../../../../lib/src/i18n/timeline/zh.json').timeline },
    en: { timeline: jest.requireActual('../../../../../lib/src/i18n/timeline/en.json').timeline },
};
let translations: ReturnType<typeof createInstance>;

jest.mock('@insight/lib/resize', () => ({
    ResizeTable: (): JSX.Element => <table/>,
    fetchColumnFilterProps: (): Record<string, unknown> => ({}),
}));
jest.mock('../ExpertSystemView', () => ({ ExpertSystemView: [], handleAdvisorSelected: jest.fn() }));
jest.mock('../EventsView', () => ({ EventView: (): null => null }));
jest.mock('../TableDataView', () => ({ TableDataView: (): null => null }));
jest.mock('../StatsSystemView', () => ({ StatsSystemView: [] }));
// 提示测试无需加载整个画布链路，按钮、翻译及BaseSummary导出处理保持真实。
jest.mock('../../../insight/units/AscendUnit', () => ({ getDetailTimeDisplay: (value: number): string => String(value) }));
jest.mock('../../../insight/units/utils', () => ({ getTimeOffset: (): number => 0 }));

const session: any = {
    units: [{ metadata: { cardId: '0' }, phase: 'download' }],
    timeAnalysisRange: [0, 1000],
    language: 'en',
    selectedUnits: [],
    rankCardInfoMap: new Map(),
};
const renderSummary = (exportRequest?: jest.Mock): ReturnType<typeof render> => render(
    <BaseSummary key={0} card={{ cardId: '0', dbPath: 'demo.db' }} session={session} bottomHeight={400}
        columns={affinityAPIColumns} request={jest.fn().mockResolvedValue({ data: [], count: 0 })} exportRequest={exportRequest} />,
);
// Ant Design会在两个汉字按钮中插入空格，匹配时只忽略这个排版空白。
const exportName = (name: string): boolean => name.replace(/\s/g, '') === translations.t('Export');
const button = (view: ReturnType<typeof render>): HTMLElement => view.getByRole('button', { name: exportName });

describe.each(['zh', 'en'] as const)('Affinity API export feedback (%s)', (language) => {
    let success: jest.SpyInstance;
    let failure: jest.SpyInstance;
    const filePath = 'C:\\性能数据\\demo & test\\advisor_affinity_api_0.csv';

    beforeEach(async () => {
        translations = createInstance();
        await translations.use(initReactI18next).init({ lng: language, resources, defaultNS: 'timeline', interpolation: { escapeValue: false } });
        success = jest.spyOn(message, 'success').mockImplementation((() => () => {}) as any);
        failure = jest.spyOn(message, 'error').mockImplementation((() => () => {}) as any);
    });
    afterEach(() => { jest.restoreAllMocks(); });

    it('localizes the real button and success path without changing the export request', async () => {
        const exportRequest = jest.fn().mockResolvedValue({ filePath, exceedingFileLimit: false });
        const view = renderSummary(exportRequest);
        expect(button(view).textContent?.replace(/\s/g, '')).toBe(language === 'zh' ? '导出' : 'Export');
        fireEvent.click(button(view));
        await waitFor(() => expect(success).toHaveBeenCalledTimes(1));
        expect(success).toHaveBeenCalledWith({
            content: language === 'zh' ? `导出成功，文件已保存至：${filePath}` : `Export succeeded. File saved to: ${filePath}`,
        });
        expect(exportRequest).toHaveBeenCalledTimes(1);
        expect(exportRequest).toHaveBeenCalledWith({ rankId: '0', orderBy: 'duration', order: 'descend', startTime: 0, endTime: 1000 });
        expect(failure).not.toHaveBeenCalled();
        await waitFor(() => expect(button(view).querySelector('.anticon-loading')).toBeNull());
    });

    it('includes only the reported size warning and returned path', async () => {
        const view = renderSummary(jest.fn().mockResolvedValue({ filePath, exceedingFileLimit: true }));
        fireEvent.click(button(view));
        await waitFor(() => expect(success).toHaveBeenCalledTimes(1));
        expect(success.mock.calls[0][0].content).toBe(`${translations.t('Export Success', { filePath })} ${translations.t('Export File Exceeds Limit')}`);
        expect(failure).not.toHaveBeenCalled();
    });

    it.each(['', undefined, 42])('reports failure rather than success for invalid path %s', async (path) => {
        const view = renderSummary(jest.fn().mockResolvedValue({ filePath: path }));
        fireEvent.click(button(view));
        await waitFor(() => expect(failure).toHaveBeenCalledWith(translations.t('Export Failed')));
        expect(failure).toHaveBeenCalledTimes(1);
        expect(success).not.toHaveBeenCalled();
        await waitFor(() => expect(button(view).querySelector('.anticon-loading')).toBeNull());
    });

    it('releases the loading state after failure so the same button can export again', async () => {
        jest.spyOn(console, 'error').mockImplementation(() => {});
        const exportRequest = jest.fn().mockRejectedValueOnce(new Error('network')).mockResolvedValueOnce({ filePath });
        const view = renderSummary(exportRequest);
        fireEvent.click(button(view));
        await waitFor(() => expect(failure).toHaveBeenCalledWith(translations.t('Export Failed')));
        await waitFor(() => expect(button(view).querySelector('.anticon-loading')).toBeNull());
        fireEvent.click(button(view));
        await waitFor(() => expect(success).toHaveBeenCalledTimes(1));
        expect(exportRequest).toHaveBeenCalledTimes(2);
        expect(failure).toHaveBeenCalledTimes(1);
    });

    it('does not add an export entry or feedback to views without exportRequest', async () => {
        const view = renderSummary();
        await waitFor(() => expect(view.container.querySelector('table')).toBeInTheDocument());
        expect(view.queryByRole('button', { name: exportName })).toBeNull();
        expect(success).not.toHaveBeenCalled();
        expect(failure).not.toHaveBeenCalled();
    });
});

it('keeps the affinity export protocol and result while delegating failure feedback to the page', async () => {
    const original = window.requestData;
    const result = { filePath: '/data/advisor_affinity_api.csv', exceedingFileLimit: false };
    const request = jest.fn().mockResolvedValueOnce(result).mockRejectedValueOnce(new Error('export failed'));
    window.requestData = request;
    const params = { rankId: '0', orderBy: 'duration', order: 'descend', startTime: 0, endTime: 1000 };
    try {
        await expect(exportAffinityAPI(params)).resolves.toBe(result);
        await expect(exportAffinityAPI(params)).rejects.toThrow('export failed');
        expect(request).toHaveBeenCalledTimes(2);
        expect(request).toHaveBeenLastCalledWith('advisor/exportAffinityAPI', params, 'advisor', { silent: true });
    } finally {
        window.requestData = original;
    }
});
