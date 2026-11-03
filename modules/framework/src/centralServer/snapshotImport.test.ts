/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { addDataPath, CONNECTION_MAP } from './server';
import { GLOBAL_HOST } from './websocket/defs';
import type { Project } from './websocket/defs';
import type { Session } from '../entity/session';
import { importProject } from '@/utils/Request';
import { updateProject } from '@/utils/Project';
import { updateSession } from '@/connection/notificationHandler';
import connector from '@/connection';
import { ProjectAction } from '@/utils/enum';

jest.mock('./websocket/connection', () => ({ Connection: jest.fn() }));
jest.mock('@/connection', () => ({ __esModule: true, default: { send: jest.fn() } }));
jest.mock('@/utils/Request', () => ({ importProject: jest.fn() }));
jest.mock('@/utils/Rank', () => ({ updateRankMap: jest.fn() }));
jest.mock('@/utils/Project', () => ({ transformFile: jest.fn(), updateProject: jest.fn() }));
jest.mock('@/utils/useLoading', () => ({ closeLoading: jest.fn() }));
jest.mock('@/components/TabPane/Index', () => ({ updateDataScene: jest.fn() }));
jest.mock('@/connection/notificationHandler', () => ({ updateSession: jest.fn() }));
jest.mock('@/entity/session', () => ({ Session: jest.fn() }));
jest.mock('@insight/lib', () => ({ errorCenter: { handleError: jest.fn() }, WsError: Error }));
jest.mock('@/vscode-adapter', () => ({ isVscodeEnv: () => false }));

it.each([true, false])('restores early snapshot completion only for the selected file: %s', async matching => {
    const fileId = '/first.pickle';
    const selectedFilePath = matching ? fileId : '/another.pickle';
    const session = {
        module: 'memsnapshot',
        dbPath: fileId,
        fileHash: 'early-hash',
        deviceIds: { 0: ['BLOCK'] },
        memSnapshotParseFileId: fileId,
        memSnapshotParseLoading: false,
        memSnapshotParseProgress: 100,
        snapshotParsingComplete: true,
        snapshotSlices: { 0: { sliceCount: 1, readySlices: [0], slices: [{ index: 0, ready: true }] } },
        activeDataSource: { selectedFilePath: '', projectName: '' },
        actionListener: {},
        resetActionListener: jest.fn(),
        reset: jest.fn(function (this: any) { this.fileHash = ''; this.deviceIds = {}; this.snapshotSlices = {}; this.memSnapshotParseFileId = ''; }),
    } as unknown as Session;
    (importProject as jest.Mock).mockResolvedValue({ body: { reset: true, isLeaks: true, children: [{ type: 'RANK', filePath: selectedFilePath }] } });
    (updateProject as jest.Mock).mockImplementation(() => { session.activeDataSource.selectedFilePath = selectedFilePath; });
    (updateSession as jest.Mock).mockImplementation(data => Object.assign(session, data));
    CONNECTION_MAP.set(`${GLOBAL_HOST.remote}:${GLOBAL_HOST.port}`, {} as any);
    await addDataPath({ projectName: 'first', projectPath: [selectedFilePath] } as Project, ProjectAction.ADD_FILE, false, session);
    expect(session.reset).toHaveBeenCalled();
    expect(session.fileHash).toBe(matching ? 'early-hash' : '');
    if (matching) expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ to: 'MemSnapshot', body: expect.objectContaining({ fileHash: 'early-hash', memSnapshotParseLoading: false }) }));
    CONNECTION_MAP.clear();
});
