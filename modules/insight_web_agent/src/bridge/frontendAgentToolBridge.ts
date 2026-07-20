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
import { FrontendAgentToolBridgeClient, TOOL_OBSERVE } from '@insight/lib/FrontendAgentToolBridge';

/**
 * AcpSession iframe 侧 agent 工具调用桥接。
 *
 * 封装 `FrontendAgentToolBridgeClient`，向上层暴露具体工具调用接口。当前仅注册 `observe`
 * 工具；未来新增工具时，在 lib 的 `constants.ts` 增加工具名常量，并在此导出对应的
 * 调用函数即可。
 *
 * 不走 `ClientConnector.fetch`，因为 agent 工具调用是"请求/响应"语义（发起方必须
 * 等待响应），而 `connector.send` 是 fire-and-forget，`fetch` 强制 `event='request'`
 * 并配套 `ServerConnector.awaitFetch` 路由到 `requestModule`（模块数据请求），不能
 * 用于自定义工具调用契约。详见 `@insight/lib/FrontendAgentToolBridge/constants.ts` 的设计说明。
 */
const client = new FrontendAgentToolBridgeClient();

/**
 * 向 Insight framework 发起 observe 工具调用，返回当前页面的 observation。
 *
 * @throws Error 当 framework 未响应、超时、或返回错误时 reject
 */
export const observeInsightPage = (): Promise<Record<string, unknown>> =>
    client.request<Record<string, unknown>>(TOOL_OBSERVE);
