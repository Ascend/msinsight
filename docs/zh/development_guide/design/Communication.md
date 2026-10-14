# Communication部分设计文档

## 1. 文档目标与范围

本文说明 `cluster/communication` 页面相关的数据来源、接口命令和前后端代码入口，面向需要维护通信矩阵、通信耗时、算子列表、带宽和分布图的开发者。

- 支持 TEXT 与 DB 两种数据场景。
- 页面中的截图仅作为辅助，关键接口、路径和数据映射以正文为准。

## 2. 接口与数据映射关系

### 2.1 原始数据（ATT 处理后文件）

#### text 场景

![communication_text_data](./figures/communication_text_data.png)

#### db 场景

![communication_db_data](./figures/communication_db_data.png)

### 2.2 处理后 DB 数据内容

#### text 场景

![communication_processed_text_data](./figures/communication_processed_text_data.png)

#### db 场景

![communication_processed_db_data](./figures/communication_processed_db_data.png)

### 2.3 页面接口总览

| 页面数据 | URL 请求 | db 数据类型 | text 数据类型 | 说明 |
| --- | --- | --- | --- | --- |
| ![communication_page_data_1](./figures/communication_page_data_1.png) | `communication/matrix/bandwidthInfo` | ![communication_db_data_1](./figures/communication_db_data_1.png) | ![communication_text_data_1_1](./figures/communication_text_data_1_1.png) ![communication_text_data_1_2](./figures/communication_text_data_1_2.png) | 矩阵带宽详情。 |
| ![communication_duration_iterations_1](./figures/communication_duration_iterations_1.png) | `communication/duration/iterations` | ![communication_duration_iterations_2](./figures/communication_duration_iterations_2.png) | ![communication_duration_iterations_3](./figures/communication_duration_iterations_3.png) | 迭代列表与通信耗时范围。 |
| ![communication_matrix_group_1](./figures/communication_matrix_group_1.png) | `communication/matrix/group` | ![communication_matrix_group_2](./figures/communication_matrix_group_2.png) | ![communication_matrix_group_3](./figures/communication_matrix_group_3.png) 底层数据来源于：![communication_matrix_group_4](./figures/communication_matrix_group_4.png) | 通信矩阵分组信息。 |
| ![communication_sortOpNames_1](./figures/communication_sortOpNames_1.png) | `communication/matrix/sortOpNames` |  | ![communication_sortOpNames_2](./figures/communication_sortOpNames_2.png) 底层数据：![communication_sortOpNames_3](./figures/communication_sortOpNames_3.png) | 算子名排序与聚合结果；DB 场景是否支持需以源码实现为准。 |
| ![communication_operatorNames_1](./figures/communication_operatorNames_1.png) | `communication/duration/operatorNames` | ![communication_operatorNames_2](./figures/communication_operatorNames_2.png) | ![communication_operatorNames_3](./figures/communication_operatorNames_3.png) 数据：![communication_operatorNames_4](./figures/communication_operatorNames_4.png) ![communication_operatorNames_5](./figures/communication_operatorNames_5.png) ![communication_operatorNames_6](./figures/communication_operatorNames_6.png) | 通信耗时视图中的算子名列表。 |
| ![communication_operatorLists_1](./figures/communication_operatorLists_1.png) | `communication/operatorLists` | ![communication_operatorLists_2](./figures/communication_operatorLists_2.png) | ![communication_operatorLists_3](./figures/communication_operatorLists_3.png) 数据：![communication_operatorLists_4](./figures/communication_operatorLists_4.png) | 算子列表视图。 |
| ![communication_duration_list_1](./figures/communication_duration_list_1.png) | `communication/duration/list` | ![communication_duration_list_2](./figures/communication_duration_list_2.png) | ![communication_duration_list_3](./figures/communication_duration_list_3.png) ![communication_duration_list_4](./figures/communication_duration_list_4.png) | 通信耗时明细列表，专家建议由数据计算得到。 |
| ![communication_operatorDetails_1](./figures/communication_operatorDetails_1.png) | `communication/operatorDetails` | ![communication_operatorDetails_2](./figures/communication_operatorDetails_2.png) | ![communication_operatorDetails_3](./figures/communication_operatorDetails_3.png) | 算子详情。 |
| ![communication_distribution_1](./figures/communication_distribution_1.png) | `communication/distribution` | ![communication_distribution_2](./figures/communication_distribution_2.png) | ![communication_distribution_3](./figures/communication_distribution_3.png) | 通信分布图。 |
| ![communication_bandwidth_1](./figures/communication_bandwidth_1.png) | `communication/bandwidth` | ![communication_bandwidth_2](./figures/communication_bandwidth_2.png) | ![communication_bandwidth_3](./figures/communication_bandwidth_3.png) | 带宽分析。 |

### 2.4 代码入口

- 前端请求封装：`modules/cluster/src/utils/RequestUtils.ts`
- 后端命令常量：`server/src/modules/defs/ProtocolDefs.h`
- 后端插件：`server/src/modules/communication/CommunicationPlugin.h`
- 插件注册：`server/src/modules/Plugins.cpp`
- 协议测试：`server/src/test/modules/communication/protocol/CommunicationProtocolUtilTest.cpp`
- 请求样例：`server/src/test/test_data/request.csv`

### 2.5 Agent observation 慢卡建议

`observeCommunication().slowRankAdvice` 复用页面的 `currentState.slowRanks`，不新增请求或工具。

- `status`：`unavailable` 表示未就绪或结果不属于当前会话/集群/筛选（含非耗时分析、对比模式、数据清空、请求未完成）；`no-advice` 表示后端 `hasAdvice=false`；`empty` 表示 `hasAdvice=true` 但无慢卡；`ready` 表示有慢卡列表。不可把 `unavailable` 当成“无慢卡”。
- 就绪结果包含 `hasAdvice`、`timeUnit: "us"`、`totalRanks`、`truncated`、`ranks`；`hasAdvice=true` 时另有 `fastRankId`、`fastTotalElapseTime`。耗时保留后端微秒值，页面显示毫秒时除以 1000。
- 每卡：`rankId`、`totalElapseTime`、`totalDiffTime`、`totalOperators`、`truncated`、`operators`；每算子：`name`、`elapseTime`（当前卡耗时）、`diffTime`（耗时差）、`maxTime`（最快卡上的耗时，与页面列名一致，不表示最大耗时）。不输出内部路径或未使用的起始时间。
- 服务端目前提供 Top 3 慢卡、每卡 Top 3 算子；投影也限定 3×3，保持原顺序。若将来返回更多，`totalRanks`/`totalOperators` 保留原数量，`truncated` 明确标记省略；没有分页接口。

例如 `ranks[0]` 的 `rankId="3"`，`operators[0]={"name":"all_reduce_1","elapseTime":2000,"diffTime":1000,"maxTime":3000}`，三个耗时分别为 2ms、1ms、3ms。直接调用 `Communication.locateInTimeline({rankId:"3", operatorName:"all_reduce_1"})`；`rankId` 必须是非空字符串，原样保留 `"0"`、`"01"`、`"worker-A"` 等身份，不转为数字；仅数字 wire ID 在请求边界转为字符串。最快卡缺失时 `fastRankId` 保留空字符串，`operatorName` 必须是非空字符串且准确匹配当前有效建议。`Communication.highlightSlowRank` 仅为可选页内高亮，不是前置依赖。定位仍要求当前 Communication Duration Analysis、已解析 Cluster/Duration、非对比、目标图表 dbPath 及 Timeline 数据可用；查询完成后重新核对上下文与目标数据，取消或过期结果不跳转。Timeline 收到 `locateUnit` 后按精确 card/dbPath/process/thread 目标定位，使用已有 slice ID 复用正常点击的选中入口与泳道 `onClick`，查询并显示可用关联连线；不追加按名称选择，不保证目标一定有关联连线。连线异步结果仅在原会话和原选择仍有效时发布。

实现与测试：`modules/cluster/src/agent/communicationController.ts`、`communicationController.test.ts`；请求上下文由 `components/communication/CommunicationAnalysis.tsx` 发布，仅用于 observation 新鲜度校验，不改变 UI 或定位行为。

### 2.6 说明

- `text` 与 `db` 仅表示数据来源不同，页面能力和接口命名保持一致。
- 表格中的图片仍保留，用于辅助快速定位 UI，但不作为唯一信息来源。
- `sortOpNames` 的 DB 场景支持情况、专家建议的具体算法、每个接口的完整响应字段，需以源码和测试结果为准。

## 3. 通信算子缩略图键盘导航

`CommunicationTimeAnalysisChart.tsx` 在图表聚焦时处理键盘事件，调用同目录下的 `communicationChartNavigation.ts` 计算视窗并派发 ECharts `dataZoom`。横轴使用 `dataZoomIndex: 0`，纵轴使用 `dataZoomIndex: 1`，现有 WebGL 渲染监听器随缩放事件刷新画面，无新增后端接口。

- W/S 按 1.2 倍缩放当前可见时间范围；A/D 按可见范围的 10% 平移，并限制在完整范围内。
- Shift+W/S 缩放 Rank 轴，窗口按整数行计算，最少保留一行；上下方向键按可见行数的约 10% 平移，至少一行，并适配纵轴反向显示。
- 鼠标锚点从绘图区矩形计算；鼠标离开绘图区时使用视窗中心。重置通过一个批量事件恢复两轴到 0%–100%。
- 仅在图表自身获得焦点时处理快捷键，忽略输入法组合输入、子控件事件、Ctrl/Meta/Alt 组合键、加载状态和打开的右键菜单；Esc 释放焦点。
- `datazoom` 监听器同步重置图标状态，随图表销毁解除注册。相关测试位于 `CommunicationTimeAnalysisChart.test.tsx` 和 `communicationChartNavigation.test.ts`。
