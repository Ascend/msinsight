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
import { autorun, isObservable } from 'mobx';
import { CommunicationStore } from '../store/CommunicationStore';
import type { Group } from '../domain/types';

const groups: Group[] = [
    { key: 'dp', groupIdHash: 'hash-dp', pgName: 'dp', stage: '(0, 1)', label: 'dp:(0,1)' },
    { key: 'tp', groupIdHash: 'hash-tp', pgName: 'tp', stage: '(0, 1)', label: 'tp:(0,1)' },
];
function setup(): CommunicationStore {
    const store = new CommunicationStore();
    store.updateHost({ project: 'p', clusterPath: 'c', clusters: [{ path: 'c', name: 'c', parsed: true, durationParsed: false }] });
    store.resolveIteration(store.generation, ['1', '2']);
    store.resolveGroup(store.generation, '1', groups);
    return store;
}
it('isolates page instances and resets all selections on cluster or project changes', () => {
    const store = setup();
    const other = setup();
    store.selectGroup(groups[1]);
    expect(other.group).toBe(groups[0]);
    const generation = store.generation;
    store.selectCluster('next');
    expect(store.generation).toBe(generation + 1);
    expect(store.iterationId).toBe('');
    expect(store.group).toBeUndefined();
    expect(store.scope).toBeUndefined();
    store.selectIteration('2');
    store.selectGroup(groups[1]);
    store.updateHost({ project: 'other-project' });
    expect(store.scope).toBeUndefined();
    expect(other.scope).toMatchObject({ clusterPath: 'c', iterationId: '1', group: groups[0] });
});
it('resets the group on iteration change and rejects stale selection results', () => {
    const store = setup();
    store.selectIteration('2');
    expect(store.group).toBeUndefined();
    store.resolveGroup(store.generation, '1', groups);
    expect(store.group).toBeUndefined();
    store.resolveGroup(store.generation, '2', groups);
    expect(store.group).toBe(groups[0]);
    const generation = store.generation;
    store.selectCluster('next');
    store.resolveIteration(generation, ['1']);
    store.resolveGroup(generation, '1', groups);
    expect(store.iterationId).toBe('');
    expect(store.group).toBeUndefined();
});
it('resolves external locations after options arrive and lets user selections override them', () => {
    const store = new CommunicationStore();
    store.applyLocation({ iterationId: '2', stage: '(0, 1)', pgName: 'tp', operatorName: 'op' });
    store.updateHost({ project: 'p', clusterPath: 'c' });
    store.resolveIteration(store.generation, ['1', '2']);
    store.resolveGroup(store.generation, '2', groups);
    expect(store.scope).toMatchObject({ iterationId: '2', group: groups[1] });
    expect(store.locationIntent?.operatorName).toBe('op');
    store.selectGroup(groups[0]);
    expect(store.locationIntent).toBeUndefined();
    store.resolveGroup(store.generation, '2', groups);
    expect(store.group).toBe(groups[0]);
    store.applyLocation({ groupIdHash: 'hash-tp' });
    store.resolveGroup(store.generation, '2', groups);
    expect(store.group).toBe(groups[1]);
    store.selectIteration('1');
    expect(store.locationIntent).toBeUndefined();
    expect(store.group).toBeUndefined();
});
it('preserves filters and shallow references during theme, language and parsing updates', () => {
    const store = setup();
    const generation = store.generation;
    const scope = store.scope;
    store.updateHost({ dark: false, language: 'zh', unitCount: 10 });
    store.updateHost({ clusters: [{ path: 'c', name: 'c', parsed: true, durationParsed: true }] });
    expect(store.generation).toBe(generation);
    expect(store.scope).toEqual(scope);
    expect(store.durationReady).toBe(true);
    expect(isObservable(store.clusters)).toBe(false);
    expect(isObservable(store.group)).toBe(false);
});
it('notifies only observers of changed fields and releases subscriptions', () => {
    const store = setup();
    const selections = jest.fn();
    const appearance = jest.fn();
    const stopSelections = autorun(() => selections(store.iterationId, store.group));
    const stopAppearance = autorun(() => appearance(store.dark));
    try {
        store.updateHost({ dark: false });
        expect(selections).toHaveBeenCalledTimes(1);
        expect(appearance).toHaveBeenCalledTimes(2);
        store.selectIteration('2');
        expect(selections).toHaveBeenCalledTimes(2);
        expect(appearance).toHaveBeenCalledTimes(2);
    } finally { stopSelections(); stopAppearance(); }
    store.resetHost();
    expect(selections).toHaveBeenCalledTimes(2);
});
it('clears pending navigation on reset while retaining theme and language', () => {
    const store = setup();
    store.updateHost({ dark: false, language: 'zh' });
    store.applyLocation({ iterationId: '2', operatorName: 'op' });
    const generation = store.generation;
    store.resetHost();
    expect(store).toMatchObject({ generation: generation + 1, project: '', clusterPath: '', dark: false, language: 'zh', iterationId: '' });
    expect(store.locationIntent).toBeUndefined();
    expect(store.scope).toBeUndefined();
});
