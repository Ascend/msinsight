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
import type { NotificationHandler } from './connection/defs';
import {
    parseSuccessHandler,
    parseProgressHandler,
    parseFailHandler,
    allSuccessHandler,
    importRemoteHandler,
    savePageSettingRemoteHandler,
    removeRemoteHandler,
    setTheme,
    clusterCompletedHandler,
    removeSingleRemoteHandler,
    clusterDurationCompletedHandler,
    locateUnitHandler,
    switchLanguageHandler,
    baselineAddHandler,
    removeBaselineHandler,
    updateProjectNameHandler,
    resetRemoteHandler, findBlock,
    parseUnitCompletedHandler,
    getTimelineCardOffsetHandler,
    getTimelineRangeFlagListHandler,
    frameLoadedHandler,
    switchDirectoryHandler,
    updateClusterPageInfoHandler,
} from './connection/handler';

interface InsightInterface<Request extends Record<string, unknown>, Response extends Record<string, unknown>> {
    request: Request;
    response: Response;
};

const CHART_CPU = 'chart/cpu';
export interface InterfaceDefs {
    [CHART_CPU]: InsightInterface<{ chartId: number }, { data: Array<{ ts: number; value: number }>}>;
};

const REMOTE_SAVE_PAGE_SETTING = 'remote/savePageSetting';
const REMOTE_IMPORT = 'remote/import';
const REMOTE_REMOVE = 'remote/remove';
const REMOTE_RESET = 'remote/reset';
const REMOTE_REMOVE_SINGLE = 'remote/removeSingle';
const PARSE_SUCCESS = 'parse/success';
const PARSE_PROGRESS = 'parse/progress';
const PARSE_FAIL = 'parse/fail';
const PARSE_CLUSTER_COMPLETED = 'parse/clusterCompleted';
const PARSE_CLUSTER_STEP2_COMPLETED = 'parse/clusterStep2Completed';
const BASELINE_ADD = 'baseline/add';
const BASELINE_REMOVE = 'baseline/remove';
const PARSE_UNIT_COMPLETED = 'parse/unitCompleted';
const FRAME_LOADED = 'frame/loaded';
const SWITCH_DIRECTORY = 'switchDirectory';
const UPDATE_CLUSTER_PAGE_INFO = 'updateClusterPageInfo';
export const NOTIFICATION_HANDLERS: Record<string, NotificationHandler> = {
    [REMOTE_SAVE_PAGE_SETTING]: savePageSettingRemoteHandler,
    [REMOTE_IMPORT]: importRemoteHandler,
    [REMOTE_REMOVE]: removeRemoteHandler,
    [REMOTE_RESET]: resetRemoteHandler,
    allPagesSuccess: allSuccessHandler,
    [REMOTE_REMOVE_SINGLE]: removeSingleRemoteHandler,
    [PARSE_SUCCESS]: parseSuccessHandler,
    [PARSE_PROGRESS]: parseProgressHandler,
    [PARSE_FAIL]: parseFailHandler,
    [PARSE_CLUSTER_COMPLETED]: clusterCompletedHandler,
    [PARSE_CLUSTER_STEP2_COMPLETED]: clusterDurationCompletedHandler,
    [BASELINE_ADD]: baselineAddHandler,
    [BASELINE_REMOVE]: removeBaselineHandler,
    setTheme,
    locateUnit: locateUnitHandler,
    switchLanguage: switchLanguageHandler,
    updateProjectName: updateProjectNameHandler,
    findBlock,
    [PARSE_UNIT_COMPLETED]: parseUnitCompletedHandler,
    getTimelineCardOffset: getTimelineCardOffsetHandler,
    getTimelineRangeFlagList: getTimelineRangeFlagListHandler,
    [FRAME_LOADED]: frameLoadedHandler,
    [SWITCH_DIRECTORY]: switchDirectoryHandler,
    [UPDATE_CLUSTER_PAGE_INFO]: updateClusterPageInfoHandler,
};
