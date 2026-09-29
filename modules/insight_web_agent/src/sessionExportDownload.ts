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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import type { SessionExportDocument } from './types';

interface HostIpc {
    postMessage?: (message: string) => void;
}

const hostIpc = (): HostIpc | undefined => {
    const current = window as Window & { ipc?: HostIpc };
    const parentWindow = window.parent as (Window & { ipc?: HostIpc }) | undefined;
    return current.ipc ?? (parentWindow !== window ? parentWindow?.ipc : undefined);
};

export const revealExportedFile = (savedPath: string): void => {
    if (!savedPath) return;
    hostIpc()?.postMessage?.(`openProjectInExplorer|${savedPath}`);
};

export const downloadSessionExport = (document: SessionExportDocument): void => {
    if (document.savedPath) revealExportedFile(document.savedPath);
    const blob = new Blob([JSON.stringify(document, null, 2)], { type: 'application/json' });
    const objectUrl = URL.createObjectURL(blob);
    const link = window.document.createElement('a');
    link.href = objectUrl;
    link.download = document.filename;
    window.document.body.appendChild(link);
    link.click();
    link.remove();
    window.setTimeout(() => URL.revokeObjectURL(objectUrl), 1000);
};
