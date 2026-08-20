# Agent 回复 Action 设计

## 1. 背景

Insight Web Agent 可以连接内置 Native Agent，也可以连接任意兼容 ACP 的外部 Agent。所有 Agent 都能通过普通 `agent_message_chunk` 输出助手文本，但只有 Native Agent 能注册本地 Tool。因此，回复中的可点击页面操作不能依赖某个 Runtime 专属 Tool 或 ACP 扩展。

本设计定义一个嵌入普通助手文本的 `<insight-action>` 标记。服务端与 Session 历史仍保存原始文本；浏览器在渲染 assistant 文本时识别标记并投影为 Action。用户点击并明确同意后，Action 复用既有 Frontend Agent Command 执行页面行为。

例如，Agent 在当前 Session 中识别出疑似泄露的内存块后，可以输出“内存块 #123”。用户点击并确认后，页面执行 `MemScope.lifecycleGraph.selectBlock({ blockId: 123 })`，在 Active Memory Block Timeline Graph 中定位并高亮对应内容。

## 2. 目标

1. Native 与外部 ACP Agent 使用同一个纯文本协议展示 Action；
2. 一段 XML 只表示一个 Action，多个 Action 由 Agent 依次输出；
3. ACP、服务端消息和 Session 历史只保存原始 assistant 文本；
4. 浏览器将普通 Markdown 与 Action 按原顺序交错渲染；
5. 展示 Action 没有页面副作用，不需要用户确认；
6. 每次执行必须获得用户对精确 `command + args` 的一次性确认；
7. Action 确认与执行完全在浏览器端完成；
8. Session 恢复后重新从原始文本解析 Action，不恢复审批和执行运行态；
9. Action 执行结果只显示在当前 UI，不自动回传 Agent。

## 3. 非目标

本设计不包含：

- 修改 Agent 直接执行 Frontend Agent Command 的既有行为；
- 为 ACP 增加 Action notification 或结构化消息类型；
- 在服务端把 XML 转成 Action block；
- 注册 Native `present_action` Tool；
- 提供“始终允许”或跨点击复用授权；
- 将 Action 绑定到生成时的 active Module、数据源或页面 revision；
- 自动把 Action 执行结果发送给 Agent；
- 使用 Markdown URL 编码命令和 JSON 参数；
- 为 Command 增加默认关闭的 Action 白名单。

## 4. 文本协议

### 4.1 格式

固定 XML 外壳中包含一个 JSON 对象：

```xml
<insight-action>
{
  "label": "内存块 #123",
  "description": "在 Active Memory Block Timeline Graph 中定位并高亮内存块 #123。",
  "command": "MemScope.lifecycleGraph.selectBlock",
  "args": {
    "blockId": 123
  }
}
</insight-action>
```

JSON 只允许以下字段：

```ts
interface ActionPayload {
    label: string;
    description: string;
    command: string;
    args: Record<string, unknown>;
}
```

字段语义：

- `label`：回复中展示的简短可点击名称；
- `description`：本次行为的上下文说明，必须说明页面将发生什么变化；
- `command`：Frontend Agent Command 的完整名称；
- `args`：目标 Command 的结构化参数。

不允许 XML attributes，不允许 JSON 数组承载多个 Action，也不允许 Agent 提供 `actionId`。浏览器使用消息 text block ID 与 XML 起始偏移生成稳定 Action ID。

### 4.2 Agent 使用约束

所有 ACP Agent 通过共享 Host System Prompt 学习该协议。只有当 Agent 明确知道当前页面 Command 的完整名称和结构化参数时，才能输出 Action。禁止猜测 `command` 名称或参数；不确定时只输出普通文本。

实际 Action XML 必须直接出现在回复文本中，不能放在 fenced code block 或 inline code 中。代码范围中的 `<insight-action>` 永远按普通示例文本渲染。

## 5. 数据流

```text
任意 ACP Agent
    │ agent_message_chunk: 普通文本
    ▼
Insight Web Agent Server
    ├── 原样累积 text block
    ├── 原样发送 SSE delta
    └── 原样保存/回放 Session 文本
            ▼
MessageList
    ├── parseActionMarkup(累计文本)
    ├── 普通片段 → ReactMarkdown
    └── 合法 XML → ActionBlock
```

协议不依赖 Agent Runtime，不扩展 ACP，也不让服务端理解 Action 业务字段。

## 6. 浏览器解析

解析器是独立纯函数，输入累计 assistant 文本、是否仍在流式生成及稳定 key 前缀，输出有序的 Markdown/Action segments。

规则：

1. 一段完整且合法的 XML 生成一个 Action；
2. 一段 XML 不允许包含多个 Action；
3. fenced code 和 inline code 中的标签不参与解析；
4. 只解析 assistant 文本，用户消息中的同名 XML 始终作为普通 Markdown；
5. JSON 解析失败、字段缺失、未知字段、数组根节点、非对象 `args` 或超长字段均不得生成 Action；
6. 一段非法 XML 不影响其前后 Markdown 和其他合法 Action；
7. `label`、`description` 和 Command 结果均按纯文本渲染，不解释为 HTML。

### 6.1 流式未闭合标记

开始标签和结束标签可能落在不同 delta 中：

- 回复仍在生成且只有 `<insight-action>` 开始标签：从开始标签起暂不展示；
- 收到完整结束标签后重新解析并展示 Action；
- 回复结束后仍未闭合：按原始 Markdown 展示，不能永久吞掉内容；
- 已闭合但 payload 非法：立即按原始 Markdown 展示。

流式状态绑定当前最后一条非权限 assistant 消息，不因 Bash/文件权限卡插入而提前结束。

## 7. 浏览器端确认流程

每个 Action 组件独立维护运行态：

```text
idle
  → loading_definition
  → awaiting_approval
  → executing
  → succeeded | failed
```

用户点击 Action 后：

1. 浏览器通过既有 bridge 免确认执行 `help { command }`；
2. Framework 从当前可见 Command 目录返回可信 `CommandDefinition`；
3. 浏览器确认返回的是同名 Command 的完整可信定义；
4. 浏览器复制并冻结本次 `command + args`；
5. 在 Action 下方内联展开独立的 `ActionApprovalCard`；
6. 用户选择“同意并执行”或“取消”；
7. 同意后通过 `executeFrontendCommand` 执行冻结载荷一次；
8. 成功、返回值或错误只显示在该 Action 附近。

确认卡同时展示：

- 可信能力说明：当前 `CommandDefinition.title` 和 `description`；
- 本次行为说明：XML payload 的 `description`；
- 实际 `command`；
- 格式化后的 `args`。

`help` 是只读预检，不单独请求确认。目标 Command 的具体参数约束由既有执行链路校验。

## 8. 授权语义

- 每次点击都重新执行 `help` 预检；
- 每次同意只授权当次冻结的 `command + args`；
- 不提供“始终允许”；
- 再次点击同一 Action 必须重新确认；
- 多个 Action 可以同时展开各自的确认卡；
- 进入执行状态后禁用重复提交；
- 审批状态只存在于浏览器组件，不写入服务端 Session；
- Action 展示不需要审批，只有目标 Command 执行需要审批。

本设计不绑定生成时页面上下文。点击时只依据当前 `help` 结果和实际 Command 路由；若页面已切换导致 Command 不可用，预检或执行会失败。

## 9. Session 恢复

Session 只保存包含 XML 的原始 assistant 文本。恢复后，浏览器使用同一解析器重新派生：

- `actionId`；
- `label`；
- `description`；
- `command`；
- `args`。

不恢复：

- 已展开状态；
- 用户同意或取消；
- 执行中状态；
- 上次成功或失败结果。

因此历史 Action 恢复后重新处于 `idle`，再次点击仍需完整预检和确认。

## 10. 安全边界

1. 只解析 assistant 文本，用户消息不能创建 Action；
2. 代码范围中的协议示例不能创建 Action；
3. XML payload 是不可信数据，严格限制字段、类型与长度；
4. 可信能力说明来自 Framework 当前 Command 目录，不能由 Agent 覆盖；
5. 目标 Command 在真正执行时仍由 `FrontendAgentCommandController` 检查当前可见性和路由；
6. Action 不接受脚本 URL、HTML、XML attributes 或任意浏览器回调；
7. Action 审批不改变 Agent 现有的直接 Command 执行语义。

## 11. 示例

Agent 先输出：

```text
当前 Session 中发现三个疑似泄露的内存块，可以分别定位查看：
```

随后依次输出三段 `<insight-action>`。UI 按文本顺序展示三个独立 Action。点击“内存块 #123”并确认后，执行：

```ts
executeFrontendCommand(
    'MemScope.lifecycleGraph.selectBlock',
    { blockId: 123 },
    requestId,
    deadline,
);
```

MemScope 继续使用既有 `pendingBlockLocateId` 和 graph worker 选择链路完成滚动、定位与高亮；回复 Action 层不感知内存块或 Timeline Graph 的内部状态。
