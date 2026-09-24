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
import { runInAction } from 'mobx';
import type { SelectedDataType } from '../entity/session';

const isFocusableOperator = (selectedData: SelectedDataType | undefined): selectedData is SelectedDataType =>
    selectedData !== undefined && Number.isFinite(selectedData.startTime) && Number.isFinite(selectedData.duration) &&
    selectedData.duration > 0 && Number.isFinite(selectedData.startTime + selectedData.duration);

export const actionFitToScreen = register({
    name: 'fitToScreen',
    label: 'timeline:contextMenu.Fit to screen',
    visible: (session) => session.selectedData !== undefined,
    disabled: (session) => !isFocusableOperator(session.selectedData),
    perform: (session): void => {
        const selectedData = session.selectedData;
        if (!isFocusableOperator(selectedData)) return;
        runInAction(() => {
            session.domainRange = {
                domainStart: selectedData.startTime,
                domainEnd: selectedData.startTime + selectedData.duration,
            };
        });
    },
});
