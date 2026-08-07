import { ISignal } from './utils/lumino/signaling';
import * as MindStudio from './mindstudio';
import * as staticManager from './staticManager';
import { ServerConnection } from '@jupyterlab/services';
/**
 * A mindstudio manager.
 */
export declare class MindStudioManager implements MindStudio.IManager {
    readonly serverSettings: ServerConnection.ISettings;
    getStaticConfigPromise: Promise<void>;
    private _models;
    private _mindstudios;
    private _isDisposed;
    private _isReady;
    private _readyPromise;
    private _runningChanged;
    private _statusConfig;
    /**
     * Construct a new mindstudio manager.
     */
    constructor(options?: staticManager.IOptions);
    get runningChanged(): ISignal<this, MindStudio.IModel[]>;
    get isDisposed(): boolean;
    get isReady(): boolean;
    get ready(): Promise<void>;
    dispose(): void;
    /**
     * Create an iterator over the most recent running mindstudio.
     *
     * @returns A new iterator over the running mindstudio.
     */
    running(): Array<MindStudio.IModel>;
    /**
     * Start profiler server and show mindstudio iframe
     *
     * @returns A new mindstudio url.
     */
    startIframeUrl(): Promise<string>;
    /**
     * Dispose mindstudio iframe and terminate profiler server
     */
    terminateIframe(profilerServerId: string): Promise<void>;
    /**
     * Create a new mindstudio.
     */
    startNew(name: string, options?: MindStudio.IOptions): Promise<MindStudio.IMindStudio>;
    /**
     * Shut down a mindstudio by name.
     */
    shutdown(name: string): Promise<void>;
    /**
     * Shut down all mindstudio.
     *
     * @returns A promise that resolves when all of the mindstudio are shut down.
     */
    shutdownAll(): Promise<void>;
    refreshRunning(): Promise<void>;
    private _onTerminated;
    private _onStarted;
    private _refreshRunning;
    private _getOptions;
    private _getStaticConfig;
}
