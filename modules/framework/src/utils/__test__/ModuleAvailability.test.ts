/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import type { ModuleConfig } from '../../moduleConfig';
import { updateSession } from '@/connection/notificationHandler';
import { getAvailableModules, updateDataScene } from '../../components/TabPane/Index';

jest.mock('@/moduleConfig', () => jest.requireActual('../../moduleConfig'), { virtual: true });
jest.mock('@/utils/enum', () => ({ SessionAction: {} }), { virtual: true });
jest.mock('@/utils/Request', () => ({ getModuleConfig: jest.fn() }), { virtual: true });
jest.mock('@/connection/notificationHandler', () => ({ updateSession: jest.fn() }), { virtual: true });
jest.mock('@/connection', () => ({ __esModule: true, default: { send: jest.fn() } }), { virtual: true });
jest.mock('@/vscode-adapter', () => ({
    onDivLoad: jest.fn(), isVscodePluginEnvironment: jest.fn(() => false), isVscodeEnv: jest.fn(() => false),
}), { virtual: true });
jest.mock('@insight/lib/utils', () => ({ safeJSONParse: jest.fn() }), { virtual: true });

const config = (name: string, flags: Partial<ModuleConfig> = {}): ModuleConfig => ({
    name, requestName: 'timeline', attributes: {}, ...flags,
});
const modules = [
    config('Timeline', { isDefault: true }),
    config('NUMA', { hasNumaData: true }),
    config('PluginA', { isDefault: true }),
    config('PluginB', { isDefault: true }),
];

describe('NUMA module availability', () => {
    it.each([
        [{ hasNumaData: false }, ['Timeline', 'PluginA', 'PluginB']],
        [{ hasNumaData: true, isFullDb: false }, ['Timeline', 'PluginA', 'PluginB', 'NUMA']],
    ])('filters and orders modules for session %o', (session, expected) => {
        expect(getAvailableModules(modules, 'Default', session).map(item => item.name)).toEqual(expected);
    });

    it.each([[{ hasNumaData: true }, true], [{}, false]])(
        'publishes NUMA availability from import data', (result, hasNumaData) => {
            updateDataScene(result);
            expect(updateSession).toHaveBeenCalledWith(expect.objectContaining({ hasNumaData }));
        },
    );
});
