import { ServerConnection } from '@jupyterlab/services';
/**
 * The namespace for MindStudioManager statics.
 */
export interface IOptions {
    /**
     * The server settings used by the manager.
     */
    serverSettings?: ServerConnection.ISettings;
}
