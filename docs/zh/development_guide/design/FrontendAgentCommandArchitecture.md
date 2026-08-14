# Frontend Agent Command 架构设计

> 状态：核心 Command 架构已实现；用户审批与 `approvalGrant` 仍是后续扩展。旧 Action/Tool Bridge 已移除。新增业务能力请参考 [Frontend Agent Command 实践指南](../frontend_agent_command_implementation.md)。

## 1. 背景

MindStudio Insight 的可操作能力分布在 framework 与多个业务 Module iframe 中，Native Agent 则运行在独立进程。历史方案同时存在 Native Tool、Frontend Tool 和 Action 三套概念：

- Native Agent 为模型注册固定 Tool；
- Agent iframe 通过 `FrontendAgentToolBridge` 调用 framework Tool；
- framework 通过 Action Registry 聚合本地 Action 与 Module Action；
- Module 通过独立 Bridge 注册并执行 Action。

这使一次业务能力新增需要理解多层注册、发现和调用模型，也让 Tool 与 Action 的语义边界不清晰。特别是，当前使用的 `@blade-ai/agent-sdk@1.1.0` 只允许在创建或恢复 Session 时通过 `SessionOptions.tools` 注入 Tool；`SendOptions` 和公开 `ISession` API 不支持按轮次替换 Tool。因此，不能把页面动态能力可靠地映射成每轮动态 Blade Tool。

本设计将整个 MindStudio Insight 前端视为一个向 Agent 提供结构化命令的远程 CLI：Blade 只看到一个稳定的 `msinsight` Tool，framework 对外提供动态 Command。

## 2. 目标

1. Native Agent 只向模型注册一个固定 `msinsight` Tool；
2. framework 只对外暴露一个 `FrontendAgentCommandController`；
3. Agent iframe 只负责请求转发，不保存 Command 目录或业务状态；
4. Module 只暴露一个 `ModuleAgentCommandClient`；
5. framework、Module、表格和图等领域能力统一为 Command；
6. Command 只在能力拥有者处注册一次；
7. `help` 和 `observe` 由 Controller 内置；
8. framework 只暴露全局 Command 与当前 active Module Command；
9. Module iframe reload 后能够安全恢复 Command 目录，旧连接消息不能污染新目录；
10. deadline、取消、错误和未来审批机制具有清晰的单一责任边界。

## 3. 非目标

本设计不包含：

- 解析 shell 命令行字符串、引号、管道或重定向；
- 把每个前端 Command 动态注册为 Blade Tool；
- 让 Agent iframe 缓存、校验或路由 Command；
- 让通信层感知 Table、Graph、MemScope 等业务领域；
- 本阶段实现用户审批 UI、approval challenge 或 approval grant；
- 本阶段确定每个 Command 的具体超时数值。

## 4. 核心术语

### 4.1 Native Tool

Native Tool 是 Blade/LLM 可以直接调用的函数。本设计中，页面能力只通过一个固定 Tool 暴露：

```ts
msinsight({
    command: 'MemScope.graph.query',
    args: { nodeId: 'node-1' },
});
```

Native Tool 是模型协议入口，不是页面业务能力目录。

### 4.2 Command

Command 是 MindStudio Insight 前端提供的动态能力，例如：

```text
help
observe
framework.openModule
MemScope.table.refresh
MemScope.graph.query
Timeline.operator.locate
```

Command 定义由能力拥有者声明，执行由 framework Controller 统一发现、裁决和路由。

### 4.3 Transport operation

`executeCommand` 和 `cancelCommand` 是 Agent 与 framework 之间的内部传输操作，不是模型可见 Command：

```text
Native msinsight Tool
    → executeCommand / cancelCommand
    → FrontendAgentCommandController
```

## 5. 总体拓扑

```text
Blade / LLM
    │
    │ 固定 Tool：msinsight({ command, args })
    ▼
Native Agent Runtime
    │
    │ frontend command HTTP/SSE
    ▼
Agent iframe
    │
    │ acpMessage: executeCommand / cancelCommand
    ▼
FrontendAgentCommandController
    ├── 内置 Command：help、observe
    ├── Framework 全局 Command
    ├── 当前 active Module Command
    ├── Framework observation provider
    └── 私有 Module transport
            │
            │ moduleAgentMessage
            ▼
ModuleAgentCommandClient
    ├── Module observation provider
    ├── Command definition + handler
    └── 领域适配器最终调用 registerCommand()
```

## 6. 分层职责

### 6.1 Native Agent Runtime

Native Agent 只注册一个固定的 `msinsight` Tool：

```ts
interface MsinsightToolInput {
    command: string;
    args?: JsonObject;
}
```

职责：

- 将模型调用转换为 `executeCommand`；
- 为调用生成 requestId；
- 传播取消信号；
- 等待 Command result/error；
- 不保存前端 Command 目录；
- 不判断 active Module；
- 不决定 Command 是否可见或可执行。

Native 自有的文件读取、搜索等 Tool 不属于页面 Command 架构，可以继续独立存在。

### 6.2 Agent iframe

Agent iframe 是无业务状态的传输适配器，只负责：

- 接收 Native Runtime 的 Command 请求；
- 通过 HTTP claim 获取一次性 claimToken，并在终态 receipt 中回传；
- 通过 `acpMessage` 转发 `executeCommand` 与 `cancelCommand`；
- 按 requestId 等待 framework 返回 result/error；
- 在 iframe 卸载或连接关闭时清理 pending request。

Agent iframe 不得：

- 缓存 `help` 返回的 Command；
- 校验 Command input schema；
- 判断 Command 是否可见；
- 保存 active Module 或 Module route；
- 决定权限或审批。

### 6.3 FrontendAgentCommandController

每个 framework Window 只创建一个 `FrontendAgentCommandController`。它是 framework 对 Agent 和 framework 业务代码暴露的唯一公开门面。

公开能力示意：

```ts
interface FrontendAgentCommandController {
    registerGlobalCommand(definition: CommandDefinition, handler: CommandHandler): () => void;
    setFrameworkObservationProvider(provider: ObservationProvider): () => void;
    attachAgentFrame(frame: HTMLIFrameElement): () => void;
    attachModuleFrame(moduleId: string, frame: HTMLIFrameElement): () => void;
    setActiveModule(moduleId: string): void;
    dispose(): void;
}
```

Controller 负责：

- 内置 `help` 和 `observe`；
- 注册 framework 全局 Command；
- 接收 Module Command 完整快照；
- 计算当前可见 Command；
- 校验请求 envelope、名称、可见性、deadline 和连接代次；
- 在本地 handler 与 Module route 之间选择执行路径；
- 保存运行请求与原始执行路由；
- 按 requestId 取消原始请求；
- 聚合 framework 与 active Module observation；
- 处理 Agent iframe 和 Module iframe 生命周期。

以下组件可以作为 Controller 内部私有实现存在，但不对上层暴露，也不允许业务代码手工组合：

- Command catalog；
- ACP transport；
- Module transport；
- pending request store；
- connection handshake；
- 运行请求与取消路由。

### 6.4 ModuleAgentCommandClient

每个业务 Module iframe 创建一个 `ModuleAgentCommandClient`：

```ts
const client = new ModuleAgentCommandClient({
    moduleId: 'MemScope',
    observe: signal => observeMemScope(signal),
});

const unregister = client.registerCommand(definition, handler);
client.start();
```

它是 Module 侧唯一公开的 Agent 通信入口，负责：

- 允许在 `start()` 前注册 Command；
- 本地维护 `commandName → definition + handler`；
- 校验 Module 命名空间；
- 处理 framework 握手和 connectionToken；
- 上报完整 Command 快照；
- 接收执行和取消请求；
- 将 deadline 转换为 AbortSignal；
- iframe reload 或断线后重新握手并上报。

### 6.5 领域适配器

Table、Graph 等领域不能扩展 Client 的通信 API。所有领域能力最终统一调用：

```ts
client.registerCommand(definition, handler);
```

领域层可以提供独立适配函数：

```ts
registerTableCommands(client, moduleId, tableControllers);
registerGraphCommands(client, graphController);
```

但 `ModuleAgentCommandClient` 本身只认识 Command 和 observation provider，不认识任何领域对象。

## 7. Command 模型

### 7.1 Command 定义

```ts
interface CommandDefinition {
    name: string;
    title: string;
    description: string;
    inputSchema: JsonObject;
    approval?: 'none' | 'required';
    timeoutMs?: number;
}
```

`approval` 和 `timeoutMs` 是协议扩展点。审批流程和具体超时治理策略不在本阶段实现。

`inputSchema` 是 `help` 返回的发现与调用契约。本阶段不引入通用 JSON Schema 执行引擎：Controller 校验请求 envelope 和内置 Command 参数；Framework 全局 handler 与 Module 领域适配器在能力边界按同一 schema 校验实际 args。

### 7.2 Command handler

```ts
interface CommandContext {
    requestId: string;
    deadline: number;
    signal: AbortSignal;
}

type CommandHandler = (
    args: JsonObject,
    context: CommandContext,
) => Promise<unknown> | unknown;
```

### 7.3 命名规则

- `help`、`observe` 是 Controller 保留名称；
- framework 全局 Command 必须使用 `framework.` 前缀；
- Module Command 必须使用 `${moduleId}.` 前缀；
- 所有当前可见 Command 名称必须唯一；
- 注册冲突立即失败，不允许静默覆盖。

示例：

```text
help
observe
framework.openModule
MemScope.table.refresh
MemScope.graph.query
Timeline.operator.locate
```

## 8. 内置 Command

### 8.1 help

`help` 是动态发现 Command 的唯一入口，不再存在独立 `listTools` 或 `listActions`。

一级查询只返回轻量索引：

```ts
msinsight({ command: 'help', args: {} });
```

```ts
{
    commands: [
        {
            name: 'observe',
            title: 'Observe page',
            description: 'Observe the current Insight page.',
        },
        {
            name: 'MemScope.graph.query',
            title: 'Query memory graph',
            description: 'Query nodes and edges in the current graph.',
        },
    ],
}
```

二级查询只返回指定 Command 的完整定义：

```ts
msinsight({
    command: 'help',
    args: { command: 'MemScope.graph.query' },
});
```

```ts
{
    command: {
        name: 'MemScope.graph.query',
        title: 'Query memory graph',
        description: 'Query nodes and edges in the current graph.',
        inputSchema: {},
    },
}
```

若目标 Command 当前不可见，返回 `COMMAND_UNAVAILABLE`。

### 8.2 observe

`observe` 是 Controller 内置的统一观察入口：

```ts
msinsight({ command: 'observe', args: {} });
```

Controller 聚合：

1. framework observation provider；
2. 当前 active Module 的 observation provider。

```ts
{
    collectedAt: 0,
    app: {
        activeModule: 'MemScope',
        availableModules: [],
    },
    module: {},
}
```

每个 Module 必须提供独立的 `observe()` provider，但不注册 `${moduleId}.observe` Command。对 Agent 始终只有一个稳定的 `observe`。

## 9. 可见性与目录

Controller 当前可见目录为：

```text
内置 help、observe
+ Framework 全局 Command
+ 当前 active Module Command
```

不返回其他非 active Module 的 Command。`executeCommand` 必须在执行时重新检查 Command 仍处于当前可见集合，不能仅信任之前的 `help` 结果。

Command 快照表达结构性能力：

- 组件挂载且支持能力时，Command 存在；
- 组件卸载或永久不再支持时，Command 移除；
- `busy/loading` 等瞬时状态不驱动目录高频增删；
- 暂时忙碌由 handler 在执行时返回 `COMMAND_BUSY`。

## 10. Agent 与 Controller 协议

对外只保留两个传输操作：

```text
executeCommand
cancelCommand
```

### 10.1 executeCommand

```ts
interface ExecuteCommandRequest {
    event: 'frontendAgent/executeCommand';
    requestId: string;
    command: string;
    args: JsonObject;
    deadline: number;
}
```

响应：

```ts
interface ExecuteCommandResponse {
    event: 'frontendAgent/commandResponse';
    requestId: string;
    result: unknown;
}
```

错误：

```ts
interface ExecuteCommandError {
    event: 'frontendAgent/commandError';
    requestId: string;
    error: CommandError;
}
```

### 10.2 cancelCommand

```ts
interface CancelCommandRequest {
    event: 'frontendAgent/cancelCommand';
    requestId: string;
    targetRequestId: string;
}
```

`cancelCommand` 不暴露给模型。Native `msinsight` Tool 的 AbortSignal 触发时自动发送取消。

取消必须幂等：目标已经完成、取消或超时，也可以返回成功。

## 11. Module 协议与完整快照

### 11.1 握手

framework 每次 attach 或 reload Module iframe 时：

1. 生成新的随机 `connectionToken`；
2. 立即清除该 Module 的旧 Command 目录；
3. 发送 `moduleAgent/hello`；
4. Module 回复 `moduleAgent/ready`；
5. Module 发送完整 Command 快照。

```ts
{
    event: 'moduleAgent/hello',
    moduleId: 'MemScope',
    connectionToken: '...',
}
```

`ready`、`commandsChanged`、request、response、error 和 cancel 都必须携带当前 connectionToken。

### 11.2 完整快照

```ts
{
    event: 'moduleAgent/commandsChanged',
    moduleId: 'MemScope',
    connectionToken: '...',
    commands: [
        { name: 'MemScope.table.refresh', ... },
        { name: 'MemScope.graph.query', ... },
    ],
}
```

Framework 收到后直接替换：

```text
moduleCommands[MemScope] = receivedCommands
```

不维护 `commandAdded/commandRemoved` 增量状态机。完整快照在注册、注销、结构性能力变化和重新握手时发送。

### 11.3 connectionToken

connectionToken 标识一次 Module 文档连接代次。Controller 必须丢弃 token 不匹配的所有消息。

原因是 iframe reload 前后的文档可能共享同一个 WindowProxy、origin 和 moduleId；只检查 `event.source` 与 origin 无法排除旧文档的延迟消息。connectionToken 防止旧快照重新污染新目录，也防止迟到 response 完成新一代请求。

## 12. 执行与路由

Controller 收到 `executeCommand` 后：

```text
校验 request envelope
→ 校验 deadline
→ 查找当前可见 Command
→ 由内置或领域边界校验 args
→ 执行权限裁决
→ 保存 requestId 对应的实际 handler/Module route
→ 本地执行或转发 Module
→ 返回 result/error
→ 清理 running request
```

Framework 全局 Command 直接调用本地 handler。Module Command 经 attach 时保存的私有 transport 路由到对应 Module iframe。

运行请求必须绑定启动时的真实执行路由。用户在执行期间切换 active Module 后：

- 不把运行中的请求迁移到新 Module；
- `cancelCommand` 仍取消原始 handler 或原 Module route；
- 正常 response 仍可以完成原请求；
- 新调用只能使用新的当前可见目录。

## 13. Deadline、取消与断线

### 13.1 Deadline

普通执行请求使用一个绝对 deadline，沿完整链路透传：

```text
Native Runtime
→ Agent iframe
→ Controller
→ Module transport
→ Module handler
```

每层只计算剩余时间：

```ts
const remaining = deadline - Date.now();
```

任何层都不能重新获得完整超时预算。超时后清理 pending/running 状态，并丢弃迟到响应。

### 13.2 取消

- Native AbortSignal 自动触发 `cancelCommand`；
- Controller 按 targetRequestId 找到原始执行路由；
- 本地 handler 通过 AbortController 取消；
- Module handler 通过携带 connectionToken 的 cancel 消息取消；
- 底层任务无法物理中断时，至少禁止迟到结果提交。

### 13.3 断线

- Agent iframe detach：拒绝对应 pending Agent 请求；
- Module iframe reload/detach：清空该 Module 目录，拒绝该 route 上的 pending 请求；
- 新连接必须完成新 token 握手后才能发布目录和接受执行；
- 断线请求不跨 iframe 文档自动恢复，调用方可根据错误重新 `help/observe` 后重试。

## 14. 安全边界

`channel` 只用于协议分流，不构成安全边界。Controller 和 Client 必须同时校验：

- `event.source` 是预期 iframe Window；
- `event.origin` 匹配预期 origin；
- moduleId 与绑定 frame 一致；
- connectionToken 属于当前连接代次；
- requestId、event、deadline 与消息结构有效；
- Command 当前可见且名称符合命名空间；
- 内置 Command 或能力拥有者按 Command inputSchema 校验 args。

Framework 是页面能力的最终裁决者。Native Agent、Agent iframe 和 Module 上报的目录都不能绕过 Controller 的执行时复核。

## 15. 错误模型

```ts
interface CommandError {
    code: string;
    message: string;
    retryable: boolean;
    details?: JsonObject;
}
```

通用错误码建议包括：

```text
COMMAND_INVALID
COMMAND_NOT_FOUND
COMMAND_UNAVAILABLE
COMMAND_BUSY
COMMAND_TIMEOUT
COMMAND_CANCELLED
COMMAND_CONNECTION_LOST
COMMAND_PERMISSION_DENIED
COMMAND_APPROVAL_REQUIRED
COMMAND_EXECUTION_FAILED
```

领域错误可以保留更具体的 code、details 和稳定状态信息。所有传输层必须保留结构化错误，不压缩成普通字符串。

## 16. 生命周期

### 16.1 Framework Window

一个 framework Window 只创建一个 Controller。Controller 生命周期不跟随 Agent 面板开关或某个 React effect 重建。

React 组件只负责：

```ts
controller.attachAgentFrame(frame);
controller.attachModuleFrame(moduleId, frame);
controller.setActiveModule(activeModule);
```

组件卸载只 detach 对应资源。framework Window 销毁时才 dispose Controller。

### 16.2 Module

Module 可以在 Client start 前注册 Command。启动后完成握手并发布完整快照；注册、注销或结构性能力变化后发布新快照。Client dispose 时终止本地运行请求并停止通信。

### 16.3 Agent iframe

Agent iframe 可以 reload 或被隐藏，不拥有 Command catalog。重新连接后不需要恢复目录，只需继续转发新的 Native 请求。

## 17. 审批扩展点

需要用户审批的 Command 未来由 Native Agent/ACP 承载交互，但审批协议对模型隐藏，Controller 保留最终裁决权。

已确认的约束：

- 等待用户审批阶段不设置执行超时；
- 批准后的实际执行使用新的执行请求与 deadline；
- active Module 变化或 connectionToken 变化后审批失效；
- Controller 生成并原子消费一次性 approvalGrant；
- grant 绑定 Command、args 指纹、active Module 和 connectionToken；
- approvalGrant 不属于模型可见的 `msinsight` input schema。

该机制本阶段不实现，CommandDefinition 仅保留 `approval` 扩展字段。

## 18. 目标 API 摘要

### Native Agent

```ts
msinsight({ command, args });
```

### Framework

```ts
controller.registerGlobalCommand(definition, handler);
controller.attachAgentFrame(frame);
controller.attachModuleFrame(moduleId, frame);
controller.setActiveModule(moduleId);
```

### Module

```ts
const client = new ModuleAgentCommandClient({ moduleId, observe });
client.registerCommand(definition, handler);
client.start();
```

### Agent 与 Framework transport

```text
executeCommand
cancelCommand
commandResponse
commandError
```

### Module 与 Framework transport

```text
hello
ready
commandsChanged
observe
executeCommand
cancelCommand
commandResponse
commandError
```

## 19. 被替代的设计

目标架构不再保留以下对外概念：

- `msinsight_listActions`；
- `msinsight_invokeAction`；
- Agent 可见的 Action；
- 独立的 `listTools` transport operation；
- Agent iframe Command catalog；
- 业务代码直接组合 frontend tool registry、action registry 和 connection manager；
- `ModuleAgentToolBridgeClient.registerAction()`；
- 通信 Client 上的 `registerTableActions()` 等领域专用注册 API。

迁移后：

- Native 只保留固定 `msinsight` Tool；
- `help` 替代动态 Action/Tool 发现；
- `executeCommand` 替代通用 Action 调用；
- `FrontendAgentCommandController` 成为 framework 唯一公开门面；
- `ModuleAgentCommandClient.registerCommand()` 成为 Module 唯一注册入口。

## 20. 实施结果

已完成：

1. 中立 Command contract、错误模型和两类 message channel；
2. framework Window 单例 `FrontendAgentCommandController`；
3. Agent iframe 的 `executeCommand/cancelCommand` 薄适配；
4. 带 connectionToken 的私有 Module transport；
5. `ModuleAgentCommandClient.registerCommand()` 与独立 observation provider；
6. 内置 `help/observe`；
7. 单一 `msinsight({ command, args })` Native Tool；
8. 表格领域适配器与 MemScope 样板；
9. 旧 Action/listActions/invokeAction 分层删除；
10. Command 化的 Window message 调试字段。

后续工作仅包括第 17 节的审批扩展，以及按业务需要增加新的 Framework/Module Command。
