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

import { URLExt } from '@jupyterlab/coreutils';
import { DefaultMindStudio } from './defaultMindStudio';

/**
 * The url for the mindstudio service.
 */
const MINDSTUDIO_SERVICE_URL = '';

const MINDSTUDIO_STATIC_CONFIG_URL = '';

const MINDSTUDIO_URL = '/resources/profiler/frontend/index.html';

const MINDSTUDIO_IFRAME_CONFIG_URL = '/mindstudio_insight_jupyterlab/get_iframe_config';

const MINDSTUDIO_TERMINATE_PROFILER_URL = '/mindstudio_insight_jupyterlab/terminate_profiler_server';

/**
 * A namespace for private data.
 */

/**
 * A mapping of running mindstudio by url.
 */
export const running: { [key: string]: DefaultMindStudio } =
    Object.create(null);

/**
 * Get the url for a mindstudio.
 */
export function getMindStudioUrl(baseUrl: string, name: string): string {
    return URLExt.join(baseUrl, MINDSTUDIO_SERVICE_URL, name);
}

/**
 * Get the url for a mindstudio.
 */
export function getMindStudioStaticConfigUrl(baseUrl: string): string {
    return URLExt.join(baseUrl, MINDSTUDIO_STATIC_CONFIG_URL);
}

/**
 * Get the base url.
 */
export function getServiceUrl(baseUrl: string): string {
    return URLExt.join(baseUrl, MINDSTUDIO_SERVICE_URL);
}

/**
 * Kill mindstudio by url.
*/
export function killMindStudio(url: string): void {
    // Update the local data store.
    if (running[url]) {
        const mindstudio = running[url];
        mindstudio.dispose();
    }
}

/**
 * Get the iframe config url.
 */
export function getIFrameConfigUrl(baseUrl: string): string {
    return URLExt.join(baseUrl, MINDSTUDIO_IFRAME_CONFIG_URL);
}

/**
 * Terminate profiler server.
 */
export function terminateIframe(baseUrl: string, profilerServerId: string): string {
    let url = URLExt.join(baseUrl, MINDSTUDIO_TERMINATE_PROFILER_URL);
    return `${url}?profilerServerId=${profilerServerId}`;
}

export function getMindStudioInstanceRootUrl(baseUrl: string): string {
    return URLExt.join(baseUrl, MINDSTUDIO_URL);
}

export function getMindStudioInstanceUrl(
    baseUrl: string,
    proxy: boolean,
    port: string,
    profilerServerId: string,
    acpPort?: string,
): string {
    let url = URLExt.join(baseUrl, MINDSTUDIO_URL);
    const acpPortParam = acpPort ? `&acpPort=${encodeURIComponent(acpPort)}` : '';
    if (proxy) {
        return `${url}?jupyterlabProxy=true&port=${port}&profilerServerId=${profilerServerId}${acpPortParam}`;
    }
    return `${url}?port=${port}&profilerServerId=${profilerServerId}${acpPortParam}`;
}
