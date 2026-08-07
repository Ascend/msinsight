import * as MindStudio from './mindstudio';
import { Signal } from './utils/lumino/signaling';
import { ServerConnection } from '@jupyterlab/services';
export declare class DefaultMindStudio implements MindStudio.IMindStudio {
    readonly serverSettings: ServerConnection.ISettings;
    private _name;
    private _terminated;
    private _isDisposed;
    private _runningUrl;
    /**
     * constructor a new mindstudio
     */
    constructor(name: string, options?: MindStudio.IOptions);
    get name(): string;
    get model(): MindStudio.IModel;
    get terminated(): Signal<this, void>;
    get isDisposed(): boolean;
    dispose(): void;
    shutdown(): Promise<void>;
}
