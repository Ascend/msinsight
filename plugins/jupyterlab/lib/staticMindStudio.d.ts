import * as MindStudio from './mindstudio';
import { ServerConnection } from '@jupyterlab/services';
/**
 * The static namespace for DefaultMindStudio
 */
/**
 * Start a new mindstudio.
 *
 * @param name
 * @param options - The mindstudio options to use.
 *
 * @returns A promise that resolves with the mindstudio instance.
 */
export declare function startNew(name: string, options?: MindStudio.IOptions): Promise<MindStudio.IMindStudio>;
export declare function getStaticConfig(settings?: ServerConnection.ISettings): Promise<MindStudio.IStaticConfig>;
/**
 * List the running mindstudio.
 *
 * @param settings - The server settings to use.
 *
 * @returns A promise that resolves with the list of running mindstudio models.
 */
export declare function listRunning(settings?: ServerConnection.ISettings): Promise<MindStudio.IModel[]>;
/**
 * Shut down a mindstudio by name.
 *
 * @param name - Then name of the target mindstudio.
 *
 * @param settings - The server settings to use.
 *
 * @returns A promise that resolves when the mindstudio is shut down.
 */
export declare function shutdown(name: string, settings?: ServerConnection.ISettings): Promise<void>;
/**
 * Shut down all mindstudio.
 *
 * @param settings - The server settings to use.
 *
 * @returns A promise that resolves when all the mindstudio are shut down.
 */
export declare function shutdownAll(settings?: ServerConnection.ISettings): Promise<void>;
/**
 * According mindstudio's name to get mindstudio's url.
 */
export declare function startIframeUrl(settings?: ServerConnection.ISettings): Promise<string>;
/**
 * Terminate mindstudio profiler server by iframe's url.
 */
export declare function terminateIframe(profilerServerId: string, settings?: ServerConnection.ISettings): Promise<void>;
