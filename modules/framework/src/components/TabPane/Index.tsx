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
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { observer } from 'mobx-react';
import type { Scene, Session } from '@/entity/session';
import { type MenuProps, message, Menu, Tooltip } from 'antd';
import { Button } from '@insight/lib/components';
import { safeJSONParse } from '@insight/lib/utils';

import { type ModuleConfig, modulesConfig, MEM_SCOPE_MODULE_NAME, ON_CHIP_MEMORY_MODULE_NAME } from '@/moduleConfig';
import styled from '@emotion/styled';
import { SessionAction } from '@/utils/enum';
import { useTranslation } from 'react-i18next';
import { getModuleConfig } from '@/utils/Request';
import { updateSession } from '@/connection/notificationHandler';
import connector from '@/connection';
import { getModuleFrames } from '@/connection/targetWindow';
import {
    onDivLoad,
    isVscodePluginEnvironment,
    isVscodeEnv,
} from '@/vscode-adapter';
import { WindowMessageDebugger } from './WindowMessageDebugger';
import { ACP_SESSION_MIN_WIDTH, WebAgentSessionPanel } from './WebAgentSessionPanel';
import { frontendAgentCommandController } from '@/agent/frontendAgentCommandController';

const MODULE_FRAME_MIN_WIDTH = 360;

const Container = styled.div`
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
    .ant-menu{
        height: 36px;
        background: ${(props): string => props.theme.contentBackgroundColor};
    }
    .ant-menu-item{
        height: 32px;
        line-height: 32px;
        font-weight: bold;
        color: ${(props): string => props.theme.textColorPrimary};
    }
    .ant-menu-item-selected{
        color: ${(props): string => props.theme.primaryColor};
        &:after {
            border-bottom: 1px solid ${(props): string => props.theme.primaryColor};
        }
    }
    .tab-body {
        flex-grow: 1;
        height: calc(100% - 40px);
        background: ${(props): string => props.theme.bgColorDark};
        > * {
            height: 100%;
        }
        display: flex;
        min-height: 0;
    }
    .module-frame-area {
        flex: 1 1 auto;
        min-width: ${MODULE_FRAME_MIN_WIDTH}px;
        height: 100%;
    }
    .tab-toolbar {
        height: 36px;
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding-right: 0;
        background: ${(props): string => props.theme.bgColor};
    }
    .tab-toolbar .ant-menu {
        flex: 1 1 auto;
        min-width: 0;
        border-bottom: 0;
    }
    .tab-toolbar-actions {
        display: flex;
        flex: 0 0 auto;
        align-items: center;
        margin-right: 100px;
        gap: 8px;
    }
    .session-toggle {
        flex: 0 0 auto;
        margin-right: 12px;
        min-width: 88px;
    }
    .acp-session-panel {
        width: 100%;
        height: calc(100% - 16px);
        margin-top: 16px;
        border-radius: 5px;
    }
    .acp-session-wrapper {
        position: relative;
        flex: 0 0 auto;
        min-width: ${ACP_SESSION_MIN_WIDTH}px;
        height: 100%;
    }
    iframe {
        width: 100%;
        height: 100%;
        border: 0;
        color-scheme: light;
        background: transparent;
    }
`;

export function updateDataScene(data: Record<string, any>): void {
    const scenceInfo = {
        isCluster: data.isCluster ?? false,
        isReset: data.reset ?? false,
        isIpynb: data.isIpynb ?? false,
        isBinary: data.isBinary ?? false,
        hasCachelineRecords: data.hasCachelineRecords ?? false,
        isOnlyTraceJson: data.isOnlyTraceJson ?? false,
        instrVersion: data.instrVersion ?? -1,
        isLeaks: data.isLeaks ?? false,
        isTriton: data.isTriton ?? false,
        isIE: data.isIE ?? false,
        isRL: false,
        isHybridParse: data.isCluster && data.isIE,
    };
    updateSession(scenceInfo);
}

function getActive(session: Session, scene: Scene, activeModule: string, availableModules: ModuleConfig[]): string {
    const moduleNameList = availableModules.map(config => config.name);
    if (!moduleNameList.includes(activeModule)) {
        return moduleNameList[0];
    } else {
        return activeModule;
    }
}
function isAvailable(moduleConfig: ModuleConfig, scene: Scene, dataCompose: Record<string, boolean>): boolean {
    // 根据包含某种数据，控制页签显隐
    const composeList = Object.keys(dataCompose);
    for (const name of composeList) {
        if (dataCompose[name] && Boolean((moduleConfig as any)[name])) {
            return true;
        }
    }
    return Boolean(moduleConfig[`is${scene}`]);
}

// 校验插件地址
function isAllowedIframeSrc(src: string | undefined | null): boolean {
    if (!src) return false;

    const cleanSrc = src.trim();

    // 禁止外部域名 / 协议跳转
    if (/^(https?:)?\/\//i.test(cleanSrc)) return false;

    // 禁止危险协议
    if (/^(javascript|data|vbscript|file|mailto):/i.test(cleanSrc)) return false;

    // 禁止目录穿越
    if (cleanSrc.includes('..')) return false;

    // 匹配白名单路径前缀
    return cleanSrc.startsWith('./plugins/');
}

const getModuleFrameId = (frame: HTMLIFrameElement): string => frame.id !== ''
    ? frame.id
    : frame.name !== '' ? frame.name : frame.parentElement?.id ?? '';

const Index = observer(({ session }: { session: Session }) => {
    const { t } = useTranslation('framework', { keyPrefix: 'tabs' });
    const [scene, setScene] = useState<Scene>('Default');
    const [dataCompose, setDataCompose] = useState<Record<string, boolean>>({});
    const [activeModule, setActiveModule] = useState('Timeline');
    const [showSessionPanel, setShowSessionPanel] = useState(false);
    const [showWindowMessageDebugger, setShowWindowMessageDebugger] = useState(false);
    const [mergedModulesConfig, setMergedModulesConfig] = useState(modulesConfig);
    const iframeLoadHandlersRef = useRef<Map<HTMLIFrameElement, () => void>>(new Map());
    const moduleFramesRef = useRef(new Map<string, HTMLIFrameElement>());
    const moduleBridgeCleanupRef = useRef(new Map<string, () => void>());
    const tabBodyRef = useRef<HTMLDivElement>(null);

    const availableModules = useMemo(() => mergedModulesConfig.filter(config => isAvailable(config, scene, dataCompose))
        , [scene, dataCompose, mergedModulesConfig]);
    const isLeaks = useMemo(() => availableModules.some(module => module.isLeaks && module.name === MEM_SCOPE_MODULE_NAME)
        , [availableModules]);
    const isTriton = useMemo(() => availableModules.some(module => module.isTriton && module.name === ON_CHIP_MEMORY_MODULE_NAME)
        , [availableModules]);
    const getIcon = (tabTitle: string): React.ReactElement => {
        return <Tooltip mouseEnterDelay={1} title={
            tabTitle === 'Timeline'
                ? <div style={{ padding: '1rem' }}>
                    <div>{t('TimelineSystemTooltip')}</div>
                    <div style={{ marginTop: '2rem' }}>{t('TimelineOperatorTooltip')}</div>
                    <div style={{ marginTop: '2rem' }}>{t('TimelineServiceTooltip')}</div>
                </div>
                : <div style={{ padding: '1rem' }}>{t(`${tabTitle}Tooltip`)}</div>
        }>
            <span>{t(tabTitle, { defaultValue: tabTitle })}</span>
        </Tooltip>;
    };
    const items: MenuProps['items'] = useMemo(() => {
        const modules = availableModules.map(config => ({
            label: getIcon(config.name),
            key: config.name,
        }));
        if (isLeaks) {
            return modules.filter(module => module.key === MEM_SCOPE_MODULE_NAME);
        }
        if (isTriton) {
            return modules.filter(module => module.key === ON_CHIP_MEMORY_MODULE_NAME);
        }
        return modules;
    }, [availableModules, t]);
    const onClick: MenuProps['onClick'] = e => {
        setActiveModule(e.key);
    };

    useEffect(() => {
        return () => {
            moduleBridgeCleanupRef.current.forEach((cleanup) => cleanup());
            moduleBridgeCleanupRef.current.clear();
            moduleFramesRef.current.clear();
        };
    }, []);

    useEffect(() => {
        frontendAgentCommandController.setActiveModule(activeModule);
        connector.send({
            event: 'moduleActive',
            to: activeModule,
            body: {
                moduleName: activeModule,
            },
        });
    }, [activeModule]);

    useEffect(() => {
        const fetchModuleConfigData = async (): Promise<void> => {
            try {
                const { configs }: any = await getModuleConfig();
                const pluginModulesConfig: ModuleConfig[] = [];
                (configs as string[]).forEach(item => {
                    const config: ModuleConfig = { name: '', requestName: '', attributes: {} };
                    Object.assign(config, safeJSONParse(item));
                    if (isAllowedIframeSrc(config.attributes.src)) {
                        pluginModulesConfig.push(config);
                    }
                });

                setMergedModulesConfig(prevConfig => ([
                    ...prevConfig,
                    ...pluginModulesConfig,
                ]));
            } catch (error) {
                message.error('Plugin load error');
            }
        };

        if (session.defaultConnected) {
            fetchModuleConfigData();
        }
    }, [session.defaultConnected]);

    useEffect(() => {
        // 删除工程的场景：不改变页签
        if (session.isBinary === null && session.isCluster === null) {
            return;
        }
        setScene(session.scene);
        setDataCompose({ hasCachelineRecords: session.hasCachelineRecords, isRL: session.isRL });
    }, [session.isBinary, session.isCluster, session.hasCachelineRecords, session.isOnlyTraceJson, session.isIE, session.isLeaks, session.isTriton, session.isRL, session.isHybridParse]);

    // 添加监听新的页签加载后发送当前工程
    useEffect(() => {
        const syncModuleFrames = (): void => {
            const currentFrames = new Map(getModuleFrames()
                .map(frame => [getModuleFrameId(frame), frame] as const)
                .filter(([moduleId]) => moduleId !== ''));

            // Bridge 绑定具体 iframe Window；先注销已移除或被同 ID 新 iframe 替换的旧实例。
            moduleFramesRef.current.forEach((registeredFrame, moduleId) => {
                if (currentFrames.get(moduleId) === registeredFrame) return;
                // Map.get(moduleId) 返回注册 bridge 时保存的 cleanup 函数，?.() 立即调用它以释放旧 bridge 和未完成请求。
                moduleBridgeCleanupRef.current.get(moduleId)?.();
                moduleBridgeCleanupRef.current.delete(moduleId);
                moduleFramesRef.current.delete(moduleId);
                const loadHandler = iframeLoadHandlersRef.current.get(registeredFrame);
                if (loadHandler) registeredFrame.removeEventListener('load', loadHandler);
                iframeLoadHandlersRef.current.delete(registeredFrame);
            });

            const sendBody = {
                event: 'frame/loaded',
                body: {
                    selectedFileType: session.activeDataSource.selectedFileType,
                    selectedFilePath: session.activeDataSource.selectedFilePath,
                    selectedProjectName: session.activeDataSource.projectName,
                    pageInfo: {
                        cluster: session.clusterPageInfo,
                        timeline: session.timelinePageInfo,
                    },
                },
            };
            // 再为新增或替换后的真实 DOM 实例注册 bridge，同一实例不重复注册。
            currentFrames.forEach((frame, moduleId) => {
                const onLoadHandler = (): void => {
                    connector.send({ ...sendBody, to: moduleId });
                };
                if (moduleFramesRef.current.get(moduleId) === frame) {
                    if (!iframeLoadHandlersRef.current.has(frame)) {
                        iframeLoadHandlersRef.current.set(frame, onLoadHandler);
                        frame.addEventListener('load', onLoadHandler);
                    }
                    return;
                }
                // Map.get(moduleId) 返回旧 bridge 的 cleanup 函数；注册新实例前调用它，避免同 moduleId 的两个 bridge 同时存活。
                moduleBridgeCleanupRef.current.get(moduleId)?.();
                moduleFramesRef.current.set(moduleId, frame);
                moduleBridgeCleanupRef.current.set(moduleId, frontendAgentCommandController.attachModuleFrame(moduleId, frame));
                iframeLoadHandlersRef.current.set(frame, onLoadHandler);
                frame.addEventListener('load', onLoadHandler);
            });
        };

        syncModuleFrames();
        const observer = new MutationObserver(syncModuleFrames);
        observer.observe(document.body, { childList: true, subtree: true });
        return (): void => {
            observer.disconnect();
            iframeLoadHandlersRef.current.forEach((handler, frame) => {
                frame.removeEventListener('load', handler);
            });
            iframeLoadHandlersRef.current.clear();
        };
    }, [availableModules]);

    useEffect(() => {
        const newActiveModule = getActive(session, scene, activeModule, availableModules);
        setActiveModule(newActiveModule);
    }, [scene, availableModules]);
    useEffect(() => {
        const { type, value } = session.actionListener;
        const allModuleName = mergedModulesConfig.map(module => module.name);
        if (type === SessionAction.SWITCH_ACTIVE_MODULE && allModuleName.includes(value)) {
            setActiveModule(value);
        }
    }, [session.actionListener]);

    useEffect(() => {
        if (isLeaks) {
            setActiveModule(MEM_SCOPE_MODULE_NAME);
        }
    }, [isLeaks]);

    useEffect(() => {
        if (isTriton) {
            setActiveModule(ON_CHIP_MEMORY_MODULE_NAME);
        }
    }, [isTriton]);
    const sessionToggleType = showSessionPanel ? 'primary' : 'default';
    return <Container>
        <div className="tab-toolbar">
            <Menu onClick={onClick} selectedKeys={[activeModule]} mode="horizontal" items={items} />
            <div className="tab-toolbar-actions">
                {process.env.NODE_ENV === 'development'
                    ? <Button size="small" onClick={() => setShowWindowMessageDebugger(true)}>
                        Window Messages
                    </Button>
                    : null}
                <Button
                    className="session-toggle"
                    size="small"
                    type={sessionToggleType}
                    onClick={() => setShowSessionPanel(value => !value)}
                >
                    {t('AgentView')}
                </Button>
            </div>
        </div>
        <div className="tab-body" ref={tabBodyRef}>
            <div className="module-frame-area">{availableModules.map(moduleConfig => (
                (isVscodeEnv() && isVscodePluginEnvironment())
                    ? <div
                        {...moduleConfig.attributes}
                        key={`frame-${moduleConfig.name}`}
                        id={moduleConfig.name}
                        style={{ display: activeModule === moduleConfig.name ? 'block' : 'none' }}
                        ref={(devRef) => {
                            if (devRef) {
                                onDivLoad(moduleConfig.name);
                            }
                        }}
                    />
                    : <iframe
                        {...moduleConfig.attributes}
                        key={`frame-${moduleConfig.name}`}
                        id={moduleConfig.name}
                        name={moduleConfig.name}
                        style={{ display: activeModule === moduleConfig.name ? 'block' : 'none' }}
                    />
            ))}</div>
            <WebAgentSessionPanel
                activeModule={activeModule}
                availableModules={availableModules}
                moduleFrameMinWidth={MODULE_FRAME_MIN_WIDTH}
                session={session}
                show={showSessionPanel}
                tabBodyRef={tabBodyRef}
            />
        </div>
        {process.env.NODE_ENV === 'development'
            ? <WindowMessageDebugger open={showWindowMessageDebugger} onClose={() => setShowWindowMessageDebugger(false)} />
            : null}
    </Container>;
});

export default Index;
