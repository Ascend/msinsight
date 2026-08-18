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

import type { InsightUnit } from '../entity/insight';
import type { Session } from '../entity/session';

/**
 * Returns the lane that owns the currently selected slice.
 *
 * selectedUnits remains the range-selected lane collection, so its first item is
 * only a compatibility fallback for selection paths that do not provide a source.
 */
export const getSelectedDataUnit = (session: Session): InsightUnit | undefined => {
    return session.selectedDataUnit ?? session.selectedUnits[0];
};

/** Selects the renderer without allowing a range-selection lane to override slice context. */
export const getBottomPanelUnit = (session: Session, isSliceDetail: boolean): InsightUnit | undefined => {
    if (isSliceDetail && session.selectedDataUnit !== undefined) {
        return session.selectedDataUnit.bottomPanelRender === undefined ? undefined : session.selectedDataUnit;
    }
    return session.selectedUnits.find(unit => unit.bottomPanelRender !== undefined);
};
