/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { runInAction } from 'mobx';
import type { Session } from '../entity/session';

export const undoZoom = (session: Session): void => {
    if (session.contextMenu.zoomHistory.length === 0) return;
    runInAction(() => {
        session.contextMenu.zoomHistory.pop();
        const zoomHistoryLength = session.contextMenu.zoomHistory.length;
        if (zoomHistoryLength === 0) {
            session.setDomainWithoutHistory({
                domainStart: 0,
                domainEnd: session.endTimeAll ?? session.domain.defaultDuration,
            });
        } else {
            session.setDomainWithoutHistory(session.contextMenu.zoomHistory[zoomHistoryLength - 1]);
        }
    });
};

export const resetZoom = (session: Session): void => {
    if (session.contextMenu.zoomHistory.length === 0) return;
    runInAction(() => {
        session.setDomainWithoutHistory({
            domainStart: 0,
            domainEnd: session.endTimeAll ?? session.domain.defaultDuration,
        });
        session.contextMenu.zoomHistory = [];
    });
};
