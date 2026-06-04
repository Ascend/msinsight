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
import { loadProjectByPath } from '@/utils/index';
import { ConnectHost } from '@/centralServer/websocket/defs';

const isDev = typeof process !== 'undefined' && process.env?.NODE_ENV === 'development';
const logger = {
    debug: (...args: unknown[]) => { if (isDev) console.log('[Webview]', ...args); },
    warn: (...args: unknown[]) => console.warn('[Webview]', ...args),
    error: (...args: unknown[]) => console.error('[Webview]', ...args),
};

/**
 * 数据查看器控制器
 * 负责管理多个 iframe 的加载和渲染
 */

interface IframeData {
    content: string;
    filePath?: string;
}

interface VSCodeAPI {
    postMessage: (message: any) => Promise<void>;
}

interface WebviewMessage {
    command?: string;
    type?: string;
    pluginName?: string;
    content?: string;
    iframes?: Array<{
        pluginName: string;
        content: string;
        filePath?: string;
    }>;
    wsPort?: number;
    binPath?: string;
    data?: string;
    id?: string;
    reason?: string;
}

export function isVscodePluginEnvironment(): boolean {
    return (
        typeof window !== 'undefined' && window.acquireVsCodeApi !== undefined
    );
}

export class DataViewerController {
    private readonly vscode?: VSCodeAPI;
    private static connectFn: ((host: ConnectHost) => Promise<boolean>) | null = null;
    public connectHost: ConnectHost = {
        remote: '127.0.0.1',
        port: 9000,
    };

    // iframe 状态管理
    private readonly iframeStatus = {
        // 期望的 iframe 列表
        expectedIframes: [
            'Communication',
            'Summary',
            'CacheFrame',
            'Details',
            'Source',
            'Compute',
            'leaks',
            'Memory',
            'Operator',
            'RL',
            'Statistic',
            'Timeline',
        ],
        // 已收到的 iframe 内容缓存
        receivedIframes: new Map<string, IframeData>(),
        // 已加载的 iframe 集合（避免重复加载）
        loadedIframes: new Set<string>(),
        // 是否已发送请求
        requestSent: false,
        // 轮询定时器
        // 轮询间隔（毫秒）
    };

    // 单个 div 的轮询定时器映射（key: pluginName/id, value: timerId）

    private observer: MutationObserver | null = null;

    constructor() {
        if (!isVscodePluginEnvironment()) {
            return;
        }
        if (!(window as any).vscode) {
            (window as any).vscode = window.acquireVsCodeApi();
        }
        this.vscode = (window as any).vscode;
    }

    /**
     * 初始化控制器
     */
    public static setConnectRemote(fn: (host: ConnectHost) => Promise<boolean>): void {
        DataViewerController.connectFn = fn;
    }

    public init(): void {
        logger.debug('[Webview] ========== init is run  ==========');
        if (!isVscodePluginEnvironment()) {
            return;
        }
        logger.debug('[Webview] ========== Webview 初始化完成 ==========');

        // 监听来自扩展的消息
        window.addEventListener('message', (event) => {
            this.handleMessage(event.data);
        });

        // 页面加载完成后检测 div 是否加载成功
        window.addEventListener('DOMContentLoaded', () => {
            this.onDOMContentLoaded();
        });

        // 如果 DOMContentLoaded 已经触发，立即检测
        if (document.readyState === 'loading') {
            logger.debug(
                '[Webview] 文档状态 loading，等待 DOMContentLoaded 事件',
            );
        } else {
            logger.debug('[Webview] ========== 文档已加载完成 ==========');
            logger.debug(`[Webview] 文档状态: ${document.readyState}`);
            this.onDOMContentLoaded();
        }

        // 使用 MutationObserver 监听 DOM 变化
        this.initMutationObserver();

        // 页面卸载时清理定时器
        window.addEventListener('beforeunload', () => {
            this.cleanup();
        });
    }

    /**
     * DOMContentLoaded 事件处理
     */
    private onDOMContentLoaded(): void {
        logger.debug(
            '[Webview] ========== DOMContentLoaded 事件触发 ==========',
        );
        // 通知扩展已准备好接收消息
        this.vscode?.postMessage({ command: 'ready' });
        logger.debug('[Webview] ✅ 已发送 ready 消息到 node 端');

        // 检测 div 是否加载成功
        this.checkDivsLoaded();
    }

    /**
     * 初始化 MutationObserver
     */
    private initMutationObserver(): void {
        logger.debug('[Webview] 初始化 MutationObserver 监听 DOM 变化');
        this.observer = new MutationObserver((mutations) => {
            // 只在有实际变化时才检测，避免频繁调用
            const hasRelevantChanges = mutations.some((mutation) => {
                return (
                    mutation.type === 'childList' &&
                    mutation.addedNodes.length > 0
                );
            });
            if (hasRelevantChanges && !this.iframeStatus.requestSent) {
                logger.debug('[Webview] 检测到 DOM 变化，重新检测 div 状态');
                this.checkDivsLoaded();
            }
        });

        // 观察整个文档的变化
        this.observer.observe(document.body, {
            childList: true,
            subtree: true,
        });
        logger.debug('[Webview] ✅ MutationObserver 已启动');
    }

    /**
     * 创建空的 iframe
     */
    private createIframe(pluginName: string): HTMLIFrameElement | null {
        const container = document.getElementById(pluginName);
        if (!container) {
            logger.error(`未找到插件容器: ${pluginName}`);
            return null;
        }

        // 清除占位符
        container.innerHTML = '';

        // 创建空的 iframe
        const iframe = document.createElement('iframe');
        iframe.name = pluginName;
        iframe.src = 'about:blank';
        iframe.setAttribute('sandbox', 'allow-scripts allow-same-origin');
        iframe.style.width = '100%';
        iframe.style.height = '100%';
        iframe.style.border = 'none';
        iframe.style.display = 'block';

        container.appendChild(iframe);
        return iframe;
    }

    /**
     * 渲染 iframe 内容
     */
    private renderIframeContent(pluginName: string, content: string): void {
        logger.debug(
            `[Webview] [${pluginName}] ========== 开始渲染 iframe ==========`,
        );
        const renderStartTime = Date.now();

        const container = document.getElementById(pluginName);
        if (!container) {
            logger.error(`[Webview] [${pluginName}] ❌ 未找到插件容器`);
            return;
        }

        // 获取或创建 iframe
        let iframe: HTMLIFrameElement | null = container.querySelector(
            'iframe',
        ) as HTMLIFrameElement | null;
        if (!iframe) {
            logger.debug(`[Webview] [${pluginName}] 创建新的 iframe 元素`);
            iframe = this.createIframe(pluginName);
            if (!iframe) {
                return;
            }
        } else {
            logger.debug(`[Webview] [${pluginName}] 使用已存在的 iframe 元素`);
        }

        // 尝试写入内容的函数
        const writeContent = (): void => {
            try {
                const doc =
                    iframe.contentDocument ?? iframe.contentWindow?.document;
                if (!doc) {
                    logger.error(
                        `[Webview] [${pluginName}] ❌ 无法获取 iframe document`,
                    );
                    return;
                }

                const writeStartTime = Date.now();
                doc.open();
                doc.write(content);
                doc.close();
                const writeEndTime = Date.now();

                const renderEndTime = Date.now();
                const totalTime = renderEndTime - renderStartTime;
                logger.debug(
                    `[Webview] [${pluginName}] ✅ 成功渲染 iframe，写入耗时: ${writeEndTime - writeStartTime
                    }ms，总耗时: ${totalTime}ms`,
                );
            } catch (error) {
                logger.error(
                    `[Webview] [${pluginName}] ❌ 渲染 iframe 内容失败:`,
                    error,
                );
                container.innerHTML = `<div class="plugin-placeholder">渲染失败: ${pluginName}</div>`;
            }
        };

        // 如果 iframe 已经加载，直接写入
        try {
            const doc =
                iframe.contentDocument ?? iframe.contentWindow?.document;
            if (doc && doc.readyState === 'complete') {
                logger.debug(
                    `[Webview] [${pluginName}] iframe 已就绪，直接写入内容`,
                );
                writeContent();
            } else {
                logger.debug(
                    `[Webview] [${pluginName}] iframe 未就绪，等待加载完成 (readyState: ${doc ? doc.readyState : 'N/A'
                    })`,
                );
                // 等待 iframe 加载完成后再写入内容
                iframe.onload = () => {
                    logger.debug(
                        `[Webview] [${pluginName}] iframe onload 事件触发`,
                    );
                    writeContent();
                };
                // 如果 onload 没有触发，使用 setTimeout 重试
                setTimeout(() => {
                    const doc =
                        iframe.contentDocument ??
                        iframe.contentWindow?.document;
                    if (doc && doc.readyState === 'complete') {
                        logger.debug(
                            `[Webview] [${pluginName}] setTimeout 检测到 iframe 已就绪`,
                        );
                        writeContent();
                    } else {
                        logger.warn(
                            `[Webview] [${pluginName}] setTimeout 检测到 iframe 仍未就绪`,
                        );
                    }
                }, 100);
            }
        } catch (error) {
            logger.warn(
                `[Webview] [${pluginName}] 访问 iframe document 失败，等待 onload:`,
                error,
            );
            // 如果访问 document 失败，等待 onload
            iframe.onload = () => {
                logger.debug(
                    `[Webview] [${pluginName}] iframe onload 事件触发（异常路径）`,
                );
                writeContent();
            };
        }
    }

    /**
     * 检测 div 是否加载成功
     */

    /**
     * 尝试渲染所有已缓存但未渲染的 iframe
     */
    private tryRenderAllCachedIframes(): void {
        this.iframeStatus.receivedIframes.forEach((iframeData, pluginName) => {
            if (!this.iframeStatus.loadedIframes.has(pluginName)) {
                const container = document.getElementById(pluginName);
                if (container) {
                    logger.debug(`[Webview] [${pluginName}] ✅ 从缓存渲染，文件路径: ${iframeData.filePath ?? 'N/A'}`);
                    this.renderIframeContent(pluginName, iframeData.content);
                    this.iframeStatus.loadedIframes.add(pluginName);
                }
            }
        });
    }

    private checkDivsLoaded(): void {
        const loadedDivs: string[] = [];
        const missingDivs: string[] = [];

        this.iframeStatus.expectedIframes.forEach((pluginName) => {
            const container = document.getElementById(pluginName);
            if (container) {
                loadedDivs.push(pluginName);
            } else {
                missingDivs.push(pluginName);
            }
        });

        const allDivsLoaded = missingDivs.length === 0;

        logger.debug(
            `[Webview] 检测 div 状态 - 已加载 ${loadedDivs.length}/${this.iframeStatus.expectedIframes.length}`,
        );
        if (loadedDivs.length > 0) {
            logger.debug(`[Webview] 已加载的 div: [${loadedDivs.join(', ')}]`);
        }
        if (missingDivs.length > 0) {
            logger.debug(`[Webview] 缺失的 div: [${missingDivs.join(', ')}]`);
        }

        if (allDivsLoaded && !this.iframeStatus.requestSent) {
            logger.debug('[Webview] ========== 所有 div 已加载成功 ==========');
            logger.debug(
                `[Webview] 当前缓存状态: ${this.iframeStatus.receivedIframes.size}/${this.iframeStatus.expectedIframes.length} 个 iframe 已缓存`,
            );
            logger.debug('[Webview] 开始轮询本地缓存..');
            this.iframeStatus.requestSent = true;
            // 数据可能已缓存，立即尝试渲染
            this.tryRenderAllCachedIframes();
        } else if (!allDivsLoaded) {
            logger.debug(
                `[Webview] 等待更多 div 加载... (${loadedDivs.length}/${this.iframeStatus.expectedIframes.length})`,
            );
        } else if (this.iframeStatus.requestSent) {
            logger.debug('[Webview] 轮询已启动，跳过重复检查');
        }
    }

    /**
     * 处理来自扩展的消息
     */
    private isValidPlugin(name: string): boolean {
        return this.iframeStatus.expectedIframes.includes(name);
    }

    private handleMessage(message: WebviewMessage): void {
        // 处理统一发送所有 iframe 内容的消息
        if (message.command === 'renderAllIframes') {
            this.handleRenderAllIframes(message);
            return;
        }

        // 处理设置 bin 文件路径的消息（保留兼容性）
        if (message.command === 'setBinPath') {
            const binPathElement = document.getElementById('binPath');
            if (binPathElement && message.data) {
                binPathElement.textContent = message.data;
            }
            return;
        }

        // 处理设置 WebSocket 端口的消息（保留兼容性）
        if (message.command === 'setWsPort') {
            const wsPortElement = document.getElementById('wsPort');
            if (wsPortElement && message.data) {
                wsPortElement.textContent = message.data;
            }
            return;
        }

        // 处理单个 iframe 内容渲染消息（保留兼容性）
        if (message.command === 'renderIframe') {
            if (message.pluginName && message.content && this.isValidPlugin(message.pluginName)) {
                this.renderIframeContent(message.pluginName, message.content);
            } else {
                logger.error('renderIframe 消息缺少必要字段:', message);
            }
            return;
        }

        // 处理 WebSocket 桥接消息（来自 NodeWebviewSocketBridge）
        if (message.type?.startsWith('ws:')) {
            this.handleWebSocketMessage(message);
        }
    }

    /**
     * 处理 renderAllIframes 消息
     */
    private async handleRenderAllIframes(
        message: WebviewMessage,
    ): Promise<void> {
        if (message.wsPort) {
            this.connectHost.port = message.wsPort;
            logger.debug(
                '[handleRenderAllIframes] ========== connectRemote ==========',
                this.connectHost,
            );
            try {
                await DataViewerController.connectFn?.(this.connectHost);
            } catch (error) {
                logger.error('[handleRenderAllIframes] connectRemote failed:', error);
            }
        }

        // 将接收到的 iframe 内容存储到缓存中
        if (message.iframes && Array.isArray(message.iframes)) {
            logger.debug(
                `[Webview] 接收到 ${message.iframes.length} 个 iframe 内容，开始缓存..`,
            );
            const cacheStartTime = Date.now();

            message.iframes.forEach((iframe) => {
                if (iframe.pluginName && iframe.content && this.isValidPlugin(iframe.pluginName)) {
                    const contentSize = iframe.content.length;
                    logger.debug(
                        `[Webview] [${iframe.pluginName
                        }] ✅ 缓存成功，内容大小: ${contentSize} 字节，文件路径: ${iframe.filePath ?? 'N/A'
                        }`,
                    );
                    // 存储到缓存中
                    this.iframeStatus.receivedIframes.set(iframe.pluginName, {
                        content: iframe.content,
                        filePath: iframe.filePath,
                    });
                } else {
                    logger.warn('[Webview] ⚠️ 跳过无效的 iframe 数据:', iframe);
                }
            });

            const cacheEndTime = Date.now();
            logger.debug(
                `[Webview] ✅ 缓存完成，耗时: ${cacheEndTime - cacheStartTime}ms`,
            );
            logger.debug(
                `[Webview] 当前缓存状态: ${this.iframeStatus.receivedIframes.size}/${this.iframeStatus.expectedIframes.length} 个 iframe 已缓存`,
            );
            logger.debug(
                `[Webview] 已缓存的 iframe: [${Array.from(
                    this.iframeStatus.receivedIframes.keys(),
                ).join(', ')}]`,
            );

            // 数据到达，立即尝试渲染所有已缓存的 iframe
            this.tryRenderAllCachedIframes();
        } else {
            logger.warn('[Webview] ⚠️ 消息中没有 iframes 数组或格式不正确');
        }

        // 打印文件地址到控制台
        if (message.binPath) {
            try {
                loadProjectByPath(message.binPath);
            } catch (error) {
                logger.error('[handleRenderAllIframes] loadProjectByPath failed:', error);
            }
        }

        logger.debug(
            '[Webview] ========== renderAllIframes 消息处理完成 ==========',
        );
    }

    /**
     * 处理 WebSocket 桥接消息
     */
    private handleWebSocketMessage(message: WebviewMessage): void {
        // WebSocket 桥接消息处理
        // 这些消息来自 NodeWebviewSocketBridge，用于 WebSocket 通信
        switch (message.type) {
            case 'ws:opened':
                break;
            case 'ws:message':
                break;
            case 'ws:error':
                logger.error('WebSocket error:', message.id, message.reason);
                break;
            case 'ws:closed':
                break;
        }
    }

    /**
     * 清理资源
     */
    private cleanup(): void {
        logger.debug('[Webview] 页面即将卸载，清理资源');

        if (this.observer) {
            this.observer.disconnect();
            this.observer = null;
        }
    }

    /**
     * 公开方法：打开 WebSocket 连接（示例）
     */
    public openWebSocket(
        id: string,
        url: string,
        protocols: string[] = [],
    ): void {
        this.vscode?.postMessage({
            type: 'ws:open',
            id,
            url,
            protocols,
        });
    }

    /**
     * 公开方法：div onload 事件处理
     * 根据 div 的 id 匹配 iframe 缓存数据并渲染
     * 如果数据还未返回，则轮询本地存储等待数据到达
     *
     * @param id div 的 id，应该与 pluginName 匹配
     */
    public onDivLoad(id: string): void {
        logger.debug(
            `[Webview] [${id}] ========== div onload 事件触发 ==========`,
        );

        // 检查容器是否存在
        const container = document.getElementById(id);
        if (!container) {
            logger.error(`[Webview] [${id}] ❌ 未找到容器元素`);
            return;
        }

        // 检查容器中是否实际存在iframe元素
        const existingIframe = container.querySelector(
            'iframe',
        ) as HTMLIFrameElement | null;
        let shouldSkipLoad = false;

        // 如果iframe存在，检查是否已经加载了内容（通过检查contentDocument的状态）
        if (existingIframe) {
            try {
                const doc =
                    existingIframe.contentDocument ??
                    existingIframe.contentWindow?.document;
                // 如果iframe存在且有有效的document且有内容，说明已经加载过了
                if (
                    doc &&
                    doc.readyState === 'complete' &&
                    doc.body &&
                    doc.body.innerHTML.trim().length > 0
                ) {
                    logger.debug(
                        `[Webview] [${id}] iframe已存在且已加载内容，跳过重复加载`,
                    );
                    // 确保标记为已加载（可能在重新渲染后丢失）
                    if (!this.iframeStatus.loadedIframes.has(id)) {
                        this.iframeStatus.loadedIframes.add(id);
                    }
                    shouldSkipLoad = true;
                } else {
                    // iframe存在但未加载内容或内容为空，需要重新加载
                    logger.debug(
                        `[Webview] [${id}] iframe存在但未加载内容，需要重新加载`,
                    );
                }
            } catch (e) {
                // 跨域或其他错误，无法检查内容，但iframe存在，假设已加载
                logger.debug(
                    `[Webview] [${id}] iframe已存在但无法检查内容状态（可能是跨域限制），跳过加载`,
                );
                if (!this.iframeStatus.loadedIframes.has(id)) {
                    this.iframeStatus.loadedIframes.add(id);
                }
                shouldSkipLoad = true;
            }
        } else {
            // iframe不存在，如果之前标记为已加载，说明div被重新渲染了，需要清除标记
            if (this.iframeStatus.loadedIframes.has(id)) {
                logger.debug(
                    `[Webview] [${id}] 检测到div被重新渲染，iframe已不存在，清除旧标记并重新加载`,
                );
                this.iframeStatus.loadedIframes.delete(id);
            }
        }

        // 如果需要跳过加载，直接返回
        if (shouldSkipLoad) {
            return;
        }

        // 检查缓存中是否有数据
        if (this.iframeStatus.receivedIframes.has(id)) {
            const iframeData = this.iframeStatus.receivedIframes.get(id);
            if (iframeData) {
                logger.debug(
                    `[Webview] [${id}] ✅ 缓存中已有数据，立即渲染，文件路径: ${iframeData.filePath ?? 'N/A'
                    }`,
                );
                this.renderIframeContent(id, iframeData.content);
                // 标记为已加载
                this.iframeStatus.loadedIframes.add(id);
                logger.debug(`[Webview] [${id}] ✅ 渲染完成`);
            }
        } else {
            logger.debug(`[Webview] [${id}] 缓存中暂无数据，等待数据到达后自动渲染`);
        }
    }
}

// 在全局作用域中初始化控制器
declare global {
    interface Window {
        acquireVsCodeApi: () => VSCodeAPI;
        DataViewerController: typeof DataViewerController;
    }
}

if (typeof window !== 'undefined') {
    window.DataViewerController = DataViewerController;

    // 如果 acquireVsCodeApi 可用，自动初始化
    if (isVscodePluginEnvironment()) {
        const controller = new DataViewerController();
        // 将控制器暴露到全局，以便调用
        (window as any).dataViewerController = controller;
    }
}

const initializer = new DataViewerController();
export function pluginModeInit(): void {
    initializer.init();
}

export function onDivLoad(id: string): void {
    initializer.onDivLoad(id);
}

export function setConnectRemoteFn(fn: (host: ConnectHost) => Promise<boolean>): void {
    DataViewerController.setConnectRemote(fn);
}

export function getConnectHost(): ConnectHost {
    return initializer.connectHost;
}
