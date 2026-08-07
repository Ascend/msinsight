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
import { ICommandPalette, showDialog, Dialog, MainAreaWidget, WidgetTracker } from '@jupyterlab/apputils';
import { ILauncher } from '@jupyterlab/launcher';
import { IMainMenu } from '@jupyterlab/mainmenu';
import { IRunningSessionManagers } from '@jupyterlab/running';
import { LabIcon } from '@jupyterlab/ui-components';
import mindstudioSvgStr from '../style/mindstudio.svg';
import { MindStudioManager } from './manager';
import * as CommandIDs from './commands';
import { MindStudioReactWidget } from './biz/widget';
const mindstudioIcon = new LabIcon({
    name: 'mindstudio-insight-jupyterlab:mindstudio',
    svgstr: mindstudioSvgStr,
});
/**
 * Initialization data for the mindstudio_insight_jupyterlab extension.
 */
const plugin = {
    id: 'mindstudio_insight_jupyterlab:plugin',
    requires: [ICommandPalette],
    optional: [ILauncher, IMainMenu, IRunningSessionManagers],
    autoStart: true,
    activate: activate,
};
export default plugin;
async function activate(app, palette, launcher, menu, runningSessionManagers) {
    const manager = new MindStudioManager();
    const namespace = 'mindstudio';
    const tracker = new WidgetTracker({
        namespace,
    });
    addCommands(app, manager, tracker, launcher, menu);
    if (runningSessionManagers) {
        await addRunningSessionManager(runningSessionManagers, app, manager);
    }
    palette.addItem({ command: CommandIDs.createNew, category: 'mindstudio' });
    return tracker;
}
function addCommands(app, manager, tracker, launcher, menu) {
    const { commands } = app;
    addOpenCommand(commands, app, manager, tracker);
    addCloseCommand(commands, tracker);
    addCreateNewCommand(commands, app);
    if (launcher) {
        launcher.add({
            rank: 2,
            command: CommandIDs.createNew,
            category: 'Other',
        });
    }
    if (menu) {
        menu.fileMenu.newMenu.addGroup([
            {
                command: CommandIDs.createNew,
            },
        ], 30);
    }
}
function addOpenCommand(commands, app, manager, tracker) {
    commands.addCommand(CommandIDs.open, {
        execute: (args) => {
            let modelName = args.modelName;
            let widget = null;
            // step1: find an opened widget
            if (!modelName) {
                widget = tracker.find((t) => {
                    return (t.content.createdModelName === undefined);
                });
            }
            return getWidget(modelName, widget, app, manager, tracker);
        },
    });
}
function getWidget(modelName, widget, app, manager, tracker) {
    if (widget) {
        app.shell.activateById(widget.id);
        return widget;
    }
    else {
        let newModelName = '';
        if (!modelName) {
            const runningMindStudios = [...manager.running()];
            for (const model of runningMindStudios) {
                newModelName = model.name;
            }
        }
        const tabReact = new MindStudioReactWidget({
            mindstudioManager: manager,
            app,
            createdModelName: newModelName,
        });
        const tabWidget = new MainAreaWidget({ content: tabReact });
        tracker.add(tabWidget);
        app.shell.add(tabWidget, 'main');
        app.shell.activateById(tabWidget.id);
        return tabWidget;
    }
}
function addCloseCommand(commands, tracker) {
    commands.addCommand(CommandIDs.close, {
        label: 'Open MindStudio Insight Doc',
        icon: (args) => (args.isPalette ? undefined : mindstudioIcon),
        execute: (args) => {
            const model = args.mindstudio;
            tracker.forEach((widget) => {
                if (widget.content.currentMindStudioModel && widget.content.currentMindStudioModel.name === model.name) {
                    widget.dispose();
                    widget.close();
                }
            });
        },
    });
}
function addCreateNewCommand(commands, app) {
    commands.addCommand(CommandIDs.createNew, {
        label: (args) => (args.isPalette ? 'New MindStudio Insight' : 'MindStudio Insight'),
        caption: 'Start a new mindstudio insight',
        icon: (args) => (args.isPalette ? undefined : mindstudioIcon),
        execute: (args) => {
            // Try to open the session panel to make it easier for users to observe more active mindstudio instances
            try {
                app.shell.activateById('jp-running-sessions');
                app.commands.execute(CommandIDs.open);
            }
            catch (e) {
                showDialog({
                    title: 'Cannot create mindstudio insight.',
                    body: 'Something wrong!',
                    buttons: [Dialog.okButton()],
                });
            }
        },
    });
}
function addRunningSessionManager(runningSessionManagers, app, manager) {
    class RunningMindStudio {
        constructor(model, runningManager) {
            this._model = model;
            this._runningManager = runningManager;
        }
        open() {
            app.commands.execute(CommandIDs.open, { modelName: this._model.name });
        }
        icon() {
            return mindstudioIcon;
        }
        label() {
            return `${this._model.name}`;
        }
        shutdown() {
            app.commands.execute(CommandIDs.close, { tb: this._model });
            return this._runningManager.shutdown(this._model.name);
        }
    }
    return manager.getStaticConfigPromise.then(() => {
        runningSessionManagers.add({
            name: 'MindStudio Insight',
            running: () => manager.running().map(model => new RunningMindStudio(model, manager)),
            shutdownAll: () => manager.shutdownAll(),
            refreshRunning: () => manager.refreshRunning(),
            runningChanged: manager.runningChanged,
        });
    });
}
//# sourceMappingURL=index.js.map