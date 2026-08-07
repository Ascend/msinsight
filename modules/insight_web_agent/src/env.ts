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
declare global {
    interface Window {
        __ACP_API_BASE__?: string;
    }
}

const acpPort = new URLSearchParams(window.location.search).get('acpPort');
const defaultApiBase = process.env.NODE_ENV === 'development' ? 'http://127.0.0.1:9090' : '';

if (process.env.NODE_ENV !== 'development' && !window.__ACP_API_BASE__ && !acpPort) {
    throw new Error('Missing required acpPort parameter.');
}

export const apiBase = window.__ACP_API_BASE__ ?? (acpPort ? `http://127.0.0.1:${acpPort}` : defaultApiBase);

export const apiUrl = (path: string): string => {
    return apiBase ? `${apiBase}${path}` : path;
};

export {};
