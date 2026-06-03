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
import { type NotificationHandler } from './connection/defs';
import {
    setTheme,
    updateSessionHandler,
    switchLanguageHandler,
    parseCompletedHandler,
    removeRemoteHandler,
    parseFailHandler,
    parseProgressHandler,
    importRemoteHandler,
} from './connection/handler';

const PARSECOMPLETED = 'parse/leaksMemoryCompleted';
const PARSEFAIL = 'parse/fail';
const PARSE_MEM_SNAPSHOT_PROGRESS = 'parse/memSnapshotProgress';
export const NOTIFICATION_HANDLERS: Record<string, NotificationHandler> = {
    setTheme,
    updateSession: updateSessionHandler,
    switchLanguage: switchLanguageHandler,
    'remote/import': importRemoteHandler,
    [PARSE_MEM_SNAPSHOT_PROGRESS]: parseProgressHandler,
    [PARSECOMPLETED]: parseCompletedHandler,
    [PARSEFAIL]: parseFailHandler,
    'remote/remove': removeRemoteHandler,
    'remote/reset': removeRemoteHandler,
};
