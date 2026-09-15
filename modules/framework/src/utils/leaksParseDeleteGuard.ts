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
import { MEM_SCOPE_MODULE_NAME } from '@/moduleConfig';

const QUERY_TIMEOUT_MS = 2000;

export interface LeaksDeleteTarget {
    projectName?: string;
    projectNames?: readonly unknown[];
}

export const shouldQueryLeaksParseStatus = (
    session: { isLeaks?: boolean; activeDataSource?: { projectName?: string } } | undefined,
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

export const queryLeaksParseComplete = (): Promise<boolean> =>
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
            finish(event.data?.body?.complete !== false);
        });
        const timer = window.setTimeout(() => {
            finish(true);
        }, QUERY_TIMEOUT_MS);
        connector.send({
            event: 'getLeaksParseStatus',
            to: MEM_SCOPE_MODULE_NAME,
            body: {},
        }, () => {
            finish(true);
        });
    });

export const warnIfLeaksParseBlocksDelete = async (
    session: { isLeaks?: boolean; activeDataSource?: { projectName?: string } } | undefined,
    target: LeaksDeleteTarget,
): Promise<boolean> => {
    if (!shouldQueryLeaksParseStatus(session, target)) {
        return false;
    }
    const complete = await queryLeaksParseComplete();
    if (complete) {
        return false;
    }
    Message.warning(i18n.t('SnapshotParsingCannotDelete', { ns: 'framework' }));
    return true;
};
