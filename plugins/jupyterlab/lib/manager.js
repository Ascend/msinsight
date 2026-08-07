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
import { ArrayExt } from './utils/lumino/algorithm';
import { Signal } from './utils/lumino/signaling';
import { JSONExt } from './utils/lumino/coreutils';
import * as MindStudio from './mindstudio';
import { ServerConnection } from '@jupyterlab/services';
/**
 * A mindstudio manager.
 */
export class MindStudioManager {
    /**
     * Construct a new mindstudio manager.
     */
    constructor(options = {}) {
        this._models = [];
        this._mindstudios = new Set();
        this._isDisposed = false;
        this._isReady = false;
        this._runningChanged = new Signal(this);
        this._statusConfig = null;
        this.serverSettings =
            options.serverSettings || ServerConnection.makeSettings();
        this._readyPromise = this._refreshRunning();
        this.getStaticConfigPromise = this._getStaticConfig();
    }
    get runningChanged() {
        return this._runningChanged;
    }
    get isDisposed() {
        return this._isDisposed;
    }
    get isReady() {
        return this._isReady;
    }
    get ready() {
        return this._readyPromise;
    }
    dispose() {
        if (this.isDisposed) {
            return;
        }
        this._isDisposed = true;
        Signal.clearData(this);
        this._models = [];
    }
    /**
     * Create an iterator over the most recent running mindstudio.
     *
     * @returns A new iterator over the running mindstudio.
     */
    running() {
        return this._models;
    }
    /**
     * Start profiler server and show mindstudio iframe
     *
     * @returns A new mindstudio url.
     */
    async startIframeUrl() {
        return await MindStudio.startIframeUrl();
    }
    /**
     * Dispose mindstudio iframe and terminate profiler server
     */
    async terminateIframe(profilerServerId) {
        return await MindStudio.terminateIframe(profilerServerId);
    }
    /**
     * Create a new mindstudio.
     */
    async startNew(name, options) {
        const mindStudio = await MindStudio.startNew(name, this._getOptions(options));
        this._onStarted(mindStudio);
        return mindStudio;
    }
    /**
     * Shut down a mindstudio by name.
     */
    async shutdown(name) {
        const index = ArrayExt.findFirstIndex(this._models, (value) => value.name === name);
        if (index === -1) {
            return;
        }
        this._models.splice(index, 1);
        this._runningChanged.emit(this._models.slice());
        try {
            await MindStudio.shutdown(name, this.serverSettings);
            const toRemove = [];
            this._mindstudios.forEach((t) => {
                if (t.name === name) {
                    t.dispose();
                    toRemove.push(t);
                }
            });
            toRemove.forEach(s => {
                this._mindstudios.delete(s);
            });
        }
        catch (error) {
            throw error;
        }
    }
    /**
     * Shut down all mindstudio.
     *
     * @returns A promise that resolves when all of the mindstudio are shut down.
     */
    async shutdownAll() {
        const models = this._models;
        if (models.length > 0) {
            this._models = [];
            this._runningChanged.emit([]);
        }
        try {
            await this._refreshRunning();
            const toRemove = [];
            for (const model of models) {
                await MindStudio.shutdown(model.name, this.serverSettings);
                this._mindstudios.forEach((t) => {
                    t.dispose();
                    toRemove.push(t);
                });
                toRemove.forEach(t => {
                    this._mindstudios.delete(t);
                });
            }
            return undefined;
        }
        catch (error) {
            throw error;
        }
    }
    refreshRunning() {
        return this._refreshRunning();
    }
    _onTerminated(name) {
        const index = ArrayExt.findFirstIndex(this._models, (value) => value.name === name);
        if (index !== -1) {
            this._models.splice(index, 1);
            this._runningChanged.emit(this._models.slice());
        }
    }
    _onStarted(mindstudio) {
        const name = mindstudio.name;
        this._mindstudios.add(mindstudio);
        const index = ArrayExt.findFirstIndex(this._models, (value) => value.name === name);
        if (index === -1) {
            this._models.push(mindstudio.model);
            this._runningChanged.emit(this._models.slice());
        }
        mindstudio.terminated.connect(() => {
            this._onTerminated(name);
        });
    }
    async _refreshRunning() {
        const models = await MindStudio.listRunning(this.serverSettings);
        const currentModel = models.map(m => ({
            name: m.name
        }));
        this._isReady = true;
        const previousModel = this._models.map(m => ({
            name: m.name
        }));
        if (!JSONExt.deepEqual(currentModel, previousModel)) {
            const names = models.map(r => r.name);
            const toRemove = [];
            for (const t of this._mindstudios) {
                if (!names.includes(t.name)) {
                    t.dispose();
                    toRemove.push(t);
                }
            }
            for (const t of toRemove) {
                this._mindstudios.delete(t);
            }
            this._models = models.slice();
            this._runningChanged.emit(models);
        }
    }
    _getOptions(options = {}) {
        return { ...options, serverSettings: this.serverSettings };
    }
    _getStaticConfig() {
        return MindStudio.getStaticConfig(this.serverSettings).then(config => {
            var _a;
            if ((_a = this._statusConfig) === null || _a === void 0 ? void 0 : _a.notebookDir) {
                this._statusConfig = config;
            }
        });
    }
}
//# sourceMappingURL=manager.js.map