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

由于 ACP 子进程的 `cwd` 是每个 agent 独立的 workspace，`insight_web_agent` 在启动前会把 `server/native-agent/index.mjs` 解析成绝对路径。

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

## Runtime 行为

native agent 使用 Vercel AI SDK 的 `streamText` 完成多步 tool loop，暴露当前 Native Tool Registry 中的 `msinsight`、`Bash` 和 `skill`。Runtime 初始化或模型调用失败会明确返回错误。

## 依赖

`ai` 以及各模型 Provider SDK 都直接声明在 `modules/insight_web_agent/package.json` 中，并随 native-agent 入口静态打包。

生产打包时 `scripts/build-server.mjs` 会把 `server/native-agent/index.mjs` 单独 bundle 到 `dist-server/native-agent/index.mjs`。

## 模型配置

模型配置保存在当前 agent 的 `env` 中，可以在 Agent View 的 Agent Settings 页面编辑。`MSINSIGHT_NATIVE_BASE_URL` 可作为 provider endpoint 覆盖地址；`openai-compatible` 必填，其他 provider 可选。

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

### DeepSeek

```text
MSINSIGHT_NATIVE_PROVIDER=deepseek
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
MSINSIGHT_NATIVE_MAX_STEPS=12
MSINSIGHT_NATIVE_TEMPERATURE=0.2
MSINSIGHT_NATIVE_MAX_OUTPUT_TOKENS=4096
MSINSIGHT_FRONTEND_COMMAND_TIMEOUT_MS=30000
```

## 页面观察

`insight_web_agent` 启动 ACP 子进程时会注入 host 地址：

```text
INSIGHT_WEB_AGENT_BASE_URL=http://<host>:<port>
```

固定 `msinsight({ command, args })` Tool 通过 `/api/frontend-commands/request` 创建实时浏览器请求，经 SSE 发送到 Agent View iframe，再以 `executeCommand/cancelCommand` 转发给 framework 的 `FrontendAgentCommandController`。

动态能力通过内置 `help` 发现；内置 `observe` 聚合 framework 与当前 active Module 的 observation provider。Module 使用 `ModuleAgentCommandClient.registerCommand(definition, handler)` 注册一次，并按连接代次同步完整 Command 快照。

Observation payload 必须只包含摘要和能力信息。不要暴露原始 profiling 数据、完整 MobX store、任意 DOM dump 或通用 JavaScript 执行能力。

## 工具

页面执行能力只通过 `tools/msinsightTools.mjs` 注册的 `msinsight` Tool 暴露：

```text
msinsight({ command, args })
```

模型先调用 `help {}` 获取轻量目录，再调用 `help { command }` 获取单个 Command 的完整 schema；页面状态通过 `observe {}` 获取。回复中的可确认页面操作由 Host System Prompt 定义的 `<insight-action>` 文本协议承载，不属于 Native Tool。

native-agent 在 AI SDK Tool Loop 中提供：

```text
msinsight
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
