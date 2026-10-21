# msinsight-native

`msinsight-native` 是 MindStudio Insight 的原生 ACP Agent Server。它和 OpenCode 一样，由 `insight_web_agent` 作为普通 ACP stdio 子进程启动。

```text
Agent View frontend
  -> insight_web_agent HTTP/SSE server
     -> createAcpAdapter()
        -> node server/native-agent/index.mjs
           -> ACP JSON-RPC over stdio
           -> Vercel AI SDK runtime
```

## Agent Server 配置

`modules/insight_web_agent/agent-servers.json` 中注册了该 agent：

```json
{
  "name": "msinsight-native",
  "type": "acp-stdio",
  "command": "node",
  "args": ["server/native-agent/index.mjs"],
  "env": {
    "MSINSIGHT_NATIVE_PROVIDER": "openai-compatible",
    "MSINSIGHT_NATIVE_MODEL": ""
  }
}
```

所有正式 Runtime 的 ACP 子进程共用 `ACP_CWD` 指定的 workspace 和 `.agents/skills/` Skill 实体目录；Native 直接从该目录发现 Skill，目录不存在时回退安装包资源。Claude Code 的 `.claude/skills` 映射到该目录。`insight_web_agent` 在启动前会把 `server/native-agent/index.mjs` 解析成绝对路径。

## ACP 方法

当前 ACP server 实现了这些方法：

```text
initialize
session/new
session/list
session/load
session/resume
session/delete
session/prompt
session/cancel
session/set_config_option
```

`session/prompt` 通过 ACP notification 把助手回复流式返回给 host：

```text
session/update -> agent_message_chunk
```

Insight 会话 metadata sidecar 保存在 Native Store 下：

```text
.msinsight_native_agent/sessions/<sessionId>.jsonl
```

AI SDK runtime 单独持久化模型消息与 UI 投影：

```text
.msinsight_native_agent/ai-sdk/sessions/<sessionId>.json
```

## System Prompt 与 Primary Agent

Native 的有效 System Prompt 由三部分组成：

```text
不可覆盖的产品基础规则
+ Insight Web Agent Host System Prompt
+ 当前 Session 绑定的 Primary Agent Markdown 正文
```

产品基础规则由 `runtime/aiSdkRuntime.mjs` 固定生成；Web Server 每轮通过 `insight-system-prompt://project` resource 发送 Host Prompt，resource 缺失时 Native 兼容读取 workspace 的 `AGENTS.md` / `CLAUDE.md`；通用专项行为来自当前 Agent。Session 通过 `session/set_config_option(primaryAgent)` 在首次 Prompt 前绑定 Agent，Prompt 开始后不可切换。AI SDK runtime 在每轮调用时组合当前 Host Prompt 和 Agent 正文。

## 会话权限默认值

Ask for Approval / Full Access 保持每个会话独立保存；修改一个会话不会影响其他已有会话。最近一次成功修改的权限选择作为新对话默认值，通过会话 metadata 的 `permissionModeUpdatedAt` 恢复，重启后继续使用；没有选择记录时默认 Ask。仅加载旧会话不会改变新对话默认值。

## Runtime 行为

native agent 使用 Vercel AI SDK 的 `streamText` 完成多步 tool loop，暴露当前 Native Tool Registry 中的 `msinsight`、`Bash` 和 `skill`。Runtime 初始化或模型调用失败会明确返回错误。

## 依赖

`ai` 以及各模型 Provider SDK 都直接声明在 `modules/insight_web_agent/package.json` 中，并随 native-agent 入口静态打包。

生产打包时 `scripts/build-server.mjs` 会把 `server/native-agent/index.mjs` 单独 bundle 到 `dist-server/native-agent/index.mjs`。

## 模型配置

正式模型配置保存在可写配置目录的 `msinsight-native.json`，在 Agent Settings 选择 Native 后通过现有「Script configuration」JSON 编辑器维护。`provider`、`baseUrl`、`apiKey` 为同一服务共享配置，`model` 是新会话默认模型；可选 `models` 为模型目录，`id` 必须是该服务实际接受的模型名，`name` 为展示名（省略时显示 id）。例如以下非敏感字段：

```json
{
  "model": "service-model-a",
  "models": [
    { "id": "service-model-a", "name": "Model A" },
    { "id": "service-model-b", "name": "Model B" }
  ]
}
```

在原有 JSON 中添加上述 `models` 即可，保留原有服务与密钥字段。目录按 id 去重（保留首项），拒绝空目录、空 id/name 和不属于目录的默认模型；不内置供应商模型列表。旧配置未提供 `models` 时仅暴露原 `model`。保存仍沿用原有密钥加密、占位符和重载流程。

设置页提供「从服务获取模型列表」按钮，仅在点击时使用当前未保存的 provider/baseUrl/apiKey 请求服务，不逐模型探测、不调用推理。支持 OpenAI/OpenAI-compatible 的 `{baseUrl}/models`、Anthropic `/v1/models`（官方 version/header，最多 5 页）；默认 endpoint 与 provider 对齐，尾部 `/v1` 不重复添加。请求总超时 10 秒、总响应 1 MiB、最多 500 个候选，达到分页上限会提示手填补充。

成功仅填充草稿 `models`，原默认模型若缺失则补回；必须手动保存才生效，不切换正在运行的会话。失败保留原列表，可继续在 JSON 手动填写。新密钥沿用现有 transit seal，密钥占位符由服务端解密持久化 key；仅相同 endpoint 可复用占位凭据，修改 endpoint 后需重新输入密钥。接口复用现有鉴权/origin 保护，返回仅包含候选 id/name 和截断标记，不返回服务错误或凭据。

Host 仅通过 `MSINSIGHT_NATIVE_CONFIG_PATH` 指向其可写配置（不传递模型或 Tool schema）；Native 启动时读取目录，凭据仍通过原有私有进程环境传递。ACP `session/new` / `session/load` 返回 `id: "model"`、`category: "model"`、`type: "select"`、`currentValue` 和 `options: [{value, name}]`。Composer 复用已有模型选择器，调用：

```json
{"method":"session/set_config_option","params":{"sessionId":"<session-id>","configId":"model","value":"service-model-b"}}
```

会话 metadata 保存 `modelId`；空闲会话可以切换模型且保留 AI SDK 历史，运行中拒绝切换。旧会话仅在缺少 id 时迁移默认值；未知或已删除 id 在 load/prompt 明确报错，禁止静默回退（可显式切换到有效模型恢复）。每轮从会话解析模型，以独立 env 副本创建 SDK model，不改变共享环境或其他会话。ACP 仅返回模型 id/展示名，不返回 endpoint、provider 或凭据。仅支持同一服务的模型切换，不支持跨 provider 切换；不同模型对已有工具/消息历史的兼容性由服务负责。

下面的环境变量示例仅用于独立启动 Native；未指定配置路径时兼容单模型模式。`MSINSIGHT_NATIVE_BASE_URL` 可覆盖 provider endpoint，`openai-compatible` 必填。

### OpenAI

```text
MSINSIGHT_NATIVE_PROVIDER=openai
MSINSIGHT_NATIVE_API_KEY=...
MSINSIGHT_NATIVE_MODEL=gpt-4o-mini
```

### Anthropic

```text
MSINSIGHT_NATIVE_PROVIDER=anthropic
MSINSIGHT_NATIVE_API_KEY=...
MSINSIGHT_NATIVE_MODEL=claude-sonnet-4-6
MSINSIGHT_NATIVE_BASE_URL=http://host:port
```

### DeepSeek 服务（使用 OpenAI-compatible 协议）

```text
MSINSIGHT_NATIVE_PROVIDER=openai-compatible
MSINSIGHT_NATIVE_BASE_URL=https://api.deepseek.com/v1
MSINSIGHT_NATIVE_API_KEY=...
MSINSIGHT_NATIVE_MODEL=deepseek-chat
```

### OpenAI-compatible

```text
MSINSIGHT_NATIVE_PROVIDER=openai-compatible
MSINSIGHT_NATIVE_BASE_URL=http://host:port/v1
MSINSIGHT_NATIVE_API_KEY=...
MSINSIGHT_NATIVE_MODEL=...
```

可选 runtime 参数：

```text
MSINSIGHT_NATIVE_MAX_STEPS=100
MSINSIGHT_NATIVE_TEMPERATURE=0.2
MSINSIGHT_NATIVE_MAX_OUTPUT_TOKENS=100000
MSINSIGHT_FRONTEND_COMMAND_TIMEOUT_MS=30000
```

单次模型调用的最大输出默认为 100000 tokens（100K），可通过 `MSINSIGHT_NATIVE_MAX_OUTPUT_TOKENS` 覆盖；配置值应符合所选模型的输出上限。

## 页面观察

`insight_web_agent` 启动 ACP 子进程时会注入 host 地址：

```text
INSIGHT_WEB_AGENT_BASE_URL=http://<host>:<port>
```

固定 `msinsight({ command, args })` Tool 通过 `/api/frontend-commands/request` 创建实时浏览器请求，经 SSE 发送到 Agent View iframe，再以 `executeCommand/cancelCommand` 转发给 framework 的 `FrontendAgentCommandController`。

动态能力通过内置 `help` 发现；内置 `observe` 聚合 framework 与当前 active Module 的 observation provider。Module 使用 `ModuleAgentCommandClient.registerCommand(definition, handler)` 注册一次，并按连接代次同步完整 Command 快照。

Observation payload 必须只包含摘要和能力信息。不要暴露原始 profiling 数据、完整 MobX store、任意 DOM dump 或通用 JavaScript 执行能力。

## 工具

Host 能力通过 `tools/capabilityTools.mjs` 代理到 Native Tool Registry，包括 `msinsight` 和当前可用的配置型能力：

```text
msinsight({ command, args })
pt_snap({ args, timeoutMs? })
```

模型先调用 `msinsight.help {}` 获取页面 Command 目录，再调用 `help { command }` 获取单个 Command 的完整 schema；页面状态通过 `observe {}` 获取。配置型能力执行前必须经 ACP 用户授权，支持仅允许一次、本会话允许和拒绝。

native-agent 在 AI SDK Tool Loop 中提供：

```text
Host Capability Tools
Bash
skill
```

`skill` 使用 Native Registry 惰性加载纯指令和资源清单，不执行 `!` 内联命令、Hooks 或 Runtime Patch。`Bash` 是 Native 实现的受控前台非交互工具，执行前强制 Agent policy、产品 deny、Session 用户审批、cwd 白名单、timeout、200 KiB 输出上限、单 Session 并发和进程树取消；子进程环境会过滤密钥类变量。不提供后台 Bash。

namespaced Command 由 framework 路由到 Framework 本地 handler 或当前 active Module handler；Agent iframe 不保存 Command 目录和路由状态。

## 安全边界

native agent 不应暴露：

- 未经 Agent policy、产品硬策略和用户审批的 shell 访问；
- Session 文件系统 roots 之外的文件访问；
- 通用浏览器自动化；
- profiling 原始数据导出；
- 任意 JavaScript 执行；
- 坐标级 click/type/scroll 作为普通工具。

后续页面控制必须通过命名空间约束下的语义 Command，例如：

```text
framework.switchModule
timeline.findVisibleColoredBlocks
timeline.focusCommunicationLane
```

## 本地 smoke test

可以向子进程发送 ACP JSON-RPC line 来做最小 stdio 协议检查：

```text
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}
{"jsonrpc":"2.0","id":2,"method":"session/new","params":{}}
```

完整 prompt 测试应该先收到若干 `session/update` chunk，最后收到 `session/prompt` 的 response。
