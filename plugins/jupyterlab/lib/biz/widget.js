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
import { ReactWidget } from '@jupyterlab/apputils';
import React from 'react';
import * as CommandIDs from '../commands';
import { MindStudioInsightTab } from './MindStudioInsight';
export class MindStudioReactWidget extends ReactWidget {
    constructor(options) {
        super();
        this.currentMindStudioModel = null;
        this.closeCurrent = () => {
            this.dispose();
            this.close();
        };
        this.startIframeUrl = async () => {
            const url = await this.mindstudioManager.startIframeUrl();
            const match = url.match(/profilerServerId=(?<profilerServerId>[^&;]+)/);
            this.profilerServerId = match ? match[1] : null;
            return url;
        };
        this.startNew = (name, options) => {
            return this.mindstudioManager.startNew(name, options);
        };
        this.setWidgetName = (name) => {
            this.title.label = name || 'MindStudio Insight';
            this.title.caption = `Name: ${this.title.label}`;
        };
        this.updateCurrentModel = (model) => {
            this.currentMindStudioModel = model;
        };
        this.openMindStudio = (modelName) => {
            this.app.commands.execute(CommandIDs.open, {
                modelName,
            });
        };
        this.mindstudioManager = options.mindstudioManager;
        this.createdModelName = options.createdModelName;
        this.app = options.app;
        this.profilerServerId = '';
        this.title.closable = true;
        this.title.label = 'MindStudio Insight';
        this.title.caption = `Name: ${this.title.label}`;
    }
    dispose() {
        var _a;
        this.mindstudioManager.terminateIframe((_a = this.profilerServerId) !== null && _a !== void 0 ? _a : '').then(() => { })
            .catch(() => { });
        super.dispose();
    }
    render() {
        return (React.createElement(MindStudioInsightTab, { mindstudioManager: this.mindstudioManager, closeWidget: this.closeCurrent, openMindStudio: this.openMindStudio, updateCurrentModel: this.updateCurrentModel, startNew: this.startNew, startIFrame: this.startIframeUrl }));
    }
    ;
}
//# sourceMappingURL=widget.js.map