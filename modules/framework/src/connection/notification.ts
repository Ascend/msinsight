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
import {
    selectClusterHandler,
    deleteCardHandler,
    getParseStatusHandler,
    getThemeHandler,
    switchModuleHandler,
    updateSessionHandler,
    getLanguageHandler,
    openImportDialogHandler,
    clusterCompletedHandler,
    clusterDurationCompletedHandler,
} from './notificationHandler';
import { sendWakeupPlugin } from '@/connection/sendNotification';

export interface NotificationMessage {
    data: {
        body: {
            [p: string]: unknown;
            from?: string;
            broadcast?: boolean;
            switchTo?: string;
            toModuleEvent?: string;
        };
        event: string;
        module: string;
        from?: string;
        modules?: string[];
        port?: number;
    };
    isTrusted?: boolean;
}

type ListenerCallback = (res: NotificationMessage) => void;

const PARSE_CLUSTER_COMPLETED = 'frame:parseClusterCompleted';
const PARSE_CLUSTER_STEP2_COMPLETED = 'frame:parseClusterDurationCompleted';

export const listenerMap: Record<string, ListenerCallback> = {
    'clusterPage/selectCluster': selectClusterHandler,
    updateSession: updateSessionHandler,
    getParseStatus: getParseStatusHandler,
    getTheme: getThemeHandler,
    deleteCard: deleteCardHandler,
    switchModule: switchModuleHandler,
    getLanguage: getLanguageHandler,
    pluginMounted: sendWakeupPlugin,
    openImportDialog: openImportDialogHandler,
    [PARSE_CLUSTER_COMPLETED]: clusterCompletedHandler,
    [PARSE_CLUSTER_STEP2_COMPLETED]: clusterDurationCompletedHandler,
};
