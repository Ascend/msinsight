# MindStudio Insight 支持 Device 内存分析特性设计说明书

**Copyright © 2026 Ascend Community**

## 改版记录

| 日期 | 修订版本 | 修订描述 | 作者 | 审核 |
| --- | --- | --- | --- | --- |
| 2026-01-15 | v1.0 | 初稿 | 刘鹏程 | 廖宴 |
| 2026-06-11 | v1.1 | 清理模板占位，补充可验证的 Device 内存分析范围、数据来源、安全与验证说明 | - | - |

## 缩略语清单

| 缩略语 | 英文全名 | 中文解释 |
| --- | --- | --- |
| msInsight | MindStudio Insight | MindStudio Insight 性能分析工具 |
| Snapshot | PyTorch Memory Snapshot | PyTorch 内存快照数据 |
| msMemScope | MindStudio Memory Scope | Device 内存采集与分析工具 |
| OOM | Out Of Memory | 内存不足 |

# 1. 特性概述

在大模型训练或推理性能调优场景下，Device 侧内存优化是重要方向。开发者需要了解内存申请、释放、峰值、碎片、调用栈和内存池状态，才能定位 OOM、内存泄漏、低效显存占用等问题。MindStudio Insight 提供图形化内存分析能力，帮助开发者分析 Device 侧内存使用情况。

## 1.1 范围

本文覆盖 MindStudio Insight 中与 Device 侧内存分析相关的开发设计说明，重点包括：

1. PyTorch Snapshot 内存分析：展示内存块生命周期、内存池状态和内存事件详情。
2. MindStudio Memory Scope / msMemScope 内存分析：展示调用栈、内存块生命周期、内存详情拆解和内存详情表。
3. Host-Device 内存拷贝分析相关能力仅作为关联能力说明，具体设计以对应模块文档和代码为准。

本文不补充未在仓库源码、用户指南或发布说明中验证的内部数据库 schema、算法细节和性能指标。

## 1.2 特性需求列表

| 需求编号 | 需求名称 | 特性描述 | 可验证资料 |
| --- | --- | --- | --- |
| 1 | 支持 PyTorch Snapshot 内存分析 | 支持 snapshot 中内存生命周期展示、内存池状态分析和内存事件详情展示 | `docs/zh/user_guide/memory_tuning.md` |
| 2 | 支持 MindStudio Memory Scope 内存分析 | 支持 msMemScope 数据导入后的调用栈、内存块生命周期、内存分类拆解等功能 | `docs/zh/user_guide/memory_tuning.md` |

# 2. 需求场景分析

## 2.1 特性需求来源与价值概述

在强化学习、大模型训练和推理场景中，内存变化可能随阶段变化而波动，例如 `generate_sequence`、`actor_update` 等阶段可能出现不同的内存峰值或碎片特征。通过 Snapshot 或 msMemScope 数据，开发者可以定位 OOM、内存碎片、内存峰值和低效显存使用问题。

## 2.2 典型使用场景

### PyTorch Snapshot 场景

1. 在模型运行前调用 `torch_npu.npu.memory._record_memory_history()` 启用内存历史记录。
2. 运行需要分析的训练或推理代码。
3. 调用 `torch_npu.npu.memory._dump_snapshot("snapshot.pickle")` 导出 `pickle` 文件。
4. 在 MindStudio Insight 中导入内存快照文件，查看内存块生命周期、内存池状态和内存事件详情。

### msMemScope 场景

1. 使用 msMemScope 工具采集内存结果文件。
2. 导入 `memscope_dump_{timestamp}.db` 格式的结果文件。
3. 在 MindStudio Insight 中查看调用栈火焰图、内存块生命周期图、内存详情拆解图和内存详情表。

## 2.3 约束与限制

### 2.3.1 硬件限制

支持昇腾 NPU 相关内存分析场景。具体硬件支持范围以 MindStudio Insight 发布说明和对应数据采集工具说明为准。

### 2.3.2 系统限制

现有文档中记录的建议环境如下：

1. Windows 10+。
2. Linux 建议 `glibc > 2.30`。
3. macOS 13.0+。
4. 建议运行 MindStudio Insight 的设备内存超过 16GB。
5. 建议内存数据文件不超过 1GB。

如实际支持矩阵发生变化，以安装指南、发布说明和构建脚本为准。

### 2.3.3 安全与敏感信息

MindStudio Insight 是本地分析工具，但输入文件可能包含用户路径、函数名、调用栈、符号名、模型执行信息等内容。开发和文档示例应遵循以下原则：

- 示例路径使用脱敏路径，如 `/home/xxx/demo.py`。
- 不在文档中放置真实用户名、局点信息、密钥、口令或公网地址。
- 对用户导入的文件路径、文件类型和文件大小进行合法性校验。
- 若日志中需要输出异常信息，应避免打印敏感路径和完整调用栈，必要时进行脱敏。

# 3. 总体方案

## 3.1 数据来源

| 数据源 | 数据格式 | 主要展示内容 | 说明 |
| --- | --- | --- | --- |
| PyTorch Snapshot | `pickle` | 内存块生命周期、内存池状态、内存事件详情 | 由 `torch_npu.npu.memory._dump_snapshot()` 导出 |
| msMemScope | `memscope_dump_{timestamp}.db` | 调用栈火焰图、内存块生命周期图、内存详情拆解图、内存详情表 | 由 msMemScope 工具采集生成 |

## 3.2 处理流程

1. 用户采集 Snapshot 或 msMemScope 数据。
2. 用户在 MindStudio Insight 中导入数据文件。
3. 后端解析数据并建立查询所需的数据结构或数据库连接。
4. 前端按页面交互发起查询请求。
5. 后端返回图表、表格、详情或筛选结果。
6. 前端展示内存生命周期、调用栈、内存池状态和明细数据。

## 3.3 与其他文档的关系

- Snapshot 详细设计请参见 `support_snapshot_analysis.md`。
- 用户侧采集和使用流程请参见 `docs/zh/user_guide/memory_tuning.md`。
- 普通 Memory 模块开发说明请参见 `Memory.md`。

# 4. Use Case 一：PyTorch Snapshot 内存分析

## 4.1 设计目标

支持导入 PyTorch Snapshot 数据，展示模型运行过程中 PyTorch 管理的内存池使用情况，辅助定位内存碎片、内存峰值和潜在泄漏问题。

## 4.2 用户入口

用户通过 PyTorch NPU 内存快照 API 采集并导出 `pickle` 文件。用户指南中的采集示例如下：

```python
torch_npu.npu.memory._record_memory_history(stacks='python')
# 运行模型代码
torch_npu.npu.memory._dump_snapshot("model_memory_snapshot.pickle")
```

## 4.3 页面能力

Snapshot 页面可展示：

- 内存块生命周期图。
- 内存池状态图。
- 内存块视图。
- 内存事件视图。
- 选中详情。
- 自动筛选区间未释放内存块。

字段含义和用户操作以 `docs/zh/user_guide/memory_tuning.md` 中“PyTorch Snapshot 数据内存详情（内存快照）”章节为准。

## 4.4 待确认事项

以下内容需从源码或设计评审中进一步确认后再补充：

- Snapshot 解析器的具体代码路径。
- 后端数据库表结构或中间数据结构。
- 查询接口的完整请求/响应字段。
- 大文件性能指标和异常返回格式。

# 5. Use Case 二：msMemScope 内存分析

## 5.1 设计目标

支持导入 msMemScope 采集的 DB 结果文件，以图形化方式展示 Device 内存申请释放生命周期、调用栈和内存拆解信息。

## 5.2 数据准备

支持导入 `memscope_dump_{timestamp}.db` 格式文件。用户指南中记录的展示能力包括：

- 内存块生命周期图。
- 内存详情拆解图。
- Python 调用栈图。
- 内存块视图。
- 内存事件视图。
- 低效显存筛选。

## 5.3 页面能力

msMemScope 数据导入后，页面可展示：

1. 调用栈火焰图：按线程和函数查看 Python 调用栈。
2. 内存块生命周期图：查看内存申请、释放、访问事件。
3. 内存详情拆解图：按类型展示 CANN、PTA 等内存层级。
4. 内存详情表：在内存块视图和内存事件视图中查看明细。

字段含义和用户操作以 `docs/zh/user_guide/memory_tuning.md` 中“MemScope 数据内存详情”章节为准。

# 6. DFX 设计

## 6.1 性能设计

- 建议对大文件导入、长时间范围查询、缩放和平移操作进行性能验证。
- 建议对 1GB 以内内存数据文件进行导入与查询验证。
- 不在本文中承诺具体响应时间；具体指标需以性能测试结果为准。

## 6.2 异常处理设计

建议覆盖以下异常场景：

- 文件格式不匹配。
- 文件损坏或字段缺失。
- 文件过大导致解析失败或耗时过长。
- 查询时间范围为空。
- 调用栈或访问事件未采集。
- Snapshot 中某些事件缺少申请或释放记录。

## 6.3 安全设计

- 不新增对外监听端口。
- 不新增认证方式。
- 文件导入路径和文件类型应做合法性校验。
- 示例和日志应避免暴露真实用户路径、口令、密钥或局点信息。
- 若新引入第三方组件，应按项目三方件引入流程完成 license 与漏洞检查。

## 6.4 可测试性设计

建议测试覆盖：

- Snapshot 正常导入与详情查询。
- msMemScope DB 正常导入与详情查询。
- 空数据、缺失调用栈、缺失访问事件。
- 大文件导入。
- 错误文件格式。
- 筛选、缩放、拖拽、搜索、复制等页面交互。

# 7. 验证方法

## 7.1 静态验证

- 检查文档中相对链接是否存在。
- 检查示例路径是否已脱敏。
- 检查是否仍存在模板占位、空图片或未闭合 Markdown 标记。

## 7.2 功能验证

- 使用 PyTorch Snapshot `pickle` 文件导入，验证生命周期图、内存池状态图、详情表。
- 使用 msMemScope DB 文件导入，验证调用栈火焰图、内存块生命周期图、拆解图和详情表。
- 验证异常文件、空数据和缺失字段场景的提示。

# 8. 待确认事项

- Snapshot 和 msMemScope 的后端解析器路径、数据库结构和完整接口定义。
- Host-Device 内存拷贝分析的开发设计文档归属。
- 性能指标、错误码和告警格式。
