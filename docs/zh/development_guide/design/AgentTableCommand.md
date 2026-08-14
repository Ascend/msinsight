# Agent 通用表格 Command 设计

## 1. 背景与目标

业务 Module 运行在 framework 管理的 iframe 中，Native Agent 无法直接访问表格的 React、MobX 或 Controller 状态。表格能力因此通过 [Frontend Agent Command 架构](./FrontendAgentCommandArchitecture.md) 暴露：模型只调用固定 `msinsight` Tool，framework 统一发现和路由动态 Command，表格领域适配器负责业务语义。

目标：

- 业务容器显式注册表格，不扫描 DOM 或自动暴露所有通用表格；
- 统一筛选、单列排序、分页、刷新、展示数据读取、复制、选择、展开和单元格语义 Command；
- 用户操作与 Agent Command 共用同一个可等待业务事务；
- 使用 `targetId + expectedRevision` 防止在过期状态上执行；
- 支持按表串行、跨表并行、deadline、取消和结构化错误；
- 只发布稳定且可序列化的状态。

首个接入样板是 MemScope System View 的 Block View 与 Event View。

## 2. 非目标

- 自动注册全部 `ResizeTable`；
- DOM 查询、坐标点击或键盘模拟；
- 全量导出或跨页复制；
- 任意布尔筛选表达式树或多列排序；
- 通用单元格编辑、删除、提交、启动或终止任务；
- 让通信层认识 Table 领域对象。

## 3. 运行时拓扑

```text
Blade / LLM
    │ msinsight({ command, args })
    ▼
Native Agent → Web Agent server → Agent iframe
    │ executeCommand / cancelCommand
    ▼
FrontendAgentCommandController
    │ active Module route + connectionToken
    ▼
ModuleAgentCommandClient
    │ registerTableCommands(...)
    ▼
TableControllerRegistry
    │ targetId
    ▼
TableController → TableControllerAdapter → business state/query/UI
```

职责边界：

- Native、Agent iframe 和 framework transport 不理解表格语义；
- `registerTableCommands()` 将内部 `table.*` capability 转换为 `${moduleId}.table.*` Command；
- `TableControllerRegistry` 只存在于业务 Module iframe；
- 每张挂载表格对应一个 `TableController`；
- 通用表格组件不承担注册和业务事务协调。

## 4. Command 发现与调用

表格内部 capability ID：

```text
table.setQuery
table.setFilters
table.clearFilters
table.setSort
table.clearSort
table.goToPage
table.setPageSize
table.refresh
table.getDisplayedData
table.getFilterOptions
table.copy
table.setSelectedRows
table.setExpandedRows
table.invokeCellCommand
```

`registerTableCommands()` 汇总当前可见表格的 capability，并通过 `ModuleAgentCommandClient.registerCommand()` 注册 namespaced Command，例如：

```text
MemScope.table.getDisplayedData
MemScope.table.refresh
```

完整 Command 参数由 `help { command }` 返回。每个表格 Command 都包含路由字段：

```ts
interface TableCommandArgs {
    targetId: string;
    expectedRevision: number;
    // 其余字段由具体 table.* Command 定义
}
```

适配器移除路由字段后构造内部请求：

```ts
interface TableCommandRequest {
    targetId: string;
    expectedRevision: number;
    commandId: TableCommandId;
    args: JsonObject;
    requestId: string;
    deadline: number;
}
```

除实时 `observe` 外，所有表格读取和变更 Command 都校验 `expectedRevision`。

## 5. Observation

Module observation provider 调用 `TableControllerRegistry.observe()`。每个可见表格返回：

```ts
interface TableCommandObservation extends Omit<TableStableSnapshot, 'capabilities'> {
    protocolVersion: 1;
    targetId: string;
    tableKey: string;
    title: string;
    revision: number;
    availability: {
        visible: boolean;
        ready: boolean;
        busy: boolean;
    };
    commands: string[];
}
```

规则：

- `visible: false` 的表格不进入 observation，也不贡献 Command；
- `ready` 和 `busy` 是瞬时执行状态，不驱动 Command 目录增删；
- 可见但尚未 ready 的表格仍可观察，执行时返回 `TABLE_NOT_READY`；
- busy 表格仍可观察最后稳定快照，执行时返回 `TABLE_BUSY`；
- observation 不包含原始业务 row；
- Registry 内部 `capabilities` 使用 `table.*` ID，不能直接暴露给模型；
- `observeTableCommands(moduleId, registry)` 将它们转换为可直接传给 `help` 和 `msinsight` 的完整 `commands`，例如 `MemScope.table.setSort`。

## 6. 稳定快照与 revision

```ts
interface TableStableSnapshot {
    state: {
        query: TableQueryState;
        total: number;
        rowCount: number;
        selectedRowIds: string[];
        expandedRowIds: string[];
    };
    columns: TableColumnDefinition[];
    capabilities: TableCommandId[];
    constraints?: TableInteractionConstraints;
    dataAccess: {
        maxRowsPerRequest: number;
        availableRows: number;
    };
}
```

推进 revision 的变化：

- 查询状态、展示数据或总数变化；
- 列定义、capabilities、选择或展开状态变化；
- 单元格 Command 导致表内稳定状态变化；
- `table.refresh` 成功，即使返回相同快照。

不推进 revision：

- loading 开始、临时输入、hover 或焦点；
- 请求失败、取消、超时或被取代；
- `getDisplayedData`、`getFilterOptions`、`copy`；
- 只产生导航或弹窗等表外效果的 Command。

## 7. 列、筛选与排序

业务容器必须显式声明可暴露列：

```ts
interface TableColumnDefinition {
    columnId: string;
    title: string;
    dataType: 'string' | 'number' | 'boolean' | 'date';
    unit?: string;
    readable: boolean;
    sortable?: boolean;
    filterOperators?: TableFilterOperator[];
    allowedValues?: Array<{ value: JsonPrimitive; label: string }>;
    filterOptions?: 'dynamic';
    cellCommands?: CellCommandDefinition[];
}
```

约束：

- `columnId` 使用稳定业务 key；
- 只声明业务真正支持的排序和筛选操作符；
- 同一列最多一个筛选条件，不同列固定 `AND`；
- `setFilters` 整体替换；
- 首版只支持单列排序；
- 筛选、排序或页大小变化重置到第 1 页；
- `setQuery` 原子更新多个字段，只触发一次业务查询；
- 规范化目标状态不变时返回 `noOp: true`。

## 8. 展示数据与行级能力

`table.getDisplayedData` 只读取当前已展示或已加载的数据，不请求其他分页。业务适配器显式映射可读列，不能直接返回原始 row、ReactNode 或 MobX 对象。

每个 `rowId` 必须非空、唯一、稳定且不可使用当前数组下标。

选择和展开：

- `setSelectedRows`、`setExpandedRows` 整体替换集合；
- `constraints` 声明单选/多选及数量上限；
- 行数据声明当前是否可选或可展开；
- 非法集合原子失败，不静默修正。

单元格 Command：

```ts
interface CellCommandDefinition {
    cellCommandId: string;
    title: string;
    effect: 'view' | 'navigation' | 'table-state';
    inputSchema?: JsonObject;
}
```

调用必须同时提供 `rowId + columnId + cellCommandId`，并在执行时复核列声明和该单元格的 `availableCommands`。禁止把任意 URL、路由或 JavaScript 作为通用输入。

## 9. 事务、并发与取消

Registry 对同一 `targetId` 串行，不同表格可以并行。进入业务执行器前依次校验：

```text
target → visible/ready/busy → deadline → expectedRevision → capability
```

每次执行创建 `TransitionContext`：

```ts
interface TransitionContext {
    requestId: string;
    transitionId: string;
    source: 'agent' | 'user';
    deadline: number;
    signal: AbortSignal;
}
```

业务执行器必须在提交前检查 signal 和自身请求代次。底层请求不能物理取消时，也必须禁止迟到结果提交。Registry 在 cancel、deadline 或卸载时同时 abort signal 并调用可选 `controller.cancel(requestId)`。

## 10. 结果与错误

成功结果：

```ts
interface TableCommandResult {
    status: 'completed';
    targetId?: string;
    targetStatus?: 'available' | 'unavailable';
    revision?: number;
    noOp?: boolean;
    state?: TableStableState;
    result?: JsonValue;
    effect?: TableCommandEffect;
    requiresObserve?: boolean;
}
```

主要错误码：

```text
TABLE_TARGET_UNAVAILABLE
TABLE_NOT_READY
TABLE_BUSY
TABLE_STATE_STALE
TABLE_CAPABILITY_UNSUPPORTED
TABLE_COMMAND_INVALID
TABLE_COLUMN_UNKNOWN
TABLE_FILTER_UNSUPPORTED
TABLE_FILTER_VALUE_INVALID
TABLE_SORT_UNSUPPORTED
TABLE_PAGE_OUT_OF_RANGE
TABLE_ROW_UNKNOWN
TABLE_SELECTION_LIMIT_EXCEEDED
TABLE_EXPANSION_LIMIT_EXCEEDED
TABLE_COMMAND_SUPERSEDED
TABLE_COMMAND_TIMEOUT
TABLE_COMMAND_CANCELLED
TABLE_TRANSITION_FAILED
TABLE_COPY_FAILED
TABLE_CONNECTION_LOST
```

所有传输层保留 `code/message/retryable/details/state`，不压缩成普通字符串。

## 11. MemScope 样板

- Module：`MemScope`；
- 表格：Block View、Event View；
- `tableKey`：`memscope.system.blocks`、`memscope.system.events`；
- Module Registry 与 Client：`modules/leaks/src/agent/runtime.ts`；
- 领域适配器：`modules/leaks/src/agent/systemTableController.ts`；
- 组件接入：`BlocksTable.tsx`、`EventsTable.tsx`。

两张表只声明真实支持的查询、筛选、排序、分页、刷新、展示数据读取和复制能力，不声明选择、展开或单元格 Command。
