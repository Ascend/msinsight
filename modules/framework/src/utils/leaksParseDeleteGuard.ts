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

import { message as Message } from 'antd';
import i18n from '@insight/lib/i18n';
import connector from '@/connection';
import { getMemoryApplicationName, type MemoryApplicationName } from '@/moduleConfig';

const QUERY_TIMEOUT_MS = 2000;

export interface LeaksDeleteTarget {
    projectName?: string;
    projectNames?: readonly unknown[];
}

interface LeaksDeleteSession {
    isLeaks?: boolean;
    module?: string;
    activeDataSource?: { projectName?: string; selectedFilePath?: string };
}

export const shouldQueryLeaksParseStatus = (
    session: LeaksDeleteSession | undefined,
    target: LeaksDeleteTarget,
): boolean => {
    if (session?.isLeaks !== true) {
        return false;
    }
    const activeName = session.activeDataSource?.projectName;
    if (activeName === undefined || activeName === '') {
        return false;
    }
    if (target.projectNames !== undefined) {
        return target.projectNames.length === 0 || target.projectNames.includes(activeName);
    }
    return target.projectName === activeName;
};

export const queryLeaksParseComplete = (application: MemoryApplicationName): Promise<boolean> =>
    new Promise((resolve) => {
        let settled = false;
        const finish = (complete: boolean): void => {
            if (settled) {
                return;
            }
            settled = true;
            window.clearTimeout(timer);
            connector.removeListener(listener);
            resolve(complete);
        };
        const listener = connector.addListener('leaksParseStatus', (event: MessageEvent<{ body?: { complete?: boolean } }>) => {
            finish(event.data?.body?.complete === true);
        });
        const timer = window.setTimeout(() => {
            finish(false);
        }, QUERY_TIMEOUT_MS);
        connector.send({
            event: 'getLeaksParseStatus',
            to: application,
            body: {},
        }, () => {
            finish(false);
        });
    });

export const warnIfLeaksParseBlocksDelete = async (
    session: LeaksDeleteSession | undefined,
    target: LeaksDeleteTarget,
): Promise<boolean> => {
    if (!shouldQueryLeaksParseStatus(session, target)) {
        return false;
    }
    const application = getMemoryApplicationName(session?.module ?? '', session?.activeDataSource?.selectedFilePath);
    const complete = await queryLeaksParseComplete(application);
    if (complete) {
        return false;
    }
    Message.warning(i18n.t('SnapshotParsingCannotDelete', { ns: 'framework' }));
    return true;
};
