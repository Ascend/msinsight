# Insight Agent Bridge 设计文档

> 适用范围：`modules/insight_web_agent/`（前端 `src/` + 后端 `server/`）。
> 文档维护者：`insight-web-agent` 模块作者。
> 对应代码版本：`insight-agent-bridge-impl` 分支。

## 1. 概述

Insight Agent Bridge 是一个将外部 Agent（OpenCode、Claude、Trae 等）以 ACP（Agent Control Protocol）形态嵌入到 MindStudio Insight "Agent Panel" 的桥梁。它包含：

- 一个 **HTTP + SSE 服务**，由 Node ESM runtime 启动，承载聊天、agent config、session、permission 等能力；
- 一个 **ACP Adapter**，作为 host ↔ agent 的双向 JSON-RPC 桥接；
- 一个 **前端 TypeScript 应用**（React + Antd），提供 chat panel、Agent Settings dialog、permission approval UI。

服务入口：

```text
modules/insight_web_agent/server/index.mjs
```

前端入口：

```text
modules/insight_web_agent/src/index.tsx
```

构建产物：

| 资源 | 路径 |
| --- | --- |
| 前端 | `modules/insight_web_agent/build/` |
| 后端 | `modules/insight_web_agent/dist-server/index.mjs`（esbuild bundle） |
| App 内集成位置 | `<MindStudioInsight.app>/Contents/MacOS/resources/profiler/server/insight_web_agent/` |

> 启动与调试说明见 [`docs/zh/developer_guide/development_guide.md` § 3.6](../developer_guide/development_guide.md#36-insight-web-agent-acp-server-单独拉起与调试)。

## 2. 运行时拓扑

```text
┌──────────────────┐ WS/HTTP ─────────────────┐
│  MindStudio      │ profiler_server          │
│  Insight.app     │ (Rust + C++)            │
│  (Tauri shell)   │  --wsPort=9000          │
└────────┬─────────┘                          │
         │ spawns node process                │
         ▼                                    │
┌──────────────────────────────────────────┐ │
│ insight_web_agent (Node ESM)              │ │
│  HTTP 127.0.0.1:<port>                    │ │
│  ├─ /api/*  JSON  接口                    │◀┘
│  ├─ /api/events  SSE 事件流
│  ├─ /api/prompt /api/permissions/respond
│  └─ ACP adapter (stdin/stdout JSON-RPC)
└─────┬────────────────────────────────────┘
      │ stdin/stdout (NDJSON / JSON-RPC)
      ▼
┌────────────────────────┐
│  ACP agent (外部进程)  │
│  例：opencode/claude   │
└────────────────────────┘
```

约束：

- 一个 `insight_web_agent` 实例同一时刻只持有一个 active ACP adapter；切换 agent 时会执行 reload（断开旧 adapter、新建新 adapter，失败回滚到旧 runtime）。
- 主进程（`profiler_server` 通过 `--path .../insight_web_agent`）按需拉起或复用该 Node 进程；端口由启动参数决定，App 默认会从 9001/9002/9003/... 顺序挑。

## 3. 配置与持久化

runtime 启动时按以下顺序加载：

1. 命令行：`--path <runtimeDir>`、`--port <port>`
2. 环境变量：

   | 变量 | 含义 | 默认 |
   | --- | --- | --- |
   | `ACP_ROOT` | 等价 `--path` | 推导自入口目录 |
   | `ACP_AGENT` | 强制指定 active agent name | 取 `agent-servers.json:activeAgent` |
   | `PORT` | 监听端口 | `9090` |
   | `ACP_CWD` | agent workspace 根目录 | `<rootDir>/agent-workspace` |
   | `ACP_DEBUG` | 调试日志（`"1"` 打开） | `false` |
   | `ACP_MODEL` | 模型默认值 | 由 agent 自身决定 |
   | `ACP_REQUEST_TIMEOUT_MS` | 普通 ACP 请求超时 | `30000` |
   | `ACP_PROMPT_REQUEST_TIMEOUT_MS` | `session/prompt` 超时 | `5 * 60 * 1000` |
   | `ACP_PERMISSION_REQUEST_TIMEOUT_MS` | 权限审批超时 | `5 * 60 * 1000` |

3. 文件：

   ```text
   <rootDir>/agent-servers.json          # 必需
   <rootDir>/acp-session-conf.json       # 可选
   <rootDir>/prompts/system.md           # 可选：注入 system prompt
   <rootDir>/docs                        # 可选：暴露给 allowlist
   ```

`agent-servers.json` 示例：

```json
{
  "activeAgent": "OpenCode",
  "agentServers": [
    {
      "name": "OpenCode",
      "command": "opencode",
      "args": ["acp"],
      "env": { "ACP_DEBUG": "1" }
    }
  ]
}
```

`acp-session-conf.json` 示例：

```json
{
  "requestTimeoutMs": 30000,
  "promptRequestTimeoutMs": 300000,
  "permissionRequestTimeoutMs": 300000,
  "defaultAllowlist": {
    "includeDocsRoot": true,
    "includeAgentWorkspaceRoot": true,
    "includeProjectRoot": false,
    "extraPaths": []
  }
}
```

## 4. HTTP 接口契约

所有路由在 `modules/insight_web_agent/server/http/router.mjs` 内集中注册，并以 `127.0.0.1:<port>` 监听。

通用约定：

- 所有请求/响应均为 `application/json`，UTF-8。
- `OPTIONS` 总是返回 `204`，允许跨源（默认放开 `*`）。
- 成功响应：`HTTP 200`，body 为业务字段；`eventBus` 客户端通过 `/api/events` 推送。
- 错误响应：以下章节会逐条标注 HTTP 状态码与 `error` 字段。
- SSE：`/api/events` 走 `text/event-stream`，事件由 `eventBus.broadcast` 派发。
- CORS 默认放开 `*`，由 `server/http/response.mjs: applyCors` 设置。

### 4.1 `GET /api/state`

返回当前 chat state 快照（用于前端初始化或刷新）。

**响应 200：**

```json
{
  "initialized": true,
  "agentServers": [{ "name": "OpenCode" }],
  "activeAgentName": "OpenCode",
  "agentInfo": { "name": "OpenCode", "version": "1.0.0" },
  "agentCapabilities": { "fs": { "readTextFile": true } },
  "availableCommands": [],
  "availableSkills": [{ "name": "explain", "description": "..." }],
  "configOptions": [],
  "activeContext": { "profileId": null, "activeModule": null, "selection": null, "projectRoot": null }
}
```

字段语义：

| 字段 | 含义 |
| --- | --- |
| `initialized` | adapter `initialize` 是否成功 |
| `agentServers` | 当前保存的所有 agent 名列表 |
| `activeAgentName` | 当前 active agent 名 |
| `agentInfo` | adapter initialize 返回的 `agentInfo` / `agent_info` |
| `agentCapabilities` | adapter initialize 返回的 capability 摘要（仅公开字段） |
| `availableCommands` | 由 `session/update:available_commands_update` 推送的命令列表 |
| `availableSkills` | skills service 加载的技能摘要 |
| `configOptions` | agent 暴露的可配置项（如 model、mode） |
| `activeContext` | 通过 `POST /api/context` 设置的隐藏上下文 |

### 4.2 `GET /api/agent-config`

读取运行时 agent server 与 session config 快照；用于前端打开 "Agent Settings" dialog。

**响应 200：**

```json
{
  "snapshot": {
    "activeAgentName": "OpenCode",
    "agentServers": [
      {
        "name": "OpenCode",
        "command": "opencode",
        "args": ["acp"],
        "env": { "ACP_DEBUG": "1" }
      }
    ],
    "sessionConfig": {
      "requestTimeoutMs": 30000,
      "promptRequestTimeoutMs": 300000,
      "permissionRequestTimeoutMs": 300000,
      "defaultAllowlist": {
        "includeDocsRoot": true,
        "includeAgentWorkspaceRoot": true,
        "includeProjectRoot": true,
        "extraPaths": []
      }
    }
  }
}
```

### 4.3 `PUT /api/agent-config`

保存 agent server 与 session config 快照，并 reload runtime。原子写：先写 `.tmp` 再 rename，失败回滚。

**请求 body：**

```json
{
  "activeAgentName": "Trae",
  "agentServers": [
    {
      "name": "OpenCode",
      "command": "opencode",
      "args": ["acp"],
      "env": { "ACP_DEBUG": "1" }
    },
    {
      "name": "Trae",
      "command": "traecli",
      "args": ["serve", "acp"],
      "env": { "TRAECLI_F": "trae-lt-13e1" }
    }
  ],
  "sessionConfig": {
    "requestTimeoutMs": 30000,
    "promptRequestTimeoutMs": 300000,
    "permissionRequestTimeoutMs": 300000,
    "defaultAllowlist": {
      "includeDocsRoot": true,
      "includeAgentWorkspaceRoot": true,
      "includeProjectRoot": true,
      "extraPaths": []
    }
  }
}
```

**响应 200（保存并 reload 成功）：**

```json
{
  "ok": true,
  "snapshot": {
    "activeAgentName": "Trae",
    "agentServers": [ /* 规范化后的 agent 列表 */ ],
    "sessionConfig": { /* 同上结构 */ }
  }
}
```

**响应 400（`validation_failed`）：**

```json
{
  "error": "validation_failed",
  "message": "agent args cannot be empty",
  "details": [
    { "field": "agentServers.0.args", "message": "agent args cannot be empty" }
  ]
}
```

**响应 409（`agent_busy`）：** 当前存在 `pendingPrompt` 或 `pendingPermission`，建议前端禁用 Save。

```json
{ "error": "agent_busy", "message": "Agent is busy" }
```

**响应 500（`config_write_failed`）：** 磁盘写失败；本接口在写失败时不会触碰 runtime。

```json
{ "error": "config_write_failed", "message": "<底层错误>" }
```

**响应 500（`reload_failed`）：** 写盘成功但 reload runtime 时初始化/连接失败。此时已 saved，但 runtime 已被回滚到旧配置；前端可提示用户"重新编辑或稍后重试"。

```json
{
  "error": "reload_failed",
  "message": "<底层错误>",
  "saved": true
}
```

校验规则（来自 `services/agentConfigService.mjs`）：

- `activeAgentName` 必填且必须是 `agentServers` 中某项的 `name`。
- 每个 agent 需要有 `name`（全局唯一，不可与已存在 agent 重名）和 `command`。
- `args` 必须是非空字符串数组，不允许空串。
- `env` 必须是对象；key 必须唯一且非空。
- `requestTimeoutMs` / `promptRequestTimeoutMs` / `permissionRequestTimeoutMs` 必须是正有限数。
- `defaultAllowlist` 的 include flags 必须是 `boolean`，`extraPaths` 必须是非空字符串数组。
- 已存在 agent 不能被删除或重命名（避免破坏已开 session 的状态）。

### 4.4 `GET /api/sessions`

列出当前 agent 的所有 session（远程会话 + 本地缓存会话的并集）。

**响应 200：**

```json
{
  "sessions": [
    {
      "sessionId": "0a91...",
      "title": "Implement agent config",
      "pendingPrompt": false,
      "updatedAt": "2026-07-04T02:24:35.000Z"
    }
  ]
}
```

> 字段名透传 agent 自身；缺字段时由 service 兜底填充。

### 4.5 `POST /api/sessions/new`

**请求 body（可选）：**

```json
{ "mode": "code" }
```

**响应 200：**

```json
{
  "ok": true,
  "sessionId": "0a91...",
  "messages": [],
  "pendingPrompt": false,
  "configOptions": []
}
```

**响应 500：**

```json
{ "error": "<message>", "status": 500 }
```

### 4.6 `POST /api/sessions/load`

加载指定 session（含初始 messages、`pendingPrompt`、`configOptions`）。

**请求 body：**

```json
{ "sessionId": "0a91..." }
```

**响应 200：**

```json
{
  "ok": true,
  "sessionId": "0a91...",
  "messages": [],
  "pendingPrompt": false,
  "configOptions": []
}
```

**响应 400：** 缺 `sessionId`。

**响应 5xx：** 实际响应状态码与错误信息由 `session/load` 或 `session/resume` 的失败决定；典型值为 500。

### 4.7 `POST /api/sessions/delete`

删除指定 session。

**请求 body：**

```json
{ "sessionId": "0a91..." }
```

**响应 200：** `{ "ok": true }`
**响应 400：** `{ "error": "sessionId is required", "status": 400 }`
**响应 409：** `{ "error": "cannot delete a session while prompting", "status": 409 }` 或 `{ "error": "delete session is not supported by this agent", "status": 409 }`

### 4.8 `POST /api/session-config/model`

**请求 body：**

```json
{ "sessionId": "0a91...", "model": "sonnet" }
```

`sessionId` 可省略代表设置全局默认 model。

**响应 200：** `{ "ok": true, "configOptions": [...] }`
**响应 400/409：** 缺 model / model 不可用 / agent 不支持 config option。

### 4.9 `POST /api/session-config/mode`

**请求 body：**

```json
{ "sessionId": "0a91...", "mode": "free_chat" }
```

**响应 200：** `{ "ok": true }`
**响应 400/409：** 缺 sessionId / 缺 mode / mode config option 不可用。

### 4.10 `GET /api/agents`

**响应 200：**

```json
{
  "activeAgentName": "OpenCode",
  "agentServers": [{ "name": "OpenCode" }, { "name": "Trae" }]
}
```

仅返回名字，不返回 `command/args/env`，避免敏感信息泄露。完整的配置请使用 `GET /api/agent-config`。

### 4.11 `POST /api/agents/switch`

运行时切换 active agent。会触发 `reloadRuntime`，失败时回滚到旧 adapter。

**请求 body：**

```json
{ "name": "Trae" }
```

**响应 200：** `{ "ok": true, "activeAgentName": "Trae", "agentServers": [{"name": "Trae"}, ...] }`
**响应 400：** `{ "error": "agent is unavailable", "status": 400 }`
**响应 500：** `{ "error": "<底层错误>", "status": 500 }`

> 同一 name 重复切换时直接返回当前 snapshot，不触发 reload。

### 4.12 `POST /api/prompt`

向当前 agent 发送 prompt。

**请求 body：**

```json
{
  "text": "Implement agent config save endpoint",
  "sessionId": "0a91...",
  "newSession": false,
  "images": [{ "id": "...", "name": "diagram.png", "data": "<base64>", "mimeType": "image/png" }],
  "mode": "free_chat",
  "hiddenContext": { "profileId": "...", "selection": [...] }
}
```

字段：

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `text` | 否（与 images/skills 至少其一时必填） | 用户输入文本 |
| `sessionId` | 看场景 | 不填时强制 `newSession: true` |
| `newSession` | 否 | true 时会先创建一个新 session |
| `images` | 否 | 附件图片，最多 base64 编码 |
| `mode` | 否 | 配合 `newSession` 设置 mode |
| `hiddenContext` | 否 | 由 `POST /api/context` 设置后再 inject 进 prompt |

**响应 200：** `{ "ok": true, "sessionId": "0a91..." }`
**响应 400：** message 空 / sessionId 缺失等。
**响应 409：** 同会话已有 `pendingPrompt`，返回 `{ "error": "another prompt is running", "status": 409 }`。
**响应 503：** agent 未初始化：`{ "error": "<agentError>", "status": 503 }`。

prompt 响应是"提交即返回"，实际回复通过 `/api/events` SSE 流推送。

### 4.13 `POST /api/cancel`

**请求 body：**

```json
{ "sessionId": "0a91..." }
```

**响应 200：** `{ "ok": true }`
错误：缺少 `sessionId` 时返回 `400`。

### 4.14 `POST /api/context`

更新 `state.activeContext`，并通过 SessionManager（如果存在）推送给 agent。

**请求 body：**

```json
{
  "profileId": "perf-debug",
  "activeModule": "compute",
  "selection": [{ "id": "row-1", "kind": "kernel" }],
  "projectRoot": "/Users/me/project"
}
```

**响应 200：**

```json
{
  "ok": true,
  "activeContext": { "profileId": "perf-debug", "activeModule": "compute", "selection": [...], "projectRoot": "..." }
}
```

### 4.15 `POST /api/permissions/respond`

响应 agent 提出的权限请求（来源为 `session/request_permission` / `fs/read_text_file`）。

**请求 body：**

```json
{
  "sessionId": "0a91...",
  "requestId": "uuid",
  "decision": "allow_once" | "allow_always" | "deny"
}
```

**响应 200：**

```json
{ "ok": true, "requestId": "uuid", "state": "allowed_once" }
```

**响应 400：** `{ "error": "malformed permission response", "status": 400 }`
**响应 404：** `{ "error": "permission request not found", "status": 404 }`
**响应 409：** `{ "error": "permission request already resolved", "status": 409 }`

### 4.16 `GET /api/events`

SSE 事件流。客户端通过 `EventSource` 订阅，服务端会持续推送：

```text
data: {"type":"state","state":{...}}
data: {"type":"message_added","sessionId":"...","message":{...}}
data: {"type":"message_removed","sessionId":"...","id":"..."}
data: {"type":"prompt_status","sessionId":"...","pendingPrompt":true}
data: {"type":"permission_request","sessionId":"...","requestId":"...","path":"...","actions":["allow_once","allow_always","deny"]}
data: {"type":"permission_resolved","sessionId":"...","requestId":"...","state":"denied"}
```

事件定义在 `server/state/eventBus.mjs` 与 `server/services/*`。

### 4.17 错误码约定

| error | 出现接口 | HTTP | 触发条件 |
| --- | --- | --- | --- |
| `validation_failed` | `PUT /api/agent-config` | 400 | body 不满足校验规则；带 `details: [{field, message}]` |
| `agent_busy` | `PUT /api/agent-config` | 409 | 存在 `pendingPrompt` 或 pending permission |
| `config_write_failed` | `PUT /api/agent-config` | 500 | 文件系统写入失败（`.tmp` → rename 失败），未触碰 runtime |
| `reload_failed` | `PUT /api/agent-config` | 500 | 写盘成功但 `reloadRuntime` 失败；运行时会回滚到旧配置；带 `saved: true` |
| `agent is unavailable` | `POST /api/agents/switch` | 400 | 请求的 agent name 不在 agentServers 中 |
| `another prompt is running` | `POST /api/prompt` | 409 | 同一 session 已有 pendingPrompt |
| `agent is not initialized` | `POST /api/prompt` | 503 | adapter 还未 initialize 成功 |

## 5. SSE 事件分类

事件类型（`type` 字段）：

| 事件 | 触发位置 | 主要 payload |
| --- | --- | --- |
| `state` | reload 完成、broadcastState 等 | `{ state }` |
| `message_added` | 用户/助手消息追加 | `{ sessionId, message }` |
| `message_removed` | 删除空 assistant 占位 | `{ sessionId, id }` |
| `prompt_status` | prompt 开始/结束 | `{ sessionId, pendingPrompt }` |
| `permission_request` | 服务端发起权限审批 | `{ sessionId, requestId, path, actions }` |
| `permission_resolved` | 审批结果回执 | `{ sessionId, requestId, state }` |
| `config_option_update` | agent 推送 config option | `{ sessionId, configOptions }` |

`state` 事件被频繁使用，前端在 onMount 时应当先调一次 `GET /api/state` 再订阅 SSE，避免丢失更新。推送的内容即为 `publicState(state)` —— 见 § 4.1 字段语义。

## 6. Reload 生命周期

当 `PUT /api/agent-config` 或 `POST /api/agents/switch` 触发 `reloadRuntime` 时，服务会做：

1. 克隆当前 config 与运行时 state 作为 previous snapshot。
2. 解析新 active agent，并通过 `createActiveAcpAdapter({ autoConnect: false })` 实例化 next adapter。
3. 调用 `chatService.initialize({ targetAdapter: nextClient })` 发送 `initialize` 请求：
   - **失败** → 回滚 config / state / messages，disconnect nextClient，返回 `reload_failed`。
   - **成功** → 切换 active adapter，丢弃 previous adapter（先 disconnect）。
4. 重新调用 `sessionService.refreshSessions()`，最后通过 `eventBus.broadcast({ type: "state" })` 推送新 state。

要点：

- 回滚时 `permissionService.updateTimeout` / `restoreRuntimeState` 会按 previous snapshot 重置。
- `pendingPrompt` 为 true 时禁止 reload（`agent_busy` 错误码）。
- App 内 `profiler_server` 不会主动重启 `insight_web_agent`：reload 仅替换进程内 adapter，旧 Node 进程仍在跑。
  这就是为什么 "app 重出后必须重启 App" —— 老的 Node 进程持有 `dist-server/index.mjs` 的内容，仍跑未替换的代码。

## 7. 前端集成

前端通过 `modules/insight_web_agent/src/api.ts` 暴露的 helper 与后端通信：

| helper | 后端路由 |
| --- | --- |
| `fetchAgentConfig` | `GET /api/agent-config` |
| `saveAgentConfig` | `PUT /api/agent-config` |
| `requestJson<T>(method, url, body?)` | 通用 fetch；发生非 2xx 时抛 `Error`，`error.body` 包含响应 JSON |

AgentPanel、ChatPanel、AgentSettingsDialog 的设计见：

- `src/components/ChatPanel.tsx`
- `src/components/AgentSettingsDialog.tsx`
- `src/components/Composer.tsx`
- `src/components/MessageList.tsx`
- `src/hooks/useChatState.tsx`

## 8. 单独拉起与排错

参见 `docs/zh/developer_guide/development_guide.md` § 3.6 中的：

- 源码级拉起与 `--inspect` 调试
- `pnpm server:build` + `node dist-server/index.mjs`
- `build/patch-acp-server.py` 一键构建并覆盖到 `MindStudioInsight.app`
- 常见错误与排查（端口冲突、`Not found`、未初始化等）

## 9. 修订记录

| 日期 | 变更 | 作者 |
| --- | --- | --- |
| 2026-07-04 | 首发：覆盖重新设计的运行时、HTTP 接口契约、reload lifecycle | insight-web-agent |
