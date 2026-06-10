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

import { runInAction } from 'mobx';
import { register } from './register';
import type { Session } from '../entity/session';
import { checkCardIsIE } from './actionGenerateCurve';

const timeRangeAnalysisMenuVisible = (session: Session): boolean => {
    // Ftrace 数据暂时不支持时间范围分析
    if (session.hasFtraceData) {
        return false;
    }
    if (session.isTimeAnalysisMode || session.selectedRangeIsLock || session.sliceSelection.active) {
        return false;
    }
    if (session.selectedRange === undefined || session.selectedUnits === undefined) {
        return false;
    }
    // 算子调优和服务化调优不支持
    if (session.isSimulation || session.isIE) {
        return false;
    }
    return session.selectedUnits.length !== 0;
};

const applyTimeRangeAnalysisMenuVisible = (session: Session): boolean => {
    // Ftrace 数据暂时不支持时间范围分析
    if (session.hasFtraceData) {
        return false;
    }
    if (session.isTimeAnalysisMode || session.selectedRangeIsLock || session.sliceSelection.active || !session.selectedData) {
        return false;
    }
    // 算子调优和服务化调优不支持
    return !(session.isSimulation || checkCardIsIE(session));
};

const timeRangeAnalysis = (session: Session): void => {
    if (session.selectedRange === undefined || session.selectedUnits === undefined) {
        return;
    }
    if (session.selectedUnits.length === 0) {
        return;
    }
    runInAction(() => {
        session.isTimeAnalysisMode = true;
        session.mKeyRender = true;
        if (session.selectedRange !== undefined) {
            session.timeAnalysisRange = [session.selectedRange[0], session.selectedRange[1]];
            // 此处利用M键的竖向遮罩
            session.mMaskRange = [session.selectedRange[0], session.selectedRange[1]];
        }
    });
};

const removeTimeRangeAnalysis = (session: Session): void => {
    runInAction(() => {
        session.isTimeAnalysisMode = false;
        session.timeAnalysisRange = undefined;
        // 因为时间范围分析利用了M键的竖向遮罩，所以移除时需要清空
        session.mKeyRender = false;
        session.mMaskRange = [];
    });
};

const timeRangeAnalysisAndZoomIn = (session: Session): void => {
    if (session.selectedRange === undefined || session.selectedUnits === undefined) {
        return;
    }
    if (session.selectedUnits.length === 0) {
        return;
    }
    runInAction(() => {
        session.isTimeAnalysisMode = true;
        session.mKeyRender = true;
        if (session.selectedRange !== undefined) {
            session.timeAnalysisRange = [session.selectedRange[0], session.selectedRange[1]];
            const tempRange = session.selectedRange[1] - session.selectedRange[0];
            session.domainRange = {
                domainStart: session.selectedRange[0] - tempRange * 0.05,
                domainEnd: session.selectedRange[1] + tempRange * 0.05,
            };
            // 此处利用M键的竖向遮罩
            session.mMaskRange = [session.selectedRange[0], session.selectedRange[1]];
        }
    });
};

const applyTimeRangeAnalysis = (session: Session): void => {
    if (session.selectedData === undefined) {
        return;
    }
    runInAction(() => {
        session.isTimeAnalysisMode = true;
        session.mKeyRender = true;
        const selectedData = session.selectedData;
        if (selectedData !== undefined) {
            session.timeAnalysisRange = [selectedData.startTime, selectedData.startTime + selectedData.duration];
            session.domainRange = {
                domainStart: selectedData.startTime - selectedData.duration * 0.05,
                domainEnd: selectedData.startTime + selectedData.duration + selectedData.duration * 0.05,
            };
            // 此处利用M键的竖向遮罩
            session.mMaskRange = [selectedData.startTime, selectedData.startTime + selectedData.duration];
        }
    });
};

export const actionTimeRangeAnalysis = register({
    name: 'timeRangeAnalysis',
    label: 'timeline:contextMenu.Time Range Analysis',
    visible: (session: Session): boolean => timeRangeAnalysisMenuVisible(session),
    perform: (session): void => {
        timeRangeAnalysis(session);
    },
});

export const actionRemoveTimeRangeAnalysis = register({
    name: 'removeTimeRangeAnalysis',
    label: 'timeline:contextMenu.Remove Time Range Analysis',
    visible: (session: Session): boolean => session.isTimeAnalysisMode,
    perform: (session): void => {
        removeTimeRangeAnalysis(session);
    },
});

export const actionTimeRangeAnalysisAndZoomIn = register({
    name: 'timeRangeAnalysisAndZoomIn',
    label: 'timeline:contextMenu.Time Range Analysis and Zoom in',
    visible: (session: Session): boolean => timeRangeAnalysisMenuVisible(session),
    perform: (session): void => {
        timeRangeAnalysisAndZoomIn(session);
    },
});

export const actionApplyTimeRangeAnalysis = register({
    name: 'applyTimeRangeAnalysis',
    label: 'timeline:contextMenu.Apply Time Range Analysis',
    visible: (session: Session): boolean => applyTimeRangeAnalysisMenuVisible(session),
    perform: (session): void => {
        applyTimeRangeAnalysis(session);
    },
});
