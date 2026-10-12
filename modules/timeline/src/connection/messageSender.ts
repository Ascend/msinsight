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
import { getWindowMessageRouter } from '@insight/lib/WindowMessageRouter';

export interface IMessageSender {
    sendMessage: (ceq: any) => void;
    selectFolder: () => Promise<string>;
    selectFile: () => Promise<string>;
}

const selectionCommands = new Set(['ascend.folderSelected', 'ascend.folderSelectionCanceled']);

export const removeAndAddEventListener = (resolve: (value: (string | PromiseLike<string>)) => void): void => {
    let unsubscribe = (): void => {};
    unsubscribe = getWindowMessageRouter().subscribe((event) => {
        const message = event.data as { command: string; path: string };
        unsubscribe(); // 上一次的取消订阅
        switch (message.command) {
            case 'ascend.folderSelected':
                resolve(message.path);
                break;
            case 'ascend.folderSelectionCanceled':
                resolve('');
                break;
            default:
        }
    }, event => (
        typeof event.data === 'object'
        && event.data !== null
        && selectionCommands.has((event.data as { command?: string }).command ?? '')
    ));
};
