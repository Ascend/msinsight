/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { createRendererRegistry, getRendererSessionKey } from '../common/shared/workers/RendererRegistry';

describe('renderer owner leases', () => {
    it('keeps StrictMode cleanup/setup on the same resource and disposes actual unmount once', async () => {
        const create = jest.fn(() => ({ dispose: jest.fn() }));
        const registry = createRendererRegistry(create);
        const owner = { module: 'leaks' };
        const release = registry.retain(owner);
        const resource = registry.get(owner);
        release();
        const releaseAgain = registry.retain(owner);
        await Promise.resolve();
        expect(registry.get(owner)).toBe(resource);
        expect(resource.dispose).not.toHaveBeenCalled();
        releaseAgain();
        releaseAgain();
        await Promise.resolve();
        expect(resource.dispose).toHaveBeenCalledTimes(1);
        const remounted = registry.get(owner);
        expect(remounted).not.toBe(resource);
        registry.retain(owner);
        expect(registry.get(owner)).toBe(remounted);
        expect(create).toHaveBeenCalledTimes(2);
        registry.disposeAll();
    });

    it('isolates feature switching on the same session and separate sessions', async () => {
        const registry = createRendererRegistry(() => ({ dispose: jest.fn() }));
        const first = { module: 'leaks' };
        const second = { module: 'leaks' };
        const release = registry.retain(first);
        const scope = registry.get(first);
        registry.retain(second);
        const other = registry.get(second);
        first.module = 'memsnapshot';
        registry.retain(first);
        const snapshot = registry.get(first);
        release();
        await Promise.resolve();
        expect(scope.dispose).toHaveBeenCalledTimes(1);
        expect(snapshot.dispose).not.toHaveBeenCalled();
        expect(other.dispose).not.toHaveBeenCalled();
        registry.disposeAll();
        expect(snapshot.dispose).toHaveBeenCalledTimes(1);
        expect(other.dispose).toHaveBeenCalledTimes(1);
    });

    it('does not create resources on peek and distinguishes canvas session keys', () => {
        const create = jest.fn(() => ({ dispose: jest.fn() }));
        const registry = createRendererRegistry(create);
        const first = { module: 'leaks' };
        const second = { module: 'leaks' };
        expect(registry.peek(first)).toBeUndefined();
        expect(create).not.toHaveBeenCalled();
        expect(getRendererSessionKey(first)).toBe(getRendererSessionKey(first));
        expect(getRendererSessionKey(first)).not.toBe(getRendererSessionKey(second));
    });
});
