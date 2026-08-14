# 新增 Frontend Agent Command 实践指南

## 1. 适用范围

本文说明如何在当前 Frontend Agent Command 架构下新增一项可被 Agent 发现和调用的前端能力。

当前架构将整个 MindStudio Insight 前端视为一个结构化远程 CLI。模型只看到一个固定 Native Tool：

```ts
msinsight({ command, args });
```

页面中的动态能力称为 Command。新增普通 Command 时，能力只在实际拥有者处注册一次：

- Framework 自身能力：注册为 `framework.*` 全局 Command；
- 业务 Module 能力：注册为 `${moduleId}.*` Module Command；
- 表格等领域能力：通过独立领域适配器最终调用 `ModuleAgentCommandClient.registerCommand()`。

**不要为新 Command 修改 Native Tool、Web Agent server、Agent iframe relay 或 message protocol。** 这些层已经是通用传输链路。

完整分层和协议设计见 [Frontend Agent Command 架构](./design/FrontendAgentCommandArchitecture.md)。表格能力另见 [表格 Command 接入指南](./agent_table_command_implementation.md)。

## 最短接入路径

新增普通 Module Command 通常只需要五步：

1. 找到或创建该 Module 唯一的 `ModuleAgentCommandClient`；
2. 定义带 `${moduleId}.` 前缀的 `CommandDefinition`；
3. 编写与 `inputSchema` 一致的参数 parser；
4. 调用 `client.registerCommand(definition, handler)`；
5. 用 `help {}`、`help { command }` 和实际调用完成联调。

如果能力属于 Framework，则把第 1、4 步替换为 `frontendAgentCommandController.registerGlobalCommand()`，名称使用 `framework.` 前缀。其余参数、取消、错误和结果要求不变。

## 2. 先判断 Command 应注册在哪里

| 能力归属 | 注册入口 | 命名规则 | 可见范围 |
| --- | --- | --- | --- |
| Framework 自身 | `frontendAgentCommandController.registerGlobalCommand()` | `framework.*` | 始终可见 |
| 业务 Module | `ModuleAgentCommandClient.registerCommand()` | `${moduleId}.*` | 仅当前 active Module 可见 |
| 表格 | `registerTableCommands()` | `${moduleId}.table.*` | 当前 active Module 中至少一个可见表支持时可见 |
| 内置发现和观察 | Controller 内置 | `help`、`observe` | 始终可见 |

判断标准不是“代码放在哪个仓库目录”，而是“谁拥有最终执行和状态裁决权”。

例如：

- 切换 Framework 页签：`framework.switchModule`；
- 查询 MemScope 图节点：`MemScope.graph.query`；
- 读取 Timeline 当前选中算子：`Timeline.operator.getSelection`；
- 操作通用业务表格：复用 `${moduleId}.table.*`，不要重复设计另一套表格 Command。

`help` 和 `observe` 是保留名，业务代码不得覆盖，也不要注册 `${moduleId}.observe`。

## 3. 一个 Command 的组成

公共类型由 `@insight/lib/FrontendAgentCommand` 导出：

```ts
interface CommandDefinition {
    name: string;
    title: string;
    description: string;
    inputSchema: JsonObject;
    approval?: 'none' | 'required';
    timeoutMs?: number;
}

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

### 3.1 definition

`CommandDefinition` 用于 `help` 动态发现：

- `name`：稳定、唯一、带所有者命名空间；
- `title`：短标题；
- `description`：说明能力、执行效果和结果语义；
- `inputSchema`：模型构造 `args` 的调用契约；
- `approval`、`timeoutMs`：预留扩展点，当前阶段没有实现审批流程和按 Command 超时治理。

当前不要依赖 `approval: 'required'` 触发审批 UI，也不要依赖 `timeoutMs` 改变实际 deadline。

### 3.2 handler

Handler 接收经过通用 envelope 校验的 `args`，但仍必须在能力边界校验领域参数。Handler 应：

1. 校验字段、类型、范围和未知参数；
2. 检查当前业务可用性；
3. 将 `context.signal` 传给底层异步操作；
4. 在提交业务状态前再次检查取消或请求代次；
5. 返回可 JSON 序列化的结果；
6. 使用结构化 `CommandError` 表达可识别失败。

## 4. Module Command：与 MemScope 一致的推荐实践

普通业务能力通常按本节接入。当前参考实现是 `modules/leaks/src/agent/runtime.ts`：每个 Module 在 `agent/runtime.ts` 中创建一个模块级单例 Client，在模块加载时完成静态 Command 或领域适配器注册，再暴露幂等的 `start/stop` 函数。

不要为每次 React mount 创建新的 Client，也不要用 runtime factory 隐藏 Client 后又让组件直接引用它。

下面示例在同一结构下新增 `YourModule.resource.query`。`resourceStore` 代表 Module 已有的业务状态和查询入口，实际接入时替换为真实依赖。

### 4.1 创建模块级 runtime

```ts
import {
    COMMAND_ERROR_CODES,
    CommandError,
    type CommandHandler,
    type JsonObject,
} from '@insight/lib/FrontendAgentCommand';
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { resourceStore } from '../store/resourceStore';

const MODULE_ID = 'YourModule';

const handleResourceQuery: CommandHandler = async (args, context) => {
    const input = parseResourceQueryArgs(args);
    if (!resourceStore.ready) {
        throw new CommandError({
            code: COMMAND_ERROR_CODES.UNAVAILABLE,
            message: 'Resource query is not ready.',
            retryable: true,
        });
    }
    if (resourceStore.loading) {
        throw new CommandError({
            code: COMMAND_ERROR_CODES.BUSY,
            message: 'Resource query is busy.',
            retryable: true,
        });
    }

    const result = await resourceStore.query(input, context.signal);
    if (context.signal.aborted) throw context.signal.reason;
    return result;
};

const moduleAgentCommandClient = new ModuleAgentCommandClient({
    moduleId: MODULE_ID,
    observe: () => ({
        moduleId: MODULE_ID,
        selectedResourceId: resourceStore.selectedId ?? null,
        revision: resourceStore.revision,
        ready: resourceStore.ready,
        busy: resourceStore.loading,
    }),
});

moduleAgentCommandClient.registerCommand({
    name: `${MODULE_ID}.resource.query`,
    title: 'Query resources',
    description: 'Query resources in the current module and return JSON summaries.',
    inputSchema: {
        type: 'object',
        properties: {
            keyword: { type: 'string', minLength: 1 },
            limit: { type: 'integer', minimum: 1, maximum: 100 },
        },
        required: ['keyword'],
        additionalProperties: false,
    },
}, handleResourceQuery);

let stopClient: (() => void) | undefined;

export const startYourModuleAgentRuntime = (): void => {
    if (stopClient) return;
    stopClient = moduleAgentCommandClient.start();
};

export const stopYourModuleAgentRuntime = (): void => {
    stopClient?.();
    stopClient = undefined;
};

const parseResourceQueryArgs = (
    args: JsonObject,
): { keyword: string; limit: number } => {
    const allowedKeys = new Set(['keyword', 'limit']);
    const unknownKey = Object.keys(args).find(key => !allowedKeys.has(key));
    if (unknownKey) throw invalid(`Unknown argument '${unknownKey}'.`);

    if (typeof args.keyword !== 'string' || !args.keyword.trim()) {
        throw invalid('keyword must be a non-empty string.');
    }

    const limit = args.limit ?? 20;
    if (typeof limit !== 'number' || !Number.isInteger(limit) || limit < 1 || limit > 100) {
        throw invalid('limit must be an integer between 1 and 100.');
    }

    return { keyword: args.keyword.trim(), limit };
};

const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID,
    message,
    retryable: false,
});
```

这与 MemScope 的结构一一对应：

```ts
const moduleAgentCommandClient = new ModuleAgentCommandClient({
    moduleId: MODULE_ID,
    observe: () => ({
        moduleId: MODULE_ID,
        tables: observeTableCommands(MODULE_ID, tableRegistry),
    }),
});
registerTableCommands(moduleAgentCommandClient, MODULE_ID, tableRegistry);
let stopClient: (() => void) | undefined;
export const start... = (): void => { ... };
export const stop... = (): void => { ... };
```

区别只在注册方式：

- 普通 Command 直接调用 `moduleAgentCommandClient.registerCommand(definition, handler)`；
- 表格等成组领域能力调用独立适配器，例如 `registerTableCommands(...)`；
- 领域适配器内部最终仍调用同一个 `registerCommand()`。

### 4.2 在 Module 应用生命周期中启动

和 MemScope `App.tsx` 一样，在 Module 文档生命周期内显式启动和停止：

```ts
import {
    startYourModuleAgentRuntime,
    stopYourModuleAgentRuntime,
} from './agent/runtime';

const App = (): JSX.Element => {
    useEffect(() => {
        startYourModuleAgentRuntime();
        return stopYourModuleAgentRuntime;
    }, []);

    return <YourModule />;
};
```

`startYourModuleAgentRuntime()` 是幂等的，避免 React 生命周期或重复调用建立多个 Router subscription。`stopYourModuleAgentRuntime()` 调用 Client cleanup，停止通信并取消本地运行请求。

Command 在 `start()` 前已经注册。Client 收到 Framework `hello` 后会自动：

1. 绑定当前 `connectionToken`；
2. 回复 `ready`；
3. 发送包含全部已注册 Command 的完整快照。

业务代码不要自己发送 `ready` 或 `commandsChanged`。

### 4.3 结构性能力变化

模块加载时真实存在的能力应像 MemScope 一样注册一次，不要跟随普通 render、loading 或数据是否为空反复注册。

只有结构性能力变化才更新注册：

- 组件或领域 Controller 挂载后，能力真实出现；
- 组件卸载后，能力永久消失；
- 当前数据类型从协议上根本不支持该能力。

瞬时状态应保留 Command，并在 handler 中返回 `COMMAND_BUSY` 或 `COMMAND_UNAVAILABLE`：

- loading；
- busy；
- 请求暂时失败；
- 当前结果为空；
- 短暂 not-ready。

如果一组 Command 随领域 Controller 动态变化，优先实现独立适配器，由适配器订阅结构性变化并管理 `registerCommand()` 返回的 unregister 函数：

```ts
const unregisterGraphCommands = registerGraphCommands(
    moduleAgentCommandClient,
    MODULE_ID,
    graphControllerRegistry,
);
```

这与 `registerTableCommands()` 的做法一致。不要让任意 React 组件直接访问一个隐藏在 runtime 内部的 Client，也不要把 busy/loading 变化变成高频 `commandsChanged` 快照。

## 5. Framework 全局 Command

Framework 自有能力使用 Window 单例 `frontendAgentCommandController`：

```ts
import {
    COMMAND_ERROR_CODES,
    CommandError,
    type JsonObject,
} from '@insight/lib/FrontendAgentCommand';
import { frontendAgentCommandController } from '@/agent/frontendAgentCommandController';

const unregister = frontendAgentCommandController.registerGlobalCommand({
    name: 'framework.switchModule',
    title: 'Switch active module',
    description: 'Switch the active Insight module by its registered module name.',
    inputSchema: {
        type: 'object',
        properties: {
            moduleId: { type: 'string', minLength: 1 },
        },
        required: ['moduleId'],
        additionalProperties: false,
    },
}, async (args: JsonObject, context) => {
    if (Object.keys(args).some(key => key !== 'moduleId') ||
        typeof args.moduleId !== 'string' || !args.moduleId.trim()) {
        throw new CommandError({
            code: COMMAND_ERROR_CODES.INVALID,
            message: 'moduleId must be a non-empty string.',
            retryable: false,
        });
    }

    const moduleId = args.moduleId.trim();
    await switchActiveModule(moduleId, context.signal);
    if (context.signal.aborted) throw context.signal.reason;
    return { activeModule: moduleId };
});
```

若从 React 组件注册，应由 effect 返回 `unregister`：

```ts
useEffect(() => {
    return frontendAgentCommandController.registerGlobalCommand(definition, handler);
}, [handler]);
```

Framework Command 必须使用 `framework.` 前缀。业务 Module 不应把自己的能力注册成 Framework 全局 Command 来绕过 active Module 可见性。

## 6. inputSchema 与运行时校验必须一致

当前架构不引入 Ajv。`inputSchema` 是 `help` 暴露给模型的发现契约，不会替代领域参数校验。

因此 definition 和 parser 必须放在相邻位置，并保持以下约束一致：

| schema 声明 | handler 必须执行的校验 |
| --- | --- |
| `required` | 必填字段存在 |
| `type` | 运行时类型正确 |
| `minimum` / `maximum` | 数值范围正确 |
| `enum` | 值属于允许集合 |
| `additionalProperties: false` | 拒绝未知字段 |
| 字符串 `minLength` | 拒绝空字符串或全空白字符串 |

不应出现以下漂移：

- schema 允许，handler 拒绝；
- schema 拒绝，handler 静默接受；
- handler 忽略未知字段；
- schema 声明一个名称，handler 读取另一个名称。

### 6.1 为什么仍要写 inputSchema

Agent 使用两级 `help` 获取当前能力：

```ts
msinsight({ command: 'help', args: {} });

msinsight({
    command: 'help',
    args: { command: 'YourModule.resource.query' },
});
```

第二次调用返回完整 `CommandDefinition`，模型据此构造实际 `args`。没有准确 schema，Command 即使能执行，也无法可靠发现和调用。

## 7. Observation 与 Command 的配合

`observe` 是统一页面观察入口，不属于某个普通 Command。每个 Module 创建 Client 时必须提供独立 observation provider：

```ts
const client = new ModuleAgentCommandClient({
    moduleId: 'YourModule',
    observe: signal => ({
        moduleId: 'YourModule',
        selectedResourceId: resourceStore.selectedId ?? null,
        revision: resourceStore.revision,
        ready: resourceStore.ready,
    }),
});
```

如果 Command 需要模型传入某个当前页面对象 ID、revision 或目标状态，该信息应出现在 observation 中。例如：

```text
observe
→ 获得 selectedResourceId 和 revision
→ help { command: 'YourModule.resource.update' }
→ 按 schema 调用 Command
→ 必要时再次 observe 验证结果
```

Observation 只返回调用决策所需的摘要和稳定状态，不要返回：

- 完整 MobX store；
- DOM 或 ReactNode；
- 大规模原始 profiling 数据；
- 函数、循环引用或不可序列化对象；
- 能扩展文件权限的任意路径。

## 8. Deadline 和取消

每次调用携带同一个绝对 `deadline`，从 Native Runtime 一直透传到 handler。Handler 不应重新创建一份完整超时预算。

正确做法：

```ts
const result = await requestData({
    signal: context.signal,
    requestId: context.requestId,
});
```

如果下层 API 只接受剩余毫秒数：

```ts
const remainingMs = context.deadline - Date.now();
if (remainingMs <= 0) {
    throw new CommandError({
        code: COMMAND_ERROR_CODES.TIMEOUT,
        message: 'The command deadline has expired.',
        retryable: true,
    });
}
```

Handler 必须遵守 `context.signal`：

- `fetch`、可取消 SDK 或 worker 请求直接传入 signal；
- 取消时停止真实请求；
- 无法物理取消时，至少在提交业务状态前检查 signal；
- 迟到结果不得覆盖新状态；
- `context.requestId` 可传给底层用于幂等、日志或请求代次隔离。

不要吞掉 `AbortSignal.reason` 后返回成功，也不要把取消统一改写成普通业务失败。

Native 前端 Command 默认 deadline 为 30 秒，当前上限为 60 秒。`CommandDefinition.timeoutMs` 暂未改变这一规则。

## 9. 结构化错误

通用错误使用 `CommandError`：

```ts
throw new CommandError({
    code: COMMAND_ERROR_CODES.BUSY,
    message: 'The current view is busy.',
    retryable: true,
    details: { phase: 'loading' },
});
```

常用错误：

| code | 使用场景 | retryable 建议 |
| --- | --- | --- |
| `COMMAND_INVALID` | 参数缺失、类型或范围错误 | `false` |
| `COMMAND_UNAVAILABLE` | 当前状态暂时不可执行 | 视情况为 `true` |
| `COMMAND_BUSY` | 互斥业务事务执行中 | `true` |
| `COMMAND_TIMEOUT` | deadline 已到 | `true` |
| `COMMAND_CANCELLED` | 用户或上层取消 | `true` |
| `COMMAND_CONNECTION_LOST` | iframe reload 或断线 | `true` |
| `COMMAND_PERMISSION_DENIED` | 能力拥有者拒绝执行 | 通常 `false` |
| `COMMAND_EXECUTION_FAILED` | 未分类业务失败 | 视业务而定 |

领域可以定义更具体的稳定错误码，并保留 `details` 和 `state`。所有错误字段必须可 JSON 序列化。

未包装异常会由 transport 转成 `COMMAND_EXECUTION_FAILED`，但业务可预期错误不应依赖这一兜底。

## 10. 返回结果设计

结果应小、稳定、可序列化，并表达完成后的事实，而不是返回内部对象：

```ts
return {
    matched: 2,
    items: [
        { id: 'resource-1', name: 'Workspace', size: 1024 },
        { id: 'resource-2', name: 'Output', size: 2048 },
    ],
};
```

避免返回：

- `undefined` 混在对象属性中；
- `Map`、`Set`、class 实例；
- DOM、Window、事件对象；
- MobX observable；
- Promise、函数或 AbortSignal；
- 无边界的大数据集合；
- 原始错误对象和堆栈。

读取大量数据时应设计 `offset/limit` 或领域查询条件，并设置明确上限。

## 11. 表格等领域 Command

领域能力可以提供独立适配函数，但不能扩展通信 Client 的公共 API：

```ts
registerTableCommands(client, moduleId, tableControllerRegistry);
registerGraphCommands(client, moduleId, graphController);
```

适配函数内部最终仍调用：

```ts
client.registerCommand(definition, handler);
```

不要给 `ModuleAgentCommandClient` 增加：

```ts
client.registerTableCommands(...);
client.registerGraphCommands(...);
```

表格已经有统一 `registerTableCommands()`、`observeTableCommands()`、revision、targetId、取消和参数校验机制。新增表格能力应优先扩展 `AgentTable` 领域模型，而不是在业务 Module 旁路注册一个功能重复的 Command。`observeTableCommands()` 会把内部 `table.*` capability 转换为 `${moduleId}.table.*` 完整名称，只有完整名称才能传给 `help { command }` 或实际执行。

## 12. 不需要修改的文件

新增普通 Command 时，以下层通常不需要任何修改：

```text
modules/insight_web_agent/server/native-agent/tools/msinsightTools.mjs
modules/insight_web_agent/server/services/frontendCommandService.mjs
modules/insight_web_agent/src/bridge/frontendAgentCommandTransport.ts
modules/lib/src/FrontendAgentCommand/protocol.ts
modules/framework/src/agent/frontendAgentCommandController/*Transport.ts
```

只有在改变整个 Command 协议、路由或安全模型时，才应修改这些文件。

如果发现“新增一个业务 Command 需要同时修改 Module、Framework、Agent iframe、Server 和 Native Tool”，说明实现已经绕过统一架构，应停止并重新选择注册层级。

## 13. 测试建议

### 13.1 definition 和参数校验

至少覆盖：

- 名称使用正确命名空间；
- `help` 中的 schema 与 parser 一致；
- 必填字段缺失；
- 类型、范围、枚举错误；
- 未知字段被拒绝；
- 非法参数不会调用真实业务执行器。

### 13.2 业务执行

至少覆盖：

- 正常结果可序列化；
- not-ready 和 busy 返回正确结构化错误；
- signal 取消后底层请求被取消；
- 无法物理取消时，迟到结果不提交；
- requestId 正确传给需要幂等的下层；
- 业务状态变化时结果和 observation 一致。

### 13.3 生命周期和目录

通用 Client 和 Controller 已覆盖连接协议。新增动态 Command 时还应覆盖：

- 注册后进入完整快照；
- 注销后从完整快照移除；
- 普通 loading 不触发反复注册；
- 非 active Module 的 Command 不出现在可见目录；
- iframe reload 后旧 connectionToken 的请求和响应无效。

参考测试：

- `modules/lib/src/ModuleAgentCommandClient/client.test.ts`；
- `modules/framework/src/agent/frontendAgentCommandController/CommandCatalog.test.ts`；
- `modules/insight_web_agent/server/test/services/frontendCommandService.test.mjs`。

若业务包的 Jest 无法解析 lib 子路径，需要在该包 `package.json` 的 `moduleNameMapper` 中映射：

```json
{
  "@insight/lib/FrontendAgentCommand": "<rootDir>/../lib/src/FrontendAgentCommand/index.ts",
  "@insight/lib/ModuleAgentCommandClient": "<rootDir>/../lib/src/ModuleAgentCommandClient/index.ts"
}
```

## 14. 联调步骤

### 14.1 确认握手和目录快照

1. 启动 Framework 和目标 Module；
2. 打开目标 Module，使其成为 active Module；
3. 打开 Framework 顶部 **Window Messages**；
4. 确认存在以下 Module 链路：

   ```text
   moduleAgent/hello
   moduleAgent/ready
   moduleAgent/commandsChanged
   ```

5. `commandsChanged` 必须携带当前 `connectionToken` 和完整定义数组。

### 14.2 通过 Agent 验证发现

以下是模型 Tool payload，不是浏览器全局 JavaScript 函数：

```ts
msinsight({ command: 'help', args: {} });
```

确认轻量目录包含新 Command，再查询完整定义：

```ts
msinsight({
    command: 'help',
    args: { command: 'YourModule.resource.query' },
});
```

确认名称、描述和 `inputSchema` 正确。

### 14.3 执行并检查完整链路

```ts
msinsight({
    command: 'YourModule.resource.query',
    args: { keyword: 'workspace', limit: 20 },
});
```

按同一个 `requestId` 检查：

```text
AcpSession → Framework: frontendAgent/executeCommand
Framework → YourModule: moduleAgent/executeCommand
YourModule → Framework: moduleAgent/commandResponse 或 commandError
Framework → AcpSession: frontendAgent/commandResponse 或 commandError
```

若 Command 不可见，按顺序检查：

1. Module 是否是当前 active Module；
2. Client 是否调用了 `start()`；
3. `hello/ready` 是否使用同一个 `connectionToken`；
4. `commandsChanged` 是否包含该 Command；
5. 名称是否严格使用 `${moduleId}.` 前缀；
6. Command 是否被组件 effect 意外注销。

若 Command 可见但执行失败，检查：

1. `help { command }` 的 schema 与实际 args 是否一致；
2. 当前业务状态是否 ready/busy；
3. deadline 是否已经到期；
4. handler 是否正确传播 signal；
5. 返回值和错误是否可序列化；
6. 执行期间是否发生 active Module 切换或 iframe reload。

## 15. 常见错误

### 15.1 为每个 Command 新增 Native Tool

错误：

```text
msinsight_queryResource
msinsight_openGraph
msinsight_refreshTable
```

正确：保留一个 `msinsight` Tool，动态能力通过 `help` 和 namespaced Command 暴露。

### 15.2 在多层重复注册

错误：Module、Framework、Agent iframe、Server 和 Native 各声明一次同一能力。

正确：只在能力拥有者处注册一次，其他层通用转发。

### 15.3 认为 inputSchema 会自动校验

错误：只写 schema，handler 直接类型断言。

正确：schema 用于发现，handler 或领域适配器执行同等运行时校验。

### 15.4 用 busy/loading 增删 Command

错误：请求开始时注销，结束时重新注册，导致目录快照频繁变化。

正确：Command 保持存在，执行时返回结构化 busy/not-ready 错误。

### 15.5 忽略 AbortSignal

错误：上层已经取消，底层仍提交结果并修改 UI。

正确：向下传播 signal，并在最终提交前检查取消和请求代次。

### 15.6 返回内部对象

错误：返回 store、DOM、class 实例或无限制原始数据。

正确：显式构造小型 JSON 摘要，并对列表设置上限。

### 15.7 注册错误命名空间

错误：Module 注册 `framework.*`，或 Framework 注册 `MemScope.*`。

正确：所有者与命名空间严格一致，冲突应立即失败。

### 15.8 把审批扩展点当成已实现能力

错误：设置 `approval: 'required'` 后认为 Controller 已经会弹出审批 UI。

正确：审批和 `approvalGrant` 仍是后续扩展，当前不要依赖该字段承载安全决策。

## 16. 提交前验收清单

- [ ] 已确认 Command 属于 Framework 还是具体 Module；
- [ ] 名称使用 `framework.` 或 `${moduleId}.` 命名空间；
- [ ] 没有修改 Native Tool、Broker、Agent relay 或 message protocol；
- [ ] definition 的 title、description 和 inputSchema 可用于独立调用；
- [ ] handler 拒绝缺失、错误和未知参数；
- [ ] 当前业务可用性在执行时复核；
- [ ] handler 传播并遵守 `context.signal`；
- [ ] 没有重置端到端 deadline；
- [ ] 结果和错误全部可序列化；
- [ ] observation 提供调用所需的稳定 ID 或 revision；
- [ ] busy/loading 不驱动 Command 高频注册和注销；
- [ ] 测试覆盖参数、业务执行、取消和生命周期；
- [ ] 已通过 `help`、`observe` 和 Window Messages 验证完整链路。

## 17. 参考实现

| 职责 | 文件 |
| --- | --- |
| Command 类型与错误 | `modules/lib/src/FrontendAgentCommand/` |
| Module 注册与完整快照 | `modules/lib/src/ModuleAgentCommandClient/client.ts` |
| Framework 统一门面 | `modules/framework/src/agent/frontendAgentCommandController/FrontendAgentCommandController.ts` |
| 可见目录和命名校验 | `modules/framework/src/agent/frontendAgentCommandController/CommandCatalog.ts` |
| MemScope Module runtime | `modules/leaks/src/agent/runtime.ts` |
| 表格领域适配器 | `modules/lib/src/AgentTable/registerTableCommands.ts` |
| Agent iframe relay | `modules/insight_web_agent/src/bridge/frontendAgentCommandTransport.ts` |
| Native 固定 Tool | `modules/insight_web_agent/server/native-agent/tools/msinsightTools.mjs` |
