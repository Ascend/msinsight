# DB 场景 PMU 与 Communication 详情时间戳匹配

## 核心数据不变量

```text
globalTaskId + TASK 时间范围 [startNs, endNs]
```

可以唯一确定一个 TASK 和一个 COMMUNICATION_TASK_INFO / TASK_PMU_INFO 数据集。

具体含义：

- 同一 `globalTaskId` 的不同 TASK 调用，其时间范围 [startNs, endNs] 不会重叠。
- 每一条 `COMMUNICATION_TASK_INFO` 记录的 `timestampNs` 落在且仅落在一个 TASK 的 [startNs, endNs] 内。
- 一个 TASK 对应 0 或 1 条 `COMMUNICATION_TASK_INFO` 记录。
- 一个时间点可以对应多条 `TASK_PMU_INFO` 指标行（同一组指标的不同名称），这些行组成当前 TASK 的完整 PMU 指标组。

因此使用 `globalTaskId` + 闭区间 `[TASK.startNs, TASK.endNs]` 作为匹配键，Communication 和 PMU 的精确查询均是唯一且确定的。

## 背景

DB 场景下，多次 Device TASK 可能复用同一个 `globalTaskId`。因此 `globalTaskId` 只用于划分候选集合，
不能单独确定明细属于哪次 TASK。`COMMUNICATION_TASK_INFO` 和 `TASK_PMU_INFO` 均只增加一个
`timestampNs` 字段，数据模型不引入第二个时间字段。

## Communication 匹配模型

有 `timestampNs` 的新表按调用序号配对，并构造 `TASK.rowid -> COMMUNICATION_TASK_INFO` 映射：

- 新表 TASK 按 `(startNs, ROWID)` 排序，通信行按 `(timestampNs, ROWID)` 排序，第 n 行与第 n 个 TASK 配对。
- 新表配对后还必须校验 `TASK.startNs <= timestampNs <= TASK.endNs`，区间两端均闭合；校验失败不改配对序号。
- 旧表不使用 ROWID 猜测对应关系，单详情恢复原逻辑，使用同 `globalTaskId` 查询结果第一条。
- 单切片详情通过 `TASK.rowid` 查精确映射，不在每个 TASK 上线性扫描候选行。
- 批量切片名称只读取 `taskType`。业务保证同一 `globalTaskId` 下所有 Communication 明细的
  `taskType` 相同，因此保持原 `globalTaskId -> taskType` 哈希映射，无需执行序号配对。
- Plane 简单切片不读取 Communication 详情，只按目标 `groupName/planeId` 得到 `globalTaskId` 集合，再查询
  对应 TASK。该路径保持原逻辑，不执行序号配对；业务必须保证重复的同一 `globalTaskId` 不会跨不同 plane。
- bandwidth 直接取已配对详情行，不再单独查询，保证详情字段来自同一次通信调用。
- 旧表或新表配对失败时，详情字段使用兜底结果第一条。只有查询到多条候选明细时，才在 args 中写入
  `_ambiguousKeys`（这些字段名），供前端在对应字段上提示取值可能不属于当前 TASK；仅一条候选时无歧义。

## PMU 匹配模型

代码按固定步骤组织，便于阅读：

1. 新表直接使用传入 TASK 的 `globalTaskId` 和 `[startNs, endNs]` 查询 PMU。
2. 旧表直接调用 `QueryAllPmuInfo`，恢复原始的 globalTaskId 全量查询。
3. 新表精确查询为空时也调用 `QueryAllPmuInfo` 兜底。
4. `AppendPmuInfoToArgs` 统一写入详情 JSON；同名指标存在多个候选值时，通过 `_ambiguousKeys` 标记该指标名。

### 新表

传入的目标 TASK 已包含完整匹配条件，因此直接查询：

```text
globalTaskId 相同，并且 TASK.startNs <= PMU.timestampNs <= TASK.endNs
```

查询结果即为该 TASK 对应的完整 PMU 指标。结果为空时恢复原始 globalTaskId 查询，避免详情空白；
兜底结果中只有同名指标出现多次时，才在 `_ambiguousKeys` 中列出该指标名。

### 旧表

旧 PMU 没有时间戳，无法可靠确认明细属于哪次 TASK 调用，因此不使用 ROWID 推测对应关系。保持原始逻辑，
返回同 `globalTaskId` 的全部 PMU；只有同名指标出现多次时，才在 `_ambiguousKeys` 中列出该指标名。

## 字段不确定标记

`args` 中 `_` 前缀的 key 是内部标记，前端不渲染为参数行。当前只有 `_ambiguousKeys`：值为不确定字段名
数组，由后端在写详情 JSON 时按实际写入的字段名收集，因此前后端不需要维护静态字段清单。

## 查询边界

入口使用 `CheckColumnExist(table, "timestampNs")` 判断数据能力：

- 纯查询函数只在 SELECT 列和排序字段上有细微差异，因此合并为一个带 `withTimestamp` 参数的 helper；
  参数为 false 时不会引用旧表不存在的 `timestampNs`。
- PMU 新表直接按时间范围查询；旧表仅按 globalTaskId 查询全部指标。

停止支持旧数据时，可以直接删除入口中的无时间戳分支。

所有需要精确配对的查询都显式选择并排序 `ROWID`。单详情查询会读取同一 `globalTaskId` 的全部 TASK 与明细，
避免仅查询目标 TASK 后把它错误地当作第一个 occurrence。

## 复杂度

设 TASK 数量为 T，Communication 行数为 C，PMU 行数为 P：

- Communication 分桶和映射构造为 `O(T + C)`，各桶排序总计 `O(T log T + C log C)`，消费映射为 `O(1)`。
- 新 PMU 由 SQLite 按 globalTaskId 和时间闭区间直接筛选，不在 C++ 构造时间戳组或排序 TASK。
- 旧 PMU 是一次 globalTaskId 查询，不执行 TASK/PMU 配对或 C++ 排序。

## 异常处理

- Communication 的 TASK/明细数量不一致或序号配对时间校验失败时记录 warning。新表随后按 TASK
  时间闭区间在尚未使用的明细中恢复，只有恰好命中一条时建立映射；0 条或多条均不建立映射。旧表不执行
  序号匹配，单详情使用同 `globalTaskId` 查询结果第一条。
- 新 PMU 精确查询为空时恢复原始 globalTaskId 查询；同名指标存在多个候选值时，通过 `_ambiguousKeys` 标记。
- 旧 PMU 直接返回同 `globalTaskId` 的全部指标；同名指标存在多个候选值时，通过 `_ambiguousKeys` 标记。
- Table handler 在查询未选择 `timestampNs` 时保留默认值 0，旧表 SQL 不引用该列。
