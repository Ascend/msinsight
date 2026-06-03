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
import { ProjectAction, ProjectError } from '@/utils/enum';
import i18n from '@insight/lib/i18n';
import { message } from 'antd';
import { handleProjectAction } from '@/utils/Project';
import { checkPathValid } from '@/utils/Resource';
import { Project } from '@/centralServer/websocket/defs';

export function firstLetterUpper(value: string): string {
    const word = String(value);
    return word.charAt(0).toUpperCase() + word.slice(1);
}

// 注册文件拖拽事件
export function registerDragAndDropFile(): void {
    Object.defineProperty(window, 'handleDrop', {
        value: async (path: string) => {
            const project: Project = {
                projectName: path,
                projectPath: [path],
                children: [],
            };
            // 校验
            let validRes;
            try {
                validRes = await checkPathValid(project);
            } catch (error) {
                console.error('Check path valid failed:', error);
                message.error({ content: i18n.t('framework:FileCheckFailedDescribe') });
                return;
            }
            const projectError = validRes.result;
            // 校验通过
            if ([ProjectError.NO_ERRORS, ProjectError.IMPORTED].includes(projectError)) {
                const action = projectError === ProjectError.NO_ERRORS ? ProjectAction.ADD_FILE : ProjectAction.SWITCH_PROJECT;
                handleProjectAction({ action, project, isConflict: false });
            } else if (projectError === ProjectError.PROJECT_NAME_CONFLICT) {
                message.info({ content: `${i18n.t('framework:FileConflict')}:${i18n.t('framework:FileConflictContent')}` });
            } else {
                const detailMessage = validRes.errorDetail.map(error => `${error.path}: ${error.message}`).join('\n');
                message.info({ content: detailMessage === '' ? `Error:${ProjectError[projectError]}` : detailMessage });
            }
        },
        writable: true,
        enumerable: false,
        configurable: true,
    });
}

export const HELP_LINKS = {
    documentation: 'https://msinsight.readthedocs.io/',
    help_feedback: 'https://gitcode.com/Ascend/msinsight/issues',
};
