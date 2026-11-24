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
import React from 'react';
import { createRoot } from 'react-dom/client';
import '@insight/lib/style';
import { App } from './app/App';
import { CommunicationStore } from './store/CommunicationStore';
import { HostBridge } from './bridge/HostBridge';
import { CommunicationApi, type CommunicationRequest } from './api';
async function start(): Promise<void> {
    const store = new CommunicationStore();
    const bridge = new HostBridge(store);
    let request: CommunicationRequest = bridge.request;
    // Embedded pages must always use the host's real project and API together.
    if (window.parent === window && process.env.NODE_ENV === 'development' && process.env.REACT_APP_COMMUNICATION_MOCK === 'true') {
        const fixture = await import('./dev/fixture');
        request = fixture.request;
        store.updateHost(fixture.context);
    }
    const root = document.getElementById('root');
    if (root) { createRoot(root).render(<App services={{ store, bridge, api: new CommunicationApi(request) }} />); }
}
void start();
