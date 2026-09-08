import i18n from '@insight/lib/i18n';
import type { NumaStore } from '@/features/overview/model/NumaStore';
import type { NotificationHandler } from './types';

export type NumaNotificationHandlers = Record<string, NotificationHandler>;

export function createNumaNotificationHandlers(store: NumaStore): NumaNotificationHandlers {
    const reset: NotificationHandler = () => {
        store.reset();
    };

    return {
        switchDirectory: (body) => store.switchDirectory({
            rankId: String(body.rankId ?? ''),
            selectedFilePath: String(body.selectedFilePath ?? ''),
        }),
        setTheme: (body) => {
            const isDark = Boolean(body.isDark);
            document.documentElement.dataset.theme = isDark ? 'dark' : 'light';
            window.setTheme(isDark);
        },
        switchLanguage: (body) => {
            store.setLocale(body.lang);
            void i18n.changeLanguage(store.locale);
        },
        updateSession: (body) => {
            if (!Object.prototype.hasOwnProperty.call(body, 'timeAnalysisRange')) return;
            return store.setTimeAnalysisRange(body.timeAnalysisRange);
        },
        moduleActive: () => store.activate(),
        'remote/remove': reset,
        'remote/reset': reset,
    };
}
