import { DefaultMindStudio } from './defaultMindStudio';
/**
 * A namespace for private data.
 */
/**
 * A mapping of running mindstudio by url.
 */
export declare const running: {
    [key: string]: DefaultMindStudio;
};
/**
 * Get the url for a mindstudio.
 */
export declare function getMindStudioUrl(baseUrl: string, name: string): string;
/**
 * Get the url for a mindstudio.
 */
export declare function getMindStudioStaticConfigUrl(baseUrl: string): string;
/**
 * Get the base url.
 */
export declare function getServiceUrl(baseUrl: string): string;
/**
 * Kill mindstudio by url.
 */
export declare function killMindStudio(url: string): void;
/**
 * Get the iframe config url.
 */
export declare function getIFrameConfigUrl(baseUrl: string): string;
/**
 * Terminate profiler server.
 */
export declare function terminateIframe(baseUrl: string, profilerServerId: string): string;
export declare function getMindStudioInstanceRootUrl(baseUrl: string): string;
export declare function getMindStudioInstanceUrl(baseUrl: string, proxy: boolean, port: string, profilerServerId: string, acpPort?: string, acpCapabilityToken?: string): string;
