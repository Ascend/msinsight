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

/**
 * Frontend agent tool bridge 的事件名常量。
 *
 * 设计说明：
 * 这套事件不走 `ClientConnector` / `ServerConnector`，因为 agent 工具调用是
 * "请求/响应"语义（发起方必须等待响应），而 connector 的 `send` 是 fire-and-forget
 * （返回 void，不等待响应），`fetch` 虽等待响应但强制 `event='request'` 并配套
 * `ServerConnector.awaitFetch` 路由到 `requestModule`（模块数据请求），不能用于
 * 自定义工具调用契约。因此工具调用走独立 bridge，用自定义 `requestId` 关联请求/响应。
 *
 * 事件名采用 `insightWebAgent/tool/*` 泛化命名，通过消息体的 `tool` 字段区分
 * 具体工具（如 `observe`），支持未来注册更多工具而不必新增事件名。
 */

/** iframe → parent 的工具调用请求事件 */
export const AGENT_TOOL_REQUEST = 'insightWebAgent/tool/request';
/** parent → iframe 的工具调用成功响应事件 */
export const AGENT_TOOL_RESPONSE = 'insightWebAgent/tool/response';
/** parent → iframe 的工具调用失败响应事件 */
export const AGENT_TOOL_ERROR = 'insightWebAgent/tool/error';

/** 已注册的工具名常量，便于两侧统一引用 */
export const TOOL_OBSERVE = 'observe';

/** 默认请求超时时间（毫秒） */
export const DEFAULT_TOOL_REQUEST_TIMEOUT_MS = 3000;
