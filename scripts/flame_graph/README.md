# flame_graph

# 1. 简介

flame_graph 工具用于从 `Ascend PyTorch Profiler` 导出的 `ascend_pytorch_profiler_{Rank_ID}.db` 数据库中解析 Host CPU 侧 PyTorch API 调用区间，并基于同一线程内的开始/结束时间关系还原函数调用栈，生成可离线查看的交互式 HTML 火焰图。

生成的 HTML 文件可直接使用浏览器打开，支持按线程查看、搜索函数名、鼠标悬浮查看耗时详情、点击节点进行缩放分析，适用于定位 Host CPU 侧函数调用栈中的耗时热点。

# 2. 使用说明

## 2.1 Ascend PyTorch Profiler 采集设置

生成火焰图依赖 `PYTORCH_API` 和 `STRING_IDS` 表，因此 Profiler 采集时需要包含 Host CPU 侧活动数据，即 `torch_npu.profiler.ProfilerActivity.CPU`。

示例如下：

```python
torch_npu.profiler.profile(
    activities=[
        torch_npu.profiler.ProfilerActivity.CPU,
        torch_npu.profiler.ProfilerActivity.NPU,
    ]
)
```

## 2.2 生成火焰图 HTML

脚本位于 MindStudio Insight 安装目录的 `resources/profiler/scripts/flame_graph` 目录下，脚本执行命令如下：

```shell
python3 flamegraph.py [-h] [-o OUTPUT] db_path
```

参数说明如下：

- db_path（必需）
  - 类型：文件路径
  - 说明：指定要解析的 `ascend_pytorch_profiler_{Rank_ID}.db` 文件路径。
  - 示例：`/path/to/ascend_pytorch_profiler_0.db`

- -o, --output（可选）
  - 类型：目录路径
  - 说明：指定生成结果的输出目录。若目录不存在，脚本会尝试自动创建，并在该目录下生成 `flamegraph.html`。
  - 缺省值：当前执行目录。
  - 示例：`--output /path/to/output_dir`

示例：

```shell
python3 flamegraph.py /path/to/ascend_pytorch_profiler_0.db --output /path/to/output_dir
```

生成完成后，使用浏览器打开：

```shell
/path/to/output_dir/flamegraph.html
```

# 3. 页面功能

- 线程切换：通过 `Thread` 下拉框查看全部线程或指定线程的火焰图。
- 函数搜索：在 `Search` 输入框中输入关键字，高亮匹配函数并显示匹配数量。
- 节点详情：鼠标悬浮在火焰图节点上，可查看函数名、分类、调用次数、总耗时、自身耗时和占比。
- 缩放分析：点击节点可放大查看该节点子树；点击当前根节点或 `Reset` 可恢复视图。
- 清空搜索：点击 `Clear` 或按 `Esc` 可清空搜索条件；无搜索条件时按 `Esc` 会恢复默认缩放视图。
- 分类着色：根据函数名称关键字将节点分为 Python、Framework、CANN/NPU 和 Other，并使用不同颜色展示。

# 4. 数据说明

脚本主要读取以下表：

- `PYTORCH_API`：Host CPU 侧 PyTorch API 调用数据，包括开始时间、结束时间、线程 ID、API 名称 ID 和数据类型。
- `STRING_IDS`：字符串映射表，用于将 API 名称 ID 映射为可读字符串。

脚本会解析 `PYTORCH_API` 表中具备 `startNs`、`endNs` 和 `globalTid` 的 Host CPU 侧 API 调用区间。脚本会按线程分别处理 API 调用记录，根据 `startNs` / `endNs` 的时间区间包含关系还原调用栈层级，再将各线程结果合并到同一个火焰图中。脚本会过滤 `ProfilerStep#` 标记事件，并跳过耗时小于等于 0 的异常记录。异常记录数量会通过日志输出。

调用栈节点分类规则如下：

- `python`：函数名包含 `.py` 或 `python` 等 Python 侧特征。
- `python_framework`：函数名包含 `torch`、`torch_npu`、`aten::`、`c10::`、`aten_` 等框架侧特征。
- `cann`：函数名包含 `cann`、`ascendcl`、`aclnn`、`aclrt`、`aclmdl`、`aclprof`、`hccl` 等 CANN/NPU 侧特征。
- `unknown`：无法通过上述关键字识别的节点。

# 5. 使用约束

- 输入文件必须是合法的 SQLite `.db` 文件，并且包含 `PYTORCH_API` 和 `STRING_IDS` 表。
- 输入 DB 文件大小不能超过 10GB。
- 输出路径必须是已存在且当前用户具备写权限的目录。
- 脚本会在生成 JSON 数据时限制最大 API 调用栈深度为 1000 层，超过限制的子节点不会继续展开，并会通过日志打印告警。
- 生成的 HTML 为自包含文件，数据量较大时文件体积和浏览器加载耗时会随节点数量增加。

# 6. 常见问题

## 6.1 提示 DB 缺少必要表

请确认输入文件为 `Ascend PyTorch Profiler` 导出的 `ascend_pytorch_profiler_{Rank_ID}.db`，并检查其中是否包含 `PYTORCH_API` 和 `STRING_IDS` 表。

## 6.2 生成结果为空

可能原因包括：

- Profiler 采集时未包含 CPU 侧活动数据。
- `PYTORCH_API` 表中不存在 op、queue 或 python_trace 类型数据。
- 数据中只有 `ProfilerStep#` 标记事件或异常时间区间。

## 6.3 提示 DB 文件超过 10GB

该工具与项目内 `.db` 文件检查策略保持一致，输入 DB 文件大小限制为 10GB。请裁剪采集数据或选择更小的 profiler DB 后重新生成。

## 6.4 浏览器打开 HTML 较慢

火焰图数据会直接内联到 HTML 中。若 DB 中 API 调用数量较大，HTML 文件可能较大，浏览器解析和绘制需要一定时间。
