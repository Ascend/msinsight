/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import type { Session } from './entity/session';
import { normalizeFilePath } from './utils/filePath';

type SnapshotHostState = Pick<Session, 'activeDataSource' | 'module' | 'fileHash' | 'dbPath' | 'deviceIds' |
'memSnapshotParseFileId' | 'memSnapshotParseLoading' | 'memSnapshotParseProgress' | 'snapshotParsingComplete' | 'snapshotSlices'>;

// Replay a coherent state after the directory has initialized the application.
export const getMemSnapshotBootstrapState = (session: SnapshotHostState, fileId = session.activeDataSource.selectedFilePath): Record<string, unknown> => {
    const sameFile = session.module === 'memsnapshot' && fileId !== '' &&
        normalizeFilePath(session.dbPath) === normalizeFilePath(fileId);
    const sameProgress = fileId !== '' && normalizeFilePath(session.memSnapshotParseFileId) === normalizeFilePath(fileId);
    const snapshotSlices = sameFile ? session.snapshotSlices : {};
    const devices = Object.values(snapshotSlices);
    const complete = sameFile && session.snapshotParsingComplete;
    const ready = complete && devices.length > 0 && devices.every(device =>
        device.slices.length === device.sliceCount && device.slices.every(slice => slice.ready));
    return {
        module: 'memsnapshot',
        dbPath: fileId,
        fileHash: sameFile ? session.fileHash : '',
        deviceIds: sameFile ? session.deviceIds : {},
        memSnapshotParseFileId: fileId,
        memSnapshotParseLoading: !ready || (sameProgress && session.memSnapshotParseLoading),
        memSnapshotParseProgress: ready ? 100 : sameProgress ? session.memSnapshotParseProgress : 0,
        snapshotParsingComplete: complete,
        snapshotSlices,
    };
};
