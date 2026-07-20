# msinsight-native

`msinsight-native` 是 MindStudio Insight 的原生 ACP Agent Server。它和 OpenCode 一样，由 `insight_web_agent` 作为普通 ACP stdio 子进程启动。

```text
Agent View frontend
  -> insight_web_agent HTTP/SSE server
     -> createAcpAdapter()
        -> node server/native-agent/index.mjs
           -> ACP JSON-RPC over stdio
           -> 配置完整时使用 @blade-ai/agent-sdk runtime
           -> 否则使用 deterministic fallback runtime
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

会话元数据持久化在当前 agent workspace 下：

```text
.msinsight-native/sessions.json
```

Blade runtime 的会话存储使用：

```text
.msinsight-native/blade/
```

因此 native-agent 子进程重启后，`session/list`、`session/load`、`session/resume` 仍能找到历史 ACP session，并尝试通过 Blade sessionId 恢复 LLM 上下文。

## System Prompt

`insight_web_agent` 会把 `prompts/system.md` 作为以下 ACP resource block 随 prompt 发送：

```text
insight-system-prompt://project
```

native-agent 会提取该 resource，并与自身的工具和安全规则组合成 Blade system prompt。组合后的内容随 session 持久化；如果 host system prompt 发生变化，native-agent 会关闭旧 Blade session 并按新规则创建 session。

## Runtime 行为

native agent 采用两级 runtime 策略：

```text
1. Blade runtime
   当 @blade-ai/agent-sdk 已安装，并且模型环境变量配置完整时使用。

2. Fallback runtime
   当 SDK 缺失、模型 provider 未配置，或 Blade runtime 创建失败时使用。
```

Fallback runtime 只用于诊断，不自己实现 LLM tool loop。它仍会调用 `msinsight_observe`，并返回最新页面 observation，以及未使用 Blade 的原因。

## 依赖

`@blade-ai/agent-sdk` 声明在 `modules/insight_web_agent/package.json` 中：

```json
"@blade-ai/agent-sdk": "1.1.0"
```

native agent 通过 dynamic import 加载它：

```js
await import('@blade-ai/agent-sdk')
```

因此即使目标机器上没有该依赖，启动也不会失败；这种情况下 `msinsight-native` 会进入 fallback 诊断模式。

生产打包时 `scripts/build-server.mjs` 会把 `server/native-agent/index.mjs` 单独 bundle 到 `dist-server/native-agent/index.mjs`，使 SDK 进入 native-agent 产物。

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
MSINSIGHT_NATIVE_TEMPERATURE=0.2
MSINSIGHT_NATIVE_MAX_OUTPUT_TOKENS=4096
MSINSIGHT_OBSERVE_TIMEOUT_MS=5000
```

## 页面观察

`insight_web_agent` 启动 ACP 子进程时会注入 host 地址：

```text
INSIGHT_WEB_AGENT_BASE_URL=http://<host>:<port>
```

`msinsight_observe` 通过这个接口读取最新的 server-side page context：

```text
GET /api/page/observation
```

浏览器侧链路是：

```text
Agent View iframe
  -> agentToolBridge.observeInsightPage()  (@insight/lib/agentToolBridge client, tool='observe')
  -> framework WebAgentSessionPanel  (via @insight/lib/agentToolBridge server, handle('observe', ...))
  -> POST /api/page/observation
  -> pageContextService
  -> msinsight_observe
```

Observation payload 必须只包含摘要和能力信息。不要暴露原始 profiling 数据、完整 MobX store、任意 DOM dump 或通用 JavaScript 执行能力。

## 工具

Insight 工具通过 `ToolRegistry` 注册在 `tools/msinsightTools.mjs` 中：

```text
msinsight_observe
msinsight_listActions
msinsight_invokeAction
```

native-agent 同时启用 Blade SDK 的只读文件工具：

```text
Read
Glob
Grep
```

这些工具仅允许访问当前 agent workspace，以及 host 注入的资源目录下的 `docs/` 和 `skills/`；未启用 `Edit`、`Write`、`Bash` 等写入或执行工具。

`msinsight_invokeAction` 目前只返回 `approval_required`，还不会真正执行页面动作。

## 安全边界

native agent 不应暴露：

- 任意 shell 访问；
- `docs/`、`skills/` 和 agent workspace 之外的文件系统访问；
- 通用浏览器自动化；
- profiling 原始数据导出；
- 任意 JavaScript 执行；
- 坐标级 click/type/scroll 作为普通工具。

后续页面控制必须通过 allowlist 中的语义 action，例如：

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
