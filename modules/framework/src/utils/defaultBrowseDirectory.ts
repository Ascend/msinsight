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

const JUPYTERLAB_IFRAME_PATH = '/resources/profiler/frontend/index.html';
const CONFIG_REQUEST_TIMEOUT_MS = 5000;
// Each Insight iframe has its own module instance; retain successful configuration only.
let defaultDirectoryRequest: Promise<string> | undefined;

export const getInsightFileSelectorConfigUrl = (href: string): string | undefined => {
    const url = new URL(href);
    if (!['http:', 'https:'].includes(url.protocol) || !url.searchParams.get('profilerServerId') || !url.pathname.endsWith(JUPYTERLAB_IFRAME_PATH)) {
        return undefined;
    }
    // Derive the same Jupyter base_url used by its existing static-frontend route.
    url.pathname = `${url.pathname.slice(0, -JUPYTERLAB_IFRAME_PATH.length)}/mindstudio_insight_jupyterlab/file_selector_config`;
    url.search = '';
    url.hash = '';
    return url.toString();
};

const requestInsightFileSelectorDefaultDirectory = async (
    endpoint: string,
    fetcher: typeof fetch,
): Promise<string> => {
    const controller = new AbortController();
    const timer = window.setTimeout(() => controller.abort(), CONFIG_REQUEST_TIMEOUT_MS);
    try {
        const response = await fetcher(endpoint, {
            credentials: 'same-origin', cache: 'no-store', redirect: 'error', signal: controller.signal,
        });
        if (response.status === 404) {
            return ''; // An older Jupyter extension has no default-directory setting.
        }
        if (!response.ok) {
            throw new Error('Cannot read the Insight file selector default directory configuration');
        }
        const config: unknown = await response.json();
        if (!config || typeof config !== 'object' || !('defaultBrowseDir' in config) || typeof config.defaultBrowseDir !== 'string') {
            throw new Error('Invalid Insight file selector configuration');
        }
        return config.defaultBrowseDir;
    } finally {
        window.clearTimeout(timer);
    }
};

export const loadInsightFileSelectorDefaultDirectory = async (
    href = window.location.href,
    fetcher: typeof fetch = (...args) => window.fetch(...args),
): Promise<string> => {
    const endpoint = getInsightFileSelectorConfigUrl(href);
    if (!endpoint) {
        return '';
    }
    if (!defaultDirectoryRequest) {
        defaultDirectoryRequest = requestInsightFileSelectorDefaultDirectory(endpoint, fetcher).catch((error: unknown) => {
            defaultDirectoryRequest = undefined;
            throw error;
        });
    }
    return defaultDirectoryRequest;
};

export const prefetchInsightFileSelectorDefaultDirectory = (href?: string, fetcher?: typeof fetch): void => {
    void loadInsightFileSelectorDefaultDirectory(href, fetcher).catch((error: unknown) => {
        console.warn('Prefetch Insight file selector configuration failed:', error);
    });
};
