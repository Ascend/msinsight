import { JupyterFrontEnd } from '@jupyterlab/application';
import { ReactWidget } from '@jupyterlab/apputils';
import * as MindStudio from '../mindstudio';
import { MindStudioManager } from '../manager';
export interface MindStudioInvokeOptions {
    mindstudioManager: MindStudioManager;
    createdModelName?: string;
    app: JupyterFrontEnd;
}
export declare class MindStudioReactWidget extends ReactWidget {
    mindstudioManager: MindStudioManager;
    app: JupyterFrontEnd;
    currentMindStudioModel: MindStudio.IModel | null;
    createdModelName?: string;
    profilerServerId: string | null;
    constructor(options: MindStudioInvokeOptions);
    dispose(): void;
    closeCurrent: () => void;
    startIframeUrl: () => Promise<string>;
    startNew: (name: string, options?: MindStudio.IOptions) => Promise<MindStudio.IMindStudio>;
    setWidgetName: (name: string) => void;
    render(): JSX.Element;
    protected updateCurrentModel: (model: MindStudio.IModel | null) => void;
    protected openMindStudio: (modelName: string) => void;
}
