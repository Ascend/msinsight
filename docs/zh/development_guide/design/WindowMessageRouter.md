# Window 消息路由与调试器设计

## 背景

MindStudio Insight framework 与多个业务 iframe、Agent View iframe 通过 `window.postMessage` 通信。历史实现由 Connector、Agent 工具桥和模块桥分别注册原生 `message` 监听器，存在以下问题：

- 同一个 Window 上存在多个独立消息入口，协议分发边界不清晰；
- legacy Connector 使用 JSON 字符串，Agent 协议使用对象，接收方重复处理解析；
- iframe 握手、请求和响应缺少统一观测手段，异常发生时难以判断消息是否发出或返回；
- 直接读取跨域 `WindowProxy` 属性会触发浏览器同源策略错误。

## 目标

1. Connector、ACP Agent 和 Module Agent 在每个 Window 共享一个原生 `message` 监听入口；
2. 在统一入口完成 legacy JSON 消息规范化，并按协议过滤器分发；
3. 保持既有 Connector、ACP Agent 和 Module Agent 协议互不干扰；
4. 记录经过当前 Window 的入站消息和显式埋点的出站消息；
5. 提供有界、可筛选、可展开原始载荷的调试界面；
6. 调试逻辑不得改变消息时序、协议载荷或安全校验。

## 总体设计

### SharedWindowMessageRouter

`WindowMessageRouter` 以 Window 为隔离单位，通过 `WeakMap<Window, WindowMessageRouter>` 复用实例。Connector、ACP Agent 和 Module Agent 的首个订阅者注册时安装原生 `message` listener，最后一个订阅者移除时卸载 listener。VS Code adapter 等独立宿主协议不在本次迁移范围内。

消息进入 Router 后按以下顺序处理：

1. 如果 `event.data` 是 JSON 字符串，解析并克隆 `MessageEvent`，保留 `origin`、`source`、`ports` 等元数据；
2. 写入 Window 消息调试记录；
3. 依次执行订阅者的 filter；
4. 将规范化事件交给匹配的 listener；
5. 单个订阅者异常只记录到控制台，不影响其他订阅者。

### 协议分流

- legacy Connector：接收不含 `channel` 的消息；
- ACP Agent：使用 `channel: "acpMessage"`；
- Module Agent：使用 `channel: "moduleAgentMessage"`。

Frontend Agent Command 与 Module Agent Command 协议在共享 contract 中声明 channel 常量，并通过 Router 的通用 channel envelope/filter 发送和订阅。协议自身继续负责 `event.source`、origin、requestId、deadline，以及 Module connectionToken 校验。

### 出站记录

浏览器不会在发送 Window 上产生对应的 `message` 事件，因此出站消息必须在现有 `postMessage` 之前显式记录。当前覆盖：

- legacy Connector 广播和定向发送；
- framework 向 Agent View 发送上下文；
- framework 向 Agent View 返回 Command 结果或错误；
- framework 向模块 iframe 发送 hello、observe、executeCommand 和 cancelCommand，模块向 framework 上报 commandsChanged 完整快照。

记录动作发生在确认目标 Window 存在之后、调用 `postMessage` 之前。记录失败或订阅者异常不能阻断真实发送。

## WindowMessageDebugger

调试记录、订阅和有界缓存实现在 `WindowMessageRouter/debug.ts`，由 `WindowMessageRouter/index.ts` 统一导出；它是消息路由子系统的调试能力，不再作为独立顶层模块。

framework 顶部提供 `Window Messages` 入口。调试器展示：

- 时间戳；
- 相对 framework 的 `IN`/`OUT`；
- channel；
- `source → target`；
- event；
- command、module、requestId、targetRequestId、connectionToken、sessionId、from/to、status、error 等摘要；
- 安全文本形式的原始 JSON 载荷。

支持按方向、channel 和关键字筛选，支持清空和展开详情。

### 容量限制

- 最多保留 200 条记录；
- 单条序列化载荷最多保留 50,000 字符；
- 超限载荷标记为 `truncated`；
- 记录保存序列化快照，不保存业务对象引用。

## 跨域安全

调试器不读取跨域 `WindowProxy.name`。来源识别通过比较 `event.source` 与父页面已有 iframe 的 `contentWindow`，名称从父页面 DOM 中对应 iframe 的 `name` 或 `id` 获取。

Router 不替代协议安全校验。ACP 和 Module bridge 仍必须校验：

- `event.source` 是否为预期 Window；
- `event.origin` 是否匹配预期 origin；
- requestId、deadline 和消息类型是否有效；
- Module 消息的 moduleId 与 connectionToken 是否属于当前连接代次。

原始载荷只通过 React 文本节点和 `<pre>` 渲染，不使用 HTML 注入或 Markdown 执行。

## 非目标

- 不捕获未经过当前 framework Window 的子 iframe 直连消息；
- 不捕获 Web Agent 前端与本地服务之间的 HTTP/SSE 流量；
- 不提供消息重放、修改或伪造能力；
- 不将调试记录持久化到磁盘或发送到远端。

## 典型链路

一次 `msinsight({ command: 'observe', args: {} })` 在调试器中应能按 requestId 观察到：

1. `AcpSession → framework`：`frontendAgent/executeCommand`；
2. `framework → MemScope`：`moduleAgent/observe`；
3. `MemScope → framework`：`moduleAgent/commandResponse`；
4. `framework → AcpSession`：`frontendAgent/commandResponse`。

若链路中断，可根据最后一条消息判断问题发生在 Agent relay、Controller、Module transport、业务 handler 或响应返回阶段。
