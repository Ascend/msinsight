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
import React from 'react';
import { observer } from 'mobx-react';
import { Popconfirm, Tooltip, message } from 'antd';
import { DeleteIcon } from '@insight/lib/icon';
import { removeDataPath, removeProject } from '@/utils/Project';
import { warnIfLeaksParseBlocksDelete } from '@/utils/leaksParseDeleteGuard';
import { useTranslation } from 'react-i18next';
import { openLoading } from '@/utils/useLoading';
import type { Session } from '@/entity/session';

interface IProps {
    isProject: boolean;
    projectIndex: number;
    dataPathIndex?: number;
    dataPath?: string;
    session?: Session;
    projectName?: string;
}
const DeleteConfirm = observer(({ isProject, projectIndex, dataPath, session, projectName }: IProps) => {
    const { t } = useTranslation('framework');
    let isSelectBaseline = false;
    const confirm = async (): Promise<void> => {
        if (await warnIfLeaksParseBlocksDelete(session, { projectName })) {
            return;
        }
        if (session?.compareSet) {
            const { compareSet: { baseline, comparison } } = session;
            if (isProject) {
                isSelectBaseline = baseline.filePath.startsWith(projectName as string) || comparison.filePath.startsWith(projectName as string);
            } else {
                isSelectBaseline = baseline.filePath.startsWith(dataPath as string) || comparison.filePath.startsWith(dataPath as string);
            }
        }
        if (isSelectBaseline) {
            message.warning(t('BaselineDataComparisonDataCannotDeleted'));
        } else {
            openLoading();
            if (isProject) {
                removeProject(projectIndex);
            } else {
                if (dataPath !== undefined) {
                    removeDataPath(projectIndex, dataPath);
                }
            }
        }
    };
    return <Popconfirm placement="topLeft"
        title={isProject ? t('DeleteProjectConfirmDescribe') : t('DeleteItemConfirmDescribe')}
        onConfirm={confirm}
        okText={t('Yes')}
        cancelText={t('No')}
        destroyTooltipOnHide={{ keepParent: false }}
    >
        <Tooltip placement="top" title={isProject ? t('Delete All') : t('Delete Item')} destroyTooltipOnHide={{ keepParent: false }}>
            <DeleteIcon/>
        </Tooltip>
    </Popconfirm>;
});

export default DeleteConfirm;
