/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 */
import React from 'react';
import { act, render } from '@testing-library/react';
import { createSession as createFeatureSession, type MemSnapshotSession as Session } from '../application/session';
import useBlockWorkerMessage from '../common/shared/workers/useBlockWorkerMessage';
import useStateWorkerMessage from '../workers/useStateWorkerMessage';
import { bindBlockWorkerSession } from '../common/leaksWorker/blockWorker/worker';
import { bindStateWorkerSession } from '../rendering/stateWorker/worker';

const mockBlockWorker = new EventTarget();
const mockStateWorker = new EventTarget();

jest.mock('../common/leaksWorker/blockWorker/worker', () => ({
    subscribeBlockWorker: (_session: Session, listener: EventListener) => {
        mockBlockWorker.addEventListener('message', listener);
        return () => mockBlockWorker.removeEventListener('message', listener);
    },
    bindBlockWorkerSession: jest.fn(() => jest.fn()),
    isCurrentBlockWorkerGeneration: (generation: number) => generation === 2,
}));
jest.mock('../rendering/stateWorker/worker', () => ({
    subscribeStateWorker: (_session: Session, listener: EventListener) => {
        mockStateWorker.addEventListener('message', listener);
        return () => mockStateWorker.removeEventListener('message', listener);
    },
    bindStateWorkerSession: jest.fn(() => jest.fn()),
    isCurrentStateWorkerGeneration: (generation: number) => generation === 2,
}));

const ScopeSubscriber = ({ session }: { session: Session }): JSX.Element => {
    useBlockWorkerMessage(session);
    return <></>;
};
const SnapshotSubscriber = ({ session }: { session: Session }): JSX.Element => {
    useStateWorkerMessage(session);
    return <></>;
};
const emit = (worker: EventTarget, data: object): void => {
    act(() => { worker.dispatchEvent(new MessageEvent('message', { data })); });
};

describe('feature worker ownership', () => {
    beforeEach(() => { jest.clearAllMocks(); });

    it('binds only the block renderer for MemScope and releases messages after unmount', () => {
        const session = createFeatureSession();
        const view = render(<ScopeSubscriber session={session} />);
        expect(bindBlockWorkerSession).toHaveBeenCalledWith(session);
        expect(bindStateWorkerSession).not.toHaveBeenCalled();
        emit(mockBlockWorker, { generation: 1, type: 'hoverItemResult', result: { id: 1 } });
        expect(session.leaksWorkerInfo.hoverItem).toBeNull();
        emit(mockBlockWorker, { generation: 2, type: 'hoverItemResult', result: { id: 2 } });
        expect(session.leaksWorkerInfo.hoverItem?.id).toBe(2);
        view.unmount();
        emit(mockBlockWorker, { generation: 2, type: 'hoverItemResult', result: { id: 3 } });
        expect(session.leaksWorkerInfo.hoverItem?.id).toBe(2);
    });

    it('replaces the old session subscription without updating the previous owner', () => {
        const first = createFeatureSession();
        const second = createFeatureSession();
        const view = render(<ScopeSubscriber session={first} />);
        view.rerender(<ScopeSubscriber session={second} />);
        emit(mockBlockWorker, { generation: 2, type: 'hoverItemResult', result: { id: 7 } });
        expect(first.leaksWorkerInfo.hoverItem).toBeNull();
        expect(second.leaksWorkerInfo.hoverItem?.id).toBe(7);
    });

    it('ignores stale state generations and selection versions, then detaches on exit', () => {
        const session = createFeatureSession();
        const view = render(<SnapshotSubscriber session={session} />);
        const result = { type: 'block', data: { blocks: [{ id: 8 }] } };
        emit(mockStateWorker, { generation: 1, type: 'clickItemResult', result, selectionVersion: 0 });
        emit(mockStateWorker, { generation: 2, type: 'clickItemResult', result, selectionVersion: 9 });
        expect(session.stateWorkerInfo.clickItem).toBeNull();
        emit(mockStateWorker, { generation: 2, type: 'clickItemResult', result, selectionVersion: 0 });
        expect(session.stateWorkerInfo.clickItem?.data.blocks[0].id).toBe(8);
        view.unmount();
        emit(mockStateWorker, { generation: 2, type: 'clickItemResult', result: null, selectionVersion: 0 });
        expect(session.stateWorkerInfo.clickItem).not.toBeNull();
    });
});

jest.mock('../../../memscope/src/common/leaksWorker/blockWorker/worker', () => jest.requireMock('../common/leaksWorker/blockWorker/worker'));
