/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { TOOL_OBSERVE } from '@insight/lib/FrontendAgentToolBridge';

import type { ModuleConfig } from '@/moduleConfig';
import type { Session } from '@/entity/session';
import { registerFrontendAgentTool, unregisterFrontendAgentTool } from '../../frontendAgentToolRegistry';

/**
 * observe 工具的上下文 getter——由调用方注入，用于读取 framework 当前状态。
 *
 * 用 getter 注入而非直接读 MobX store，是因为 `activeModule`/`availableModules`
 * 当前是 `TabPane` 的组件 state，未提升到全局 store。getter 让工具实现与状态来源
 * 解耦：未来若状态提升到 store，只需改 getter 实现，工具代码不变。
 */
export type ObserveContextGetter = () => {
    activeModule: string;
    availableModules: ModuleConfig[];
    session: Session;
};

/**
 * 注册 observe 工具。
 *
 * @param getContext 返回当前 framework 状态的 getter
 * @returns cleanup 函数，用于注销工具
 */
export function registerObserveTool(getContext: ObserveContextGetter): () => void {
    registerFrontendAgentTool(TOOL_OBSERVE, () => collectObservation(getContext()));
    return () => unregisterFrontendAgentTool(TOOL_OBSERVE);
}

/**
 * 从 framework 当前状态收集 observation，作为 agent 工具调用的响应体。
 */
function collectObservation({ activeModule, availableModules, session }: {
    activeModule: string;
    availableModules: ModuleConfig[];
    session: Session;
}): Record<string, unknown> {
    return {
        version: 1,
        collectedAt: Date.now(),
        app: {
            activeModule,
            availableModules: availableModules.map(module => module.name),
            scene: session.scene,
            loading: session.loading,
            selectedProjectName: session.activeDataSource.projectName,
            hasSelectedFile: Boolean(session.activeDataSource.selectedFilePath),
            selectedFileType: session.activeDataSource.selectedFileType,
            clusterPageInfo: {
                selectedClusterPath: session.clusterPageInfo.selectedClusterPath,
                clusterCount: session.clusterPageInfo.clusterList.length,
            },
            timelinePageInfo: {
                unitCount: session.timelinePageInfo.unitCount,
            },
        },
        module: {
            module: activeModule,
            supported: false,
        },
        availableActions: [
            {
                id: 'framework.observe',
                title: 'Observe current Insight page',
                risk: 'low',
            },
        ],
    };
}
