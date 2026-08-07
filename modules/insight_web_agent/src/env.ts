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
const capabilityToken = new URLSearchParams(window.location.search).get('capabilityToken');
const jupyterlabProxy = new URLSearchParams(window.location.search).get('jupyterlabProxy') === 'true';
const defaultApiBase = process.env.NODE_ENV === 'development' ? 'http://localhost:9090' : '';

if (process.env.NODE_ENV !== 'development' && (!window.__ACP_API_BASE__ && !acpPort || !capabilityToken)) {
    throw new Error('Missing required ACP connection parameters.');
}

const resolveAcpApiBase = (): string => {
    if (window.__ACP_API_BASE__) return window.__ACP_API_BASE__;
    if (acpPort) return resolveAcpPortBase(acpPort);
    return defaultApiBase;
};

const resolveAcpPortBase = (port: string): string => {
    const { host, pathname, protocol } = window.location;
    const apiProtocol = `${protocol === 'https:' && host !== 'wry.localhost' ? 'https:' : 'http:'}//`;
    if (jupyterlabProxy) {
        const path = pathname.replace(/\/resources\/profiler\/frontend.*/, '').replace(/\/proxy\/\d+.*/, '');
        return `${apiProtocol}${host}${path}/proxy/${port}`;
    }
    if (!pathname.includes('/proxy/')) {
        const hostname = window.location.hostname || 'localhost';
        return `${apiProtocol}${hostname}:${port}`;
    }
    const path = pathname.replace(/\/proxy\/\d+.*/, `/proxy/${port}`);
    return `${apiProtocol}${host}${path}`;
};

export const apiBase = resolveAcpApiBase();

export const apiUrl = (path: string): string => {
    const url = apiBase ? `${apiBase}${path}` : path;
    if (!capabilityToken) return url;
    const separator = url.includes('?') ? '&' : '?';
    return `${url}${separator}capabilityToken=${encodeURIComponent(capabilityToken)}`;
};

export {};
