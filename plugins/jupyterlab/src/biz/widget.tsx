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

import { JupyterFrontEnd } from '@jupyterlab/application';
import { ReactWidget } from '@jupyterlab/apputils';
import React from 'react';
import * as MindStudio from '../mindstudio';
import { MindStudioManager } from '../manager';
import * as CommandIDs from '../commands';
import { MindStudioInsightTab } from './MindStudioInsight';

export interface MindStudioInvokeOptions {
  mindstudioManager: MindStudioManager;
  createdModelName?: string;
  app: JupyterFrontEnd;
}

export class MindStudioReactWidget extends ReactWidget {
  mindstudioManager: MindStudioManager;
  app: JupyterFrontEnd;

  currentMindStudioModel: MindStudio.IModel | null = null;
  createdModelName?: string;
  profilerServerId: string | null;
  private _iframeStartPromise: Promise<string> | null = null;

  constructor(options: MindStudioInvokeOptions) {
    super();
    this.mindstudioManager = options.mindstudioManager;
    this.createdModelName = options.createdModelName;
    this.app = options.app;
    this.profilerServerId = '';

    this.title.closable = true;
    this.title.label = 'MindStudio Insight';
    this.title.caption = `Name: ${this.title.label}`;
  }

  dispose(): void {
    if (this.isDisposed) {
      return;
    }
    const serverId = this.profilerServerId;
    this.profilerServerId = null;
    super.dispose();
    if (serverId) {
      void this.mindstudioManager
        .terminateIframe(serverId)
        .catch(() => undefined);
    }
  }

  closeCurrent = (): void => {
    this.dispose();
    this.close();
  };

  startIframeUrl = (): Promise<string> => {
    if (this.isDisposed) {
      return Promise.reject(
        new Error('MindStudio Insight tab has been closed.')
      );
    }
    // One request per widget, including repeated React effect executions.
    if (!this._iframeStartPromise) {
      this._iframeStartPromise = this.mindstudioManager
        .startIframeUrl()
        .then(async url => {
          const serverId = new URL(
            url,
            window.location.href
          ).searchParams.get('profilerServerId');
          if (!serverId) {
            throw new Error(
              'Invalid MindStudio Insight startup configuration.'
            );
          }
          if (this.isDisposed) {
            await this.mindstudioManager
              .terminateIframe(serverId)
              .catch(() => undefined);
            throw new Error(
              'MindStudio Insight tab has been closed.'
            );
          }
          this.profilerServerId = serverId;
          return url;
        });
    }
    return this._iframeStartPromise;
  };

  startNew = (
    name: string,
    options?: MindStudio.IOptions
  ): Promise<MindStudio.IMindStudio> => {
    return this.mindstudioManager.startNew(
      name,
      options
    );
  };

  setWidgetName = (name: string): void => {
    this.title.label = name || 'MindStudio Insight';
    this.title.caption = `Name: ${this.title.label}`;
  };

  render(): JSX.Element {
    return (
      <MindStudioInsightTab
        mindstudioManager={this.mindstudioManager}
        closeWidget={this.closeCurrent}
        openMindStudio={this.openMindStudio}
        updateCurrentModel={this.updateCurrentModel}
        startNew={this.startNew}
        startIFrame={this.startIframeUrl}
      />
    );
  };

  protected updateCurrentModel = (model: MindStudio.IModel | null): void => {
    this.currentMindStudioModel = model;
  };

  protected openMindStudio = (modelName: string): void => {
    this.app.commands.execute(CommandIDs.open, {
      modelName,
    });
  };
}
