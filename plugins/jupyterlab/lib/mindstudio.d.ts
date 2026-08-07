import { IDisposable } from './utils/lumino/disposable';
import { ISignal } from './utils/lumino/signaling';
import { JSONObject } from './utils/lumino/coreutils';
import { ServerConnection } from '@jupyterlab/services';
/**
 * The namespace for mindstudio statics.
 */
/**
 * An interface for a mindstudio.
 */
export interface IMindStudio extends IDisposable {
    /**
     * A signal emitted when the mindstudio is shut down.
     */
    terminated: ISignal<IMindStudio, void>;
    /**
     * The model associated with the mindstudio.
     */
    readonly model: IModel;
    /**
     * Get the name of the mindstudio.
     */
    readonly name: string;
    /**
     * The server settings for the mindstudio.
     */
    readonly serverSettings: ServerConnection.ISettings;
    /**
     * Shut down the mindstudio.
     */
    shutdown: () => Promise<void>;
}
/**
 * The options for intializing a mindstudio object.
 */
export interface IOptions {
    /**
     * The server settings for the mindstudio.
     */
    serverSettings?: ServerConnection.ISettings;
}
/**
 * The server model for a mindstudio.
 */
export interface IModel extends JSONObject {
    /**
     * The name of the mindstudio.
     */
    readonly name: string;
}
export interface IStaticConfig extends JSONObject {
    /**
     * The name of the mindstudio.
     */
    readonly notebookDir: string;
}
export declare function getStaticConfig(settings?: ServerConnection.ISettings): Promise<IStaticConfig>;
export declare function startNew(name: string, options?: IOptions): Promise<IMindStudio>;
export declare function listRunning(settings?: ServerConnection.ISettings): Promise<IModel[]>;
export declare function shutdown(name: string, settings?: ServerConnection.ISettings): Promise<void>;
export declare function shutdownAll(settings?: ServerConnection.ISettings): Promise<void>;
export declare function startIframeUrl(settings?: ServerConnection.ISettings): Promise<string>;
export declare function terminateIframe(profilerServerId: string, settings?: ServerConnection.ISettings): Promise<void>;
/**
 * The interface for a mindstudio manager.
 *
 * The manager is respoonsible for maintaining the state of running
 * mindstudio.
 */
export interface IManager extends IDisposable {
    readonly serverSettings: ServerConnection.ISettings;
    runningChanged: ISignal<this, IModel[]>;
    running: () => Array<IModel>;
    startNew: (name: string, options?: IOptions) => Promise<IMindStudio>;
    shutdown: (name: string) => Promise<void>;
    shutdownAll: () => Promise<void>;
    refreshRunning: () => Promise<void>;
}
