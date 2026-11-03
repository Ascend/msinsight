/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import React from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import connector from '@/connection';
import { message } from 'antd';
import { MemoryApplicationFrame } from '../components/TabPane/MemoryApplicationFrame';
import { queryLeaksParseComplete, warnIfLeaksParseBlocksDelete } from './leaksParseDeleteGuard';
import DeleteConfirm from '../components/RemoteManager/ProjectContents/DeleteConfirm';
import ImportData from '../components/RemoteManager/ImportData/Index';
import { removeDataPath, removeProject, removeProjects } from './Project';
import type { Session } from '../entity/session';

jest.mock('@/connection', () => {
    const { ServerConnector } = jest.requireActual('../../../lib/src/connection');
    return { __esModule: true, default: new ServerConnector({ getTargetWindow: () => [] }) };
});
jest.mock('@insight/lib/i18n', () => ({ __esModule: true, default: { t: (key: string) => key } }), { virtual: true });
jest.mock('antd', () => ({
    message: { warning: jest.fn() },
    Popconfirm: ({ onConfirm }: { onConfirm: () => Promise<void> }) =>
        <button onClick={onConfirm}>confirm-delete</button>,
    Tooltip: ({ children }: { children: React.ReactNode }) => <>{children}</>,
}));
jest.mock('mobx-react', () => ({ observer: (component: unknown) => component }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@insight/lib/icon', () => ({ DeleteIcon: () => null, ImportDataIcon: () => null }), { virtual: true });
jest.mock('@ant-design/icons', () => ({ ArrowLeftOutlined: () => null }));
jest.mock('@/utils/Project', () => ({ removeProject: jest.fn(), removeDataPath: jest.fn(), removeProjects: jest.fn() }));
jest.mock('@/utils/useLoading', () => ({ openLoading: jest.fn() }));
jest.mock('../components/RemoteManager/ImportData/FileExplorer', () => () => null);
jest.mock('../components/RemoteManager/ImportData/SetBtn', () => () => null);

describe('delete guard with only the independent snapshot iframe mounted', () => {
    const session = {
        isLeaks: true,
        module: 'leaks',
        activeDataSource: { projectName: 'current', selectedFilePath: 'D:/data/memory.pickle' },
    };

    beforeEach(() => {
        jest.useFakeTimers();
        jest.clearAllMocks();
    });
    afterEach(() => {
        jest.useRealTimers();
        jest.restoreAllMocks();
    });

    const mountSnapshot = (complete?: boolean): jest.SpyInstance => {
        const { container } = render(<MemoryApplicationFrame application="MemSnapshot" visible onLoad={() => {}} />);
        expect(container.querySelector('iframe[name="MemScope"]')).toBeNull();
        const frame = container.querySelector('iframe') as HTMLIFrameElement;
        return jest.spyOn(frame.contentWindow as Window, 'postMessage').mockImplementation((data) => {
            expect(JSON.parse(data)).toMatchObject({ event: 'getLeaksParseStatus', to: 'MemSnapshot' });
            if (complete !== undefined) {
                window.dispatchEvent(new MessageEvent('message', { data: { event: 'leaksParseStatus', body: { complete } } }));
            }
        });
    };

    it.each([
        ['project', { projectName: 'current' }],
        ['file in the active project', { projectName: 'current' }],
        ['selected projects', { projectNames: ['other', 'current'] }],
        ['all projects', { projectNames: [] }],
    ])('blocks deleting %s during parsing and allows it after completion', async (_, target) => {
        const post = mountSnapshot(false);
        expect(await warnIfLeaksParseBlocksDelete(session, target)).toBe(true);
        expect(post).toHaveBeenCalledTimes(1);
        expect(message.warning).toHaveBeenCalledWith('SnapshotParsingCannotDelete');
        post.mockImplementation(() => {
            window.dispatchEvent(new MessageEvent('message', { data: { event: 'leaksParseStatus', body: { complete: true } } }));
        });
        expect(await warnIfLeaksParseBlocksDelete(session, target)).toBe(false);
        expect(message.warning).toHaveBeenCalledTimes(1);
    });

    it('does not interpret a missing target iframe as completed parsing', async () => {
        expect(await warnIfLeaksParseBlocksDelete(session, { projectName: 'current' })).toBe(true);
        expect(jest.getTimerCount()).toBe(0);
    });

    it('blocks deletion on timeout and removes its listener before a late reply', async () => {
        mountSnapshot();
        const remove = jest.spyOn(connector, 'removeListener');
        const pending = warnIfLeaksParseBlocksDelete(session, { projectNames: [] });
        jest.advanceTimersByTime(2000);
        expect(await pending).toBe(true);
        expect(remove).toHaveBeenCalledTimes(1);
        window.dispatchEvent(new MessageEvent('message', { data: { event: 'leaksParseStatus', body: { complete: true } } }));
        expect(remove).toHaveBeenCalledTimes(1);
        expect(jest.getTimerCount()).toBe(0);
    });

    it('requires an explicit completion reply', async () => {
        mountSnapshot();
        const pending = queryLeaksParseComplete('MemSnapshot');
        window.dispatchEvent(new MessageEvent('message', { data: { event: 'leaksParseStatus', body: {} } }));
        expect(await pending).toBe(false);
    });

    it('keeps unrelated projects outside the snapshot guard', async () => {
        const post = mountSnapshot(false);
        expect(await warnIfLeaksParseBlocksDelete(session, { projectNames: ['other'] })).toBe(false);
        expect(post).not.toHaveBeenCalled();
    });

    it('routes database data to MemScope even with a stale snapshot module', async () => {
        const { container } = render(<MemoryApplicationFrame application="MemScope" visible onLoad={() => {}} />);
        const frame = container.querySelector('iframe') as HTMLIFrameElement;
        const post = jest.spyOn(frame.contentWindow as Window, 'postMessage').mockImplementation((data) => {
            expect(JSON.parse(data).to).toBe('MemScope');
            window.dispatchEvent(new MessageEvent('message', { data: { event: 'leaksParseStatus', body: { complete: true } } }));
        });
        expect(await warnIfLeaksParseBlocksDelete({
            ...session,
            module: 'memsnapshot',
            activeDataSource: { projectName: 'current', selectedFilePath: 'D:/data/memscope.db' },
        }, { projectName: 'current' })).toBe(false);
        expect(post).toHaveBeenCalledTimes(1);
    });

    it.each(['project', 'file', 'batch'])('protects the actual %s deletion entry', async (entry) => {
        const post = mountSnapshot(false);
        const activeSession = { ...session, projectContentEditStatus: true, actionListener: { type: '' } } as unknown as Session;
        const view = render(entry === 'batch'
            ? <ImportData session={activeSession} checkedProjectKeys={['current']} />
            : <DeleteConfirm session={activeSession} projectName="current" projectIndex={0}
                isProject={entry === 'project'} dataPath="D:/data/memory.pickle" />);
        await act(async () => { fireEvent.click(view.getByText('confirm-delete')); });
        expect(removeProject).not.toHaveBeenCalled();
        expect(removeDataPath).not.toHaveBeenCalled();
        expect(removeProjects).not.toHaveBeenCalled();
        post.mockImplementation(() => {
            window.dispatchEvent(new MessageEvent('message', { data: { event: 'leaksParseStatus', body: { complete: true } } }));
        });
        await act(async () => { fireEvent.click(view.getByText('confirm-delete')); });
        if (entry === 'project') expect(removeProject).toHaveBeenCalledWith(0);
        if (entry === 'file') expect(removeDataPath).toHaveBeenCalledWith(0, 'D:/data/memory.pickle');
        if (entry === 'batch') expect(removeProjects).toHaveBeenCalledWith(['current']);
    });
});
