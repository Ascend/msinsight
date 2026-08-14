# 为业务表格接入 Agent Command

## 1. 适用范围

本文说明如何把新业务表格接入现有 Agent Command 基础设施。协议与状态语义见 [Agent 通用表格 Command 设计](./design/AgentTableCommand.md)，通用通信边界见 [Frontend Agent Command 架构](./design/FrontendAgentCommandArchitecture.md)。

接入后，Agent 可以通过内置 `observe` 发现表格，并使用 `help` 查询 `${moduleId}.table.*` Command 的完整 schema。

## 2. 接入原则

1. 在持有真实业务状态的组件中显式创建 Controller，不扫描 DOM。
2. 用户操作和 Agent Command 复用同一个可等待业务事务。
3. 只发布稳定状态；loading 等瞬时状态不推进 revision。
4. observation 和结果必须可 JSON 序列化。
5. capability 只声明已经实现的业务能力。
6. `visible` 表达真实可见性；`ready/busy` 只影响执行，不高频增删 Command。
7. 取消、超时或抢占后，迟到结果不得提交。

## 3. 接入链路

```text
Business Table
    → TableControllerAdapter
    → createAgentTableController
    → useAgentTableController
    → module TableControllerRegistry
    → registerTableCommands
    → ModuleAgentCommandClient.registerCommand
```

业务模块不需要修改 framework、Web Agent server、Native Agent 或 transport。

## 4. 创建 Module 级 Registry 和 Client

一个业务 iframe 只创建一个 Module Client，同一 Module 的表格共享一个 Registry：

```ts
import {
    observeTableCommands,
    registerTableCommands,
    TableControllerRegistry,
} from '@insight/lib/AgentTable';
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';

const MODULE_ID = 'YourModule';
export const moduleTableControllerRegistry = new TableControllerRegistry();

const moduleAgentCommandClient = new ModuleAgentCommandClient({
    moduleId: MODULE_ID,
    observe: () => ({
        moduleId: MODULE_ID,
        observedAt: Date.now(),
        tables: observeTableCommands(MODULE_ID, moduleTableControllerRegistry),
    }),
});

registerTableCommands(
    moduleAgentCommandClient,
    MODULE_ID,
    moduleTableControllerRegistry,
);
```

Module 应用挂载时启动，卸载时停止：

```ts
useEffect(() => moduleAgentCommandClient.start(), []);
```

不要在单张表格组件中创建 Client，也不要给 Client 增加 `registerTableCommands()` 成员方法；表格接入保持为独立领域适配函数。

## 5. 定义稳定业务视图

```ts
interface BusinessTableView {
    columns: BusinessColumn[];
    rows: Array<Record<string, unknown>>;
    query: TableQueryState;
    total: number;
    visible: boolean;
    ready: boolean;
    busy: boolean;
}
```

在 React 组件中通过 ref 提供最新状态，同时保持 Controller 生命周期稳定：

```ts
viewRef.current = {
    columns: businessColumns,
    rows: businessRows,
    query: stableQueryRef.current,
    total: businessTotal,
    visible: panelExpanded && activeTab === 'table',
    ready: dataSourceReady && businessColumns.length > 0,
    busy: loading,
};
```

- `visible`：用户当前能看到该表格；隐藏但未卸载时为 false。
- `ready`：数据源、必要参数、首次查询和列定义已经稳定。
- `busy`：存在不能安全并发的业务事务。

可见但 not-ready/busy 的表格仍进入 observation；执行 Command 时 Registry 返回 `TABLE_NOT_READY` 或 `TABLE_BUSY`。

## 6. 收敛用户和 Agent 查询事务

```ts
const runQueryTransition = async (
    query: TableQueryState,
    context: TransitionContext,
): Promise<TableStableSnapshot> => {
    const sequence = ++latestRequestRef.current;
    setLoading(true);
    try {
        const response = await requestBusinessTable(query);
        if (context.signal.aborted) throw context.signal.reason;
        if (sequence !== latestRequestRef.current) {
            throw new AgentTableError({
                code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED,
                message: 'The table command was superseded by a newer request.',
                retryable: true,
            });
        }
        commitBusinessState(query, response);
        stableQueryRef.current = query;
        updateView(response, query);
        return publishStable();
    } finally {
        if (sequence === latestRequestRef.current) setLoading(false);
    }
};
```

底层不支持 AbortSignal 时，至少保证取消或抢占后不提交结果，并恢复最后稳定 query。

用户分页、筛选和排序也调用同一个事务：

```ts
const runUserQuery = (query: TableQueryState): void => {
    void runQueryTransition(query, {
        requestId: crypto.randomUUID(),
        transitionId: crypto.randomUUID(),
        source: 'user',
        deadline: Date.now() + 30000,
        signal: new AbortController().signal,
    }).catch(() => undefined);
};
```

不要保留一条用户查询 effect，再增加一条 Agent 专用请求旁路。

## 7. 构造稳定快照

```ts
const getSnapshot = (): TableStableSnapshot => ({
    state: {
        query: viewRef.current.query,
        total: viewRef.current.total,
        rowCount: viewRef.current.rows.length,
        selectedRowIds: [],
        expandedRowIds: [],
    },
    columns: toAgentColumns(viewRef.current.columns),
    capabilities: [
        'table.setQuery',
        'table.setSort',
        'table.clearSort',
        'table.goToPage',
        'table.setPageSize',
        'table.refresh',
        'table.getDisplayedData',
    ],
    dataAccess: {
        maxRowsPerRequest: 100,
        availableRows: viewRef.current.rows.length,
    },
});
```

要求：

- 不包含 loading 起始状态或临时输入；
- `rowCount` 是当前已加载行数，`total` 是当前 query 下总数；
- selection/expansion 不支持时返回空数组；
- capability 与 adapter 一一对应；
- 所有字段可 JSON 序列化。

## 8. 映射列与展示数据

列必须使用稳定业务 key，并只声明真实支持的排序、筛选能力：

```ts
const toAgentColumns = (columns: BusinessColumn[]): TableColumnDefinition[] =>
    columns.map(column => ({
        columnId: column.key,
        title: column.title,
        dataType: column.numeric ? 'number' : 'string',
        readable: !column.sensitive,
        sortable: column.sortable,
        filterOperators: column.searchable ? ['contains'] : undefined,
    }));
```

`getDisplayedData` 显式重建允许读取的数据：

```ts
const getDisplayedData = async (
    offset: number,
    limit: number,
): Promise<DisplayedDataResult> => {
    const rows = viewRef.current.rows.slice(offset, offset + limit).map(row => ({
        rowId: String(row.id),
        cells: {
            name: { value: String(row.name ?? '') },
            size: { value: Number(row.size ?? 0) },
        },
    }));
    return {
        columns: toAgentColumns(viewRef.current.columns),
        rows,
        offset,
        returned: rows.length,
        available: viewRef.current.rows.length,
        hasMore: offset + rows.length < viewRef.current.rows.length,
    };
};
```

`rowId` 必须非空、唯一且稳定，不能使用当前页数组下标。不要返回原始业务 row、ReactNode、DOM 或 MobX store。

## 9. 创建并注册 Controller

```ts
const controller = useMemo(() => createAgentTableController({
    tableKey: 'your-module.resource-list',
    title: 'Resource List',
    getAvailability: () => ({
        visible: viewRef.current.visible,
        ready: viewRef.current.ready,
        busy: viewRef.current.busy,
    }),
    getSnapshot,
    subscribeStable,
    runQueryTransition,
    getDisplayedData,
    copyDisplayedData: async () => {
        await tableRef.current?.copy();
        return {
            rowCount: viewRef.current.rows.length,
            columnCount: viewRef.current.columns.length,
        };
    },
    cancel: cancelTransition,
}), [businessSession]);

useAgentTableController(controller, moduleTableControllerRegistry);
```

`tableKey` 表达表格业务类型，不包含随机数、页码、项目路径或数据源 ID。Registry 会为每次挂载生成独立 `targetId`。

capability 对应关系：

| capability | adapter |
| --- | --- |
| query、分页、排序、筛选、refresh | `runQueryTransition` |
| `table.getDisplayedData` | `getDisplayedData` |
| `table.getFilterOptions` | `getFilterOptions` |
| `table.copy` | `copyDisplayedData` |
| `table.setSelectedRows` | `runSelectionTransition` |
| `table.setExpandedRows` | `runExpansionTransition` |
| `table.invokeCellCommand` | `runCellCommandTransition` |

## 10. 发布稳定状态

```ts
const listenersRef = useRef(new Set<(snapshot: TableStableSnapshot) => void>());

const subscribeStable = (listener: (snapshot: TableStableSnapshot) => void): (() => void) => {
    listenersRef.current.add(listener);
    return () => listenersRef.current.delete(listener);
};

const publishStable = (): TableStableSnapshot => {
    const snapshot = getSnapshot();
    listenersRef.current.forEach(listener => listener(snapshot));
    return snapshot;
};
```

只在完整业务状态原子提交后发布。Registry 会比较快照并在内容变化时推进 revision；capability 或 visible 的结构性变化会触发 Module Command 快照同步。

## 11. 错误与取消

业务层使用 `AgentTableError`：

```ts
throw new AgentTableError({
    code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED,
    message: 'The command was superseded by a newer user request.',
    retryable: true,
});
```

`cancel(requestId)` 至少应：

- 禁止该请求继续提交；
- 恢复最后稳定 query；
- 清理 loading；
- 底层支持取消时终止真实请求。

Registry 统一处理 not-ready、busy、stale revision、deadline、队列取消和表格卸载。

## 12. 联调

1. 打开目标 Module 和表格。
2. 打开 framework 顶部 **Window Messages**。
3. 调用：

   ```ts
   msinsight({ command: 'help', args: {} });
   msinsight({ command: 'observe', args: {} });
   ```

4. 从 observation 获取 `targetId` 和 `revision`。
5. 查询具体 Command schema：

   ```ts
   msinsight({
       command: 'help',
       args: { command: 'YourModule.table.getDisplayedData' },
   });
   ```

6. 使用 `targetId + expectedRevision` 执行 Command，再次 observe 验证 UI 和 revision。
7. 按 requestId 检查 `AcpSession → framework → YourModule → framework → AcpSession`。

若表格未出现，依次检查 Client 是否完成握手、Controller 是否注册到正确 Registry、`visible` 是否为 true，以及 observation provider 是否调用 `observeTableCommands(moduleId, registry)`。

表格 observation 的 `commands` 已是完整可执行名称，例如 `YourModule.table.setSort`。不要把 Registry 内部的 `table.setSort` capability 直接传给 `help`。

## 13. 验收清单

- [ ] 使用稳定 `tableKey`；
- [ ] 复用 Module 级 Registry 和 Client；
- [ ] 通过独立 `registerTableCommands()` 接入；
- [ ] 用户与 Agent 共用可等待事务；
- [ ] visible、ready、busy 语义正确；
- [ ] 快照、参数和结果可序列化；
- [ ] rowId 非空、唯一且稳定；
- [ ] capability 与 adapter 一一对应；
- [ ] stale、取消、超时和抢占不会提交迟到结果；
- [ ] unmount 会注销 Controller；
- [ ] 测试覆盖 Controller、事务和生命周期。

## 14. 参考实现

| 职责 | 文件 |
| --- | --- |
| Module Registry、Client、表格适配注册 | `modules/leaks/src/agent/runtime.ts` |
| MemScope 表格领域适配器 | `modules/leaks/src/agent/systemTableController.ts` |
| Block View | `modules/leaks/src/components/BlocksTable.tsx` |
| Event View | `modules/leaks/src/components/EventsTable.tsx` |
| Registry | `modules/lib/src/AgentTable/TableControllerRegistry.ts` |
| Command 定义与校验 | `modules/lib/src/AgentTable/commands.ts` |
| Command 适配器 | `modules/lib/src/AgentTable/registerTableCommands.ts` |
