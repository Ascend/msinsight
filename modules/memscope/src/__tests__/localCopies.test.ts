/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { createMemorySession as createScopeSession } from '../common/entity/createMemorySession';
import { createMemorySession as createSnapshotSession } from '../../../memsnapshot/src/common/entity/createMemorySession';
import { createRendererRegistry as scopeRegistry } from '../common/shared/workers/RendererRegistry';
import { createRendererRegistry as snapshotRegistry } from '../../../memsnapshot/src/common/shared/workers/RendererRegistry';

it('keeps copied application defaults and marker state independent', () => {
    const scope = createScopeSession({}, 'leaks');
    const snapshot = createSnapshotSession({}, 'memsnapshot');
    scope.leaksWorkerInfo.renderOptions.transform.x = 42;
    scope.addLifecycleMemoryMarker(1024, 'scope-only');
    expect(snapshot.leaksWorkerInfo.renderOptions.transform.x).not.toBe(42);
    expect(snapshot.getLifecycleMemoryMarkers()).toEqual([]);
});

it('releases only the owning application resource even for the same compatibility owner', () => {
    const owner = { module: 'leaks' };
    const scopeDispose = jest.fn();
    const snapshotDispose = jest.fn();
    const scope = scopeRegistry(() => ({ dispose: scopeDispose }));
    const snapshot = snapshotRegistry(() => ({ dispose: snapshotDispose }));
    scope.get(owner);
    const snapshotResource = snapshot.get(owner);
    scope.disposeAll();
    expect(scopeDispose).toHaveBeenCalledTimes(1);
    expect(snapshotDispose).not.toHaveBeenCalled();
    expect(snapshot.get(owner)).toBe(snapshotResource);
    snapshot.disposeAll();
    expect(snapshotDispose).toHaveBeenCalledTimes(1);
});
