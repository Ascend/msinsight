/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { getMemSnapshotBootstrapState } from './memSnapshotBootstrap';

const fileId = 'C:/data/snapshot.pickle';
const makeHost = (): Parameters<typeof getMemSnapshotBootstrapState>[0] => ({
    activeDataSource: { selectedFilePath: fileId },
    module: '',
    fileHash: '',
    dbPath: '',
    deviceIds: {},
    memSnapshotParseFileId: '',
    memSnapshotParseLoading: false,
    memSnapshotParseProgress: 0,
    snapshotParsingComplete: true,
    snapshotSlices: {},
} as Parameters<typeof getMemSnapshotBootstrapState>[0]);
const readySlices = { 0: { eventCount: 1, sliceCount: 1, readySlices: [0], slices: [{ index: 0, startEventId: 0, endEventId: 0, ready: true }] } };

it('initializes parsing before the first backend progress or module update', () => {
    expect(getMemSnapshotBootstrapState(makeHost())).toEqual(expect.objectContaining({
        module: 'memsnapshot',
        dbPath: fileId,
        memSnapshotParseFileId: fileId,
        memSnapshotParseLoading: true,
        snapshotParsingComplete: false,
        memSnapshotParseProgress: 0,
    }));
});
it('restores a completed snapshot coherently after a cold iframe mount', () => {
    const host = { ...makeHost(), module: 'memsnapshot', dbPath: 'c:\\data\\snapshot.pickle', fileHash: 'ready', deviceIds: { 0: ['BLOCK'] }, snapshotSlices: readySlices };
    expect(getMemSnapshotBootstrapState(host)).toEqual(expect.objectContaining({
        fileHash: 'ready',
        deviceIds: host.deviceIds,
        snapshotSlices: readySlices,
        snapshotParsingComplete: true,
        memSnapshotParseLoading: false,
        memSnapshotParseProgress: 100,
    }));
});
it('retains progress received before the iframe exists', () => {
    const host = { ...makeHost(), memSnapshotParseFileId: fileId, memSnapshotParseLoading: true, memSnapshotParseProgress: 37 };
    expect(getMemSnapshotBootstrapState(host)).toEqual(expect.objectContaining({ memSnapshotParseLoading: true, memSnapshotParseProgress: 37 }));
});
it('does not restore hash or ready slices belonging to a previous snapshot', () => {
    const host = { ...makeHost(), module: 'memsnapshot', dbPath: 'C:/old.pickle', fileHash: 'old', deviceIds: { 0: ['BLOCK'] }, snapshotSlices: readySlices };
    expect(getMemSnapshotBootstrapState(host)).toEqual(expect.objectContaining({ fileHash: '', deviceIds: {}, snapshotSlices: {}, memSnapshotParseLoading: true }));
});
