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

import { type ModuleConfig, modulesConfig, MEM_SCOPE_MODULE_NAME, ON_CHIP_MEMORY_MODULE_NAME, ACP_SESSION_SRC } from '@/moduleConfig';
import styled from '@emotion/styled';
import { SessionAction } from '@/utils/enum';
import { useTranslation } from 'react-i18next';
import { getModuleConfig } from '@/utils/Request';
import { updateSession } from '@/connection/notificationHandler';
import connector from '@/connection';
import { ACP_PORT } from '@/centralServer/websocket/defs';

const Container = styled.div`
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
    .ant-menu{
        height: 36px;
        background: ${(props): string => props.theme.bgColorLight};
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
        display: flex;
        min-height: 0;
    }
    .module-frame-area {
        flex: 1 1 auto;
        min-width: 0;
        height: 100%;
    }
    .tab-toolbar {
        height: 36px;
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding-right: 150px;
        background: ${(props): string => props.theme.bgColorLight};
    }
    .tab-toolbar .ant-menu {
        flex: 1 1 auto;
        min-width: 0;
        border-bottom: 0;
    }
    .session-toggle {
        flex: 0 0 auto;
        margin: 0 12px 0 8px;
        min-width: 88px;
    }
    .acp-session-panel {
        flex: 0 0 500px;
        width: 500px;
        height: calc(100% - 16px);
        margin-top: 16px;
        border-radius: 5px;
        background: ${(props): string => props.theme.bgColorLight};
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

const Index = observer(({ session }: { session: Session }) => {
    const { t } = useTranslation('framework', { keyPrefix: 'tabs' });
    const [scene, setScene] = useState<Scene>('Default');
    const [dataCompose, setDataCompose] = useState<Record<string, boolean>>({});
    const [activeModule, setActiveModule] = useState('Timeline');
    const [showSessionPanel, setShowSessionPanel] = useState(false);
    const [mergedModulesConfig, setMergedModulesConfig] = useState(modulesConfig);
    const prevFrameIdsRef = useRef<string[]>([]);
    const iframeLoadHandlersRef = useRef<Map<HTMLIFrameElement, () => void>>(new Map());

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
        const frames = document.querySelectorAll('iframe:not(.acp-session-panel)');
        const frameIds = Array.prototype.map.call(frames, (frame: HTMLIFrameElement) => frame.id) as string[];
        const newFrames = Array.prototype.filter.call(frames, (frame: HTMLIFrameElement) => !prevFrameIdsRef.current.includes(frame.id)) as HTMLIFrameElement[];

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
        newFrames.forEach((frame) => {
            // 设置 onload 监听器等待 iframe 加载完成
            // 如果之前已经有监听器，先清除
            if (iframeLoadHandlersRef.current.has(frame)) {
                frame.onload = null; // 清除已存在的监听器
            }
            // 创建并绑定新的监听器
            const onLoadHandler = (): void => {
                connector.send({
                    ...sendBody,
                    to: frame.id,
                });
            };
            // 保存监听器用于后续清除
            iframeLoadHandlersRef.current.set(frame, onLoadHandler);
            frame.onload = onLoadHandler;
        });
        prevFrameIdsRef.current = frameIds;
        return (): void => {
            iframeLoadHandlersRef.current.forEach((handler, frame) => {
                frame.onload = null; // 移除监听器
            });
            iframeLoadHandlersRef.current.clear(); // 清空 map
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
    const acpSessionSrc = `${ACP_SESSION_SRC}${ACP_SESSION_SRC.includes('?') ? '&' : '?'}acpPort=${ACP_PORT}`;
    return <Container>
        <div className="tab-toolbar">
            <Menu onClick={onClick} selectedKeys={[activeModule]} mode="horizontal" items={items} />
            <Button
                className="session-toggle"
                size="small"
                type={sessionToggleType}
                onClick={() => setShowSessionPanel(value => !value)}
            >
                Agent View
            </Button>
        </div>
        <div className="tab-body">
            <div className="module-frame-area">{availableModules.map(moduleConfig => (
                <iframe
                    {...moduleConfig.attributes}
                    key={`frame-${moduleConfig.name}`}
                    id={moduleConfig.name}
                    name={moduleConfig.name}
                    style={{ display: activeModule === moduleConfig.name ? 'block' : 'none' }}
                />
            ))}</div>
            <div className="acp-session-wrapper" style={{ display: showSessionPanel ? 'block' : 'none' }}>
                <iframe
                    className="acp-session-panel"
                    id="AcpSession"
                    name="AcpSession"
                    src={acpSessionSrc}
                    title="ACP Session"
                />
            </div>
        </div>
    </Container>;
});

export default Index;
