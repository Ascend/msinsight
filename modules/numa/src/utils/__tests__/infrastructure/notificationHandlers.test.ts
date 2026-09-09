/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

import i18n from '@insight/lib/i18n';
import type { NumaOverview } from '@/entities/numa/types';
import { NumaStore } from '@/features/overview/model/NumaStore';
import { createNumaNotificationHandlers } from '@/infrastructure/connection/notificationHandlers';

jest.mock('@insight/lib/i18n', () => ({
    __esModule: true,
    default: { changeLanguage: jest.fn() },
}), { virtual: true });

const emptyOverview: NumaOverview = {
    range: { startTime: 0, endTime: 0 }, totalMetrics: [], sockets: [], connections: [],
};

describe('NUMA notification handlers', () => {
    it('maps Framework notifications to Store behavior', async () => {
        const store = new NumaStore(jest.fn(async () => emptyOverview));
        const handlers = createNumaNotificationHandlers(store);

        await handlers.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });
        expect(store.directory).toEqual({ rankId: 'rank-0', selectedFilePath: 'db-path' });
        handlers.switchLanguage({ lang: 'enUS' });
        expect(store.locale).toBe('enUS');
        expect(i18n.changeLanguage).toHaveBeenCalledWith('enUS');
        await handlers.updateSession({ timeAnalysisRange: [10.2, 20.1] });
        expect(store.analysisRange).toEqual({ startTime: 10, endTime: 21 });
        handlers['remote/reset']({});
        expect(store.data).toBeNull();
        expect(store.analysisRange).toEqual({ startTime: 0, endTime: 0 });
    });

    it('applies Framework theme state to the page', () => {
        const handlers = createNumaNotificationHandlers(new NumaStore(async () => emptyOverview));
        const originalSetTheme = window.setTheme;
        window.setTheme = jest.fn();
        try {
            handlers.setTheme({ isDark: true });
            expect(document.documentElement.dataset.theme).toBe('dark');
            expect(window.setTheme).toHaveBeenCalledWith(true);
        } finally {
            window.setTheme = originalSetTheme;
        }
    });
});
