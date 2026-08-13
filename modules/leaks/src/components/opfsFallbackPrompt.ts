/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the License at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

type FallbackPromptListener = () => void;

export interface OpfsFallbackPrompt {
    approve: () => void;
    getSnapshot: () => boolean;
    requestApproval: () => Promise<void>;
    subscribe: (listener: FallbackPromptListener) => () => void;
}

export const createOpfsFallbackPrompt = (): OpfsFallbackPrompt => {
    let visible = false;
    let pendingApproval: Promise<void> | undefined;
    let resolveApproval: (() => void) | undefined;
    const listeners = new Set<FallbackPromptListener>();

    const setVisible = (nextVisible: boolean): void => {
        visible = nextVisible;
        listeners.forEach(listener => listener());
    };

    return {
        approve: () => {
            if (!resolveApproval) {
                return;
            }
            const resolve = resolveApproval;
            resolveApproval = undefined;
            pendingApproval = undefined;
            setVisible(false);
            resolve();
        },
        getSnapshot: () => visible,
        requestApproval: () => {
            if (!pendingApproval) {
                pendingApproval = new Promise<void>(resolve => {
                    resolveApproval = resolve;
                    setVisible(true);
                });
            }
            return pendingApproval;
        },
        subscribe: listener => {
            listeners.add(listener);
            return () => listeners.delete(listener);
        },
    };
};
