# Agent 能力中心设计

> 状态：第一阶段已实现全局 HTTP MCP Tool 接入。

## 1. 目标与边界

能力中心用于统一注册和执行 Agent Tool，避免每种 Agent Runtime 各维护一套 Tool 定义与业务实现。

当前边界：

- ACP 管理 Agent、Session、Prompt、状态、取消和用户授权；
- MCP 向 Agent 发布并调用 Tool；
- Capability Center 管理 Tool 定义、校验和执行；
- `msinsight` 是首个 Tool，用于控制当前 MindStudio Insight 前端页面；
- 能力中心是 Host 进程级全局能力，不与 ACP Session 一一绑定；
- 暂不支持 MCP-over-ACP、Session 级能力隔离、审计和幂等缓存。

## 2. 架构

```text
MindStudio Agent Panel
        │ ACP
        ▼
ACP Agent（OpenCode 等）
        │ HTTP MCP
        ▼
┌──────────────── insight_web_agent Host ────────────────┐
│ HTTP MCP Adapter                                       │
│        │                                                │
│        ▼                                                │
│ Capability Center                                      │
│  ├─ Registry：Tool 定义                                │
│  ├─ invoke：统一调用入口                               │
│  └─ Executor                                           │
│       └─ msinsight → FrontendCommandService → Framework│
└─────────────────────────────────────────────────────────┘

msinsight-native
        │ Host Internal API
        └──────────────→ Capability Center
```

外部 Agent 通过 HTTP MCP 调用能力；`msinsight-native` 启动时直接读取同一份 `capability-center.json` 生成代理 Tool，再通过独立 Token 保护的内部 API 调用同一个执行核心。配置型能力在 Native 侧执行前强制经过 ACP 用户授权。

## 3. 核心组件

### 3.1 Capability Definition

定义模型可见的 Tool 信息：

```ts
{
    name,
    description,
    inputSchema,
    outputSchema?,
    annotations?,
    validate?,
    execute,
}
```

当前注册：

```ts
msinsight({ command: string, args?: object })
pt_snap({ args: string[], timeoutMs?: number }) // CLI 可解析时
```

`msinsight` 的页面动态能力通过 `help/observe` 发现。其他能力由产品资源目录中的 `capability-center.json` 声明；`pt_snap.args` 原样映射为 `pt-snap` 可执行文件后的 argv，工作目录固定为 Host 注册目录，能力中心不解析或改变 CLI 子命令语义。

### 3.2 Capability Registry

Registry 提供：

```text
register(capability)
list()
execute(name, input, context)
```

它负责重名检查、定义投影、输入校验和执行路由，不感知 ACP 或 MCP。

### 3.3 HTTP MCP Adapter

Endpoint：

```text
/mcp/capabilities
```

Adapter 使用 MCP Streamable HTTP，处理：

- MCP initialize 和 Session ID；
- `tools/list`；
- `tools/call`；
- cancelled notification 和 HTTP 断连；
- MCP result/error 映射。

MCP Session 仅保存协议状态，不保存 Tool 业务状态；最大连接数为 64。

### 3.4 ACP Session Integration

`createCapabilitySessionIntegration` 决定 ACP 的 `session/new/load/resume` 是否携带能力中心 MCP 配置。

注入条件：

- Agent 声明 `mcpCapabilities.http`；
- Agent 不是 `msinsight-native`。

OpenCode 将 ACP 注入的 MCP Client 保存为目录级全局连接，重复注册同名 Server 会关闭旧连接。因此同一 OpenCode 进程只在首次成功建立 MCP 连接时注入，后续 Session 复用；连接丢失、Agent 切换或 transport 异常后重新注入。

## 4. 调用时序

```text
用户发送 Prompt
→ ACP session/prompt
→ 模型提出 MCP Tool Call
→ Agent 权限策略
→ 必要时 ACP session/request_permission
→ 用户允许
→ MCP tools/call
→ Capability Center.invoke
→ msinsight Executor
→ FrontendCommandService
→ Framework / active Module
→ MCP Tool Result
→ Agent 回灌到原会话
```

OpenCode 通过自身 Tool Call ID 和 MCP JSON-RPC ID 将结果返回发起调用的 ACP Session；能力中心不负责聊天 Session 的结果路由。

## 5. 权限与安全

### 5.1 Agent 用户授权

OpenCode 的普通 MCP Tool 在执行前先进入权限引擎。需要询问时，通过 ACP `session/request_permission` 显示通用 Tool 授权卡。

Host 支持：

```text
allow_once
allow_always
reject_once / reject_always
```

UI 只展示 Agent 实际提供的选项。通用 Tool 的 `allow_always` 由 Agent 保存，Host 不写入文件目录或 Bash allowlist。无法匹配选项时返回 `cancelled`，不默认选择任意选项。

Capability Center 不重复弹窗；它只执行 Tool 存在性、输入、状态、超时和取消等确定性校验。

### 5.2 MCP Token

Host 启动时生成一个固定的进程级 Bearer Token：

- 所有 ACP Session 共享；
- 不写磁盘、不需要用户配置；
- Host 退出后失效；
- 仅用于 `/mcp/capabilities`，不复用普通 `/api/*` Token；
- 使用常量时间比较。

Host 默认监听 `127.0.0.1`，MCP 路由拒绝带浏览器 Origin 的请求。若监听非 loopback 地址，必须额外增加 TLS 和网络访问控制。

## 6. 全局作用域限制

当前所有 ACP Session 共享同一个能力中心和当前活动前端页面：

```text
Session A ─┐
Session B ─┼→ Global Capability Center → 当前活动页面
Session C ─┘
```

标准 MCP `tools/call` 不携带可信 ACP Session ID，因此当前不支持：

- Session 级 Tool 可见性；
- Session 级权限、审计和业务上下文；
- 必须依赖独立 Session 状态的 Tool。

此类能力需等待 Session-scoped MCP、MCP-over-ACP 或可信 Session metadata。

## 7. 生命周期

- **Host 启动**：创建 Token、Capability Center 和 HTTP MCP Adapter；
- **Session 创建/恢复**：根据 Agent capability 注入 MCP 配置；
- **Agent 切换成功**：断开旧 ACP Agent，关闭旧 MCP Session，并重置注入状态；
- **ACP transport error**：关闭 MCP Session，下次 ACP Session 重新注入；
- **Host shutdown**：取消前端命令，关闭 MCP、HTTP/SSE 和 ACP Adapter。

## 8. 扩展新能力

普通能力实现 `validate/execute` 后注册。CLI 能力通过 `capability-center.json` 声明：

```json
{
  "schemaVersion": 1,
  "capabilities": [{
    "type": "cli",
    "name": "pt_snap",
    "description": "Run the pt-snap CLI.",
    "executable": {
      "win32": ["../pt-snap/bin/pt-snap.exe", "pt-snap.exe", "pt-snap"],
      "default": ["../pt-snap/bin/pt-snap", "pt-snap"]
    }
  }]
}
```

候选按顺序解析：带路径的相对值以产品 `resourceDir` 为基准；裸命令名从 Host 的 PATH 查找。解析成功后统一转换为绝对路径并由 `createCliCapability` 以 `shell:false` 执行。超时、输出上限和并发限制使用 Host 内部安全默认值，不暴露为产品配置。配置仅在 Host 启动时加载，修改后需重启。

新增 CLI 只需增加配置项；无需修改 Host 组合代码。新增能力仍需明确副作用、审批要求和是否适合全局作用域。

## 9. 代码索引

| 职责 | 路径 |
| --- | --- |
| Tool 定义与 MCP 常量 | `server/capability-center/definitions.mjs` |
| Capability Registry | `server/capability-center/registry.mjs` |
| CLI 能力配置 | `capability-center.json` |
| 配置加载与路径解析 | `server/config/capabilityCenterConfig.mjs` |
| 配置能力注册 | `server/capability-center/configuredCapabilities.mjs` |
| 通用 CLI Tool 工厂 | `server/capability-center/cliCapability.mjs` |
| 有界子进程执行器 | `server/infrastructure/boundedProcess.mjs` |
| 能力执行核心 | `server/capability-center/service.mjs` |
| ACP MCP 注入 | `server/capability-center/sessionIntegration.mjs` |
| HTTP MCP Adapter | `server/capability-adapters/http-mcp/adapter.mjs` |
| Native Internal API | `server/controllers/capabilityController.mjs` |
| Native Tool Adapter | `server/native-agent/tools/capabilityTools.mjs` |
| 前端命令 Broker | `server/services/frontendCommandService.mjs` |
