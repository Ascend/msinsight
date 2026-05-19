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

import type React from 'react';
import { Session } from '../entity/session';
import { InteractorMouseState } from '../components/charts/ChartInteractor/ChartInteractor';
import type { TFunction } from 'i18next';
import { ContextMenuItem } from '../components/ContextMenu';

type ActionFn = (
    session: Session,
    interactorMouseState?: InteractorMouseState,
    xScale?: (x: number) => number
) => void;

export type ActionName =
  | 'zoomIn'
  | 'zoomOut'
  | 'undoZoom'
  | 'resetZoom'
  | 'panLeft'
  | 'panRight'
  | 'scrollUp'
  | 'scrollDown'
  | 'toggleOperatorSelection'
  | 'alignToBenchmarkLeft'
  | 'alignToBenchmarkRight'
  | 'alignByOperator'
  | 'alignByOperatorLeft'
  | 'alignByOperatorRight'
  | 'fitToScreen'
  | 'findInCommunication'
  | 'zoomIntoSelection'
  | 'unpinAll'
  | 'pinByGroupNameValue'
  | 'unpinByGroupNameValue'
  | 'parseCardsOfRelatedGroup'
  | 'hideUnits'
  | 'showHiddenUnits'
  | 'showInEventsView'
  | 'showPythonCallStack'
  | 'hidePythonCallStack'
  | 'collapseAllUnits'
  | 'expandAllUnits'
  | 'hideFlagEvents'
  | 'showFlagEvents'
  | 'timeRangeAnalysis'
  | 'removeTimeRangeAnalysis'
  | 'timeRangeAnalysisAndZoomIn'
  | 'applyTimeRangeAnalysis'
  | 'enableAutoUnitHeight'
  | 'disableAutoUnitHeight'
  | 'recoverDefaultOffset'
  | 'setBaseSlice'
  | 'clearBaseSlice'
  | 'toggleSelection'
  | 'toggleBottomPanel'
  | 'lockSelection'
  | 'unlockSelection'
  | 'setCardAlias'
  | 'mergeUnits'
  | 'unmergeUnits'
  | 'generateCurveByBlock'
  | 'generateBubbleCurveByBlock'
  | 'createFlagMark'
  | 'sliceSelection'
  | 'jumpToLinkSlice';

export interface Action {
    name: ActionName;
    label: string | ((session: Session, t: TFunction) => string);
    disabled?: (session: Session) => boolean;
    visible?: (session: Session) => boolean;
    perform: ActionFn;
    keyTest?: (
        event: React.KeyboardEvent | KeyboardEvent,
    ) => boolean;
    once?: boolean;
    checked?: (session: Session) => boolean;
    subMode?: boolean;
    subMenus?: (session: Session) => ContextMenuItem[];
    parentMenuKey?: string;
    style?: { [key: string]: any };
}
