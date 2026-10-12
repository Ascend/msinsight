/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

import { register } from './register';
import { errorCenter, ErrorCode, KEYS, WsError } from '@insight/lib/utils';
import type { Session } from '../entity/session';
import { runInAction } from 'mobx';
import { getZoomPoint } from '../components/charts/ChartInteractor/ChartInteractor';
import { MAX_ZOOM_DURATION } from '../entity/domain';
import { i18n } from '@insight/lib';
import { resetZoom, undoZoom } from './zoomHistory';

enum ZoomDirection {
    IN = -1,
    OUT = 1,
}

const zoomDomain = (session: Session, zoomCount: number, zoomPoint: number | undefined): void => {
    runInAction(() => {
        session.zoom = { zoomCount, zoomPoint };
    });
};

export const actionUndoZoom = register({
    name: 'undoZoom',
    label: (session: Session, t) =>
        `${t('timeline:contextMenu.Undo Zoom')}(${session.contextMenu.zoomHistory.length})`,
    disabled: (session: Session) => session.contextMenu.zoomHistory.length === 0,
    perform: (session): void => {
        undoZoom(session);
    },
    keyTest: (event) => {
        return event.key === KEYS.BACKSPACE;
    },
    once: true,
});

export const actionResetZoom = register({
    name: 'resetZoom',
    label: 'timeline:contextMenu.Reset Zoom',
    disabled: (session: Session) => session.contextMenu.zoomHistory.length === 0,
    perform: (session): void => {
        resetZoom(session);
    },
    keyTest: (event) => {
        return event[KEYS.CTRL_OR_CMD] && event.key === KEYS['0'];
    },
    once: true,
});

export const actionZoomIn = register({
    name: 'zoomIn',
    label: '',
    perform: (session, interactorMouseState, xScale): void => {
        const zoomPoint = interactorMouseState === undefined || xScale === undefined
            ? 0
            : getZoomPoint(xScale, interactorMouseState);
        zoomDomain(session, ZoomDirection.IN, zoomPoint);
    },
    keyTest: (event) => event.key.toLowerCase() === KEYS.W,
});

export const actionZoomOut = register({
    name: 'zoomOut',
    label: '',
    perform: (session, interactorMouseState, xScale): void => {
        if (session.domain.duration >= MAX_ZOOM_DURATION) {
            errorCenter.handleError(new WsError(ErrorCode.ZOOM_LIMIT_WARNING, i18n.t('timeline:zoomOutLimitWarning', { maxDuration: MAX_ZOOM_DURATION / 60 / 1e9 })));
            return;
        }

        const zoomPoint = interactorMouseState === undefined || xScale === undefined
            ? 0
            : getZoomPoint(xScale, interactorMouseState);
        zoomDomain(session, ZoomDirection.OUT, zoomPoint);
    },
    keyTest: (event) => event.key.toLowerCase() === KEYS.S,
});
