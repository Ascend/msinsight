# 使用MindStudio Insight加载Linux Kernel数据以联合分析Host Bound问题

## 问题背景

在大模型中，CPU主要负责任务的下发，NPU负责计算任务的执行。无论训练还是推理领域，Host Bound都是现网高发问题。分析Host Bound问题通常需要采集Linux Kernel ftrace数据，分析CPU上的进程调度情况。

目前缺乏一种工具，能够统合profiling数据和Linux Kernel ftrace数据，做到联合分析。MindStudio Insight在本仓库中，提出一些工具脚本，帮助开发者实现两种数据的联合分析，提高Host Bound问题定位的效率。

## Host Bound问题定位思路

1. 尝试通用的调度优化手段，包括绑核、流水优化、内存分配库替换三板斧。PyTorch框架调度优化可参考：[调度优化-Ascend Extension for PyTorch-昇腾社区](https://www.hiascend.com/document/detail/zh/Pytorch/720/ptmoddevg/trainingmigrguide/performance_tuning_0059.html)
2. 若通用优化手段效果不及预期，可采集数据，进一步深入分析。建议同时采集ftrace和profiling数据。
3. 将ftrace数据转换为MindStudio Insight可识别的数据格式。
4. 同时导入ftrace数据与profiling数据，分析进程调度情况。

## 1. 工具能力概览

`ftrace_tools` 用于采集、转换和分析 Linux Kernel ftrace 数据，辅助定位调度延迟、上下文切换频繁、IRQ/SoftIRQ 打断等问题。

| 脚本 | 作用 | 主要输出 |
| --- | --- | --- |
| `trace_record.py` | 采集 sched、irq/softirq 等 ftrace 事件 | `trace.dat` 或 `trace.txt`、`pid_mapping.json` |
| `trace_convert.py` | 将原始 ftrace 转换为 MindStudio Insight 可导入格式 | `ftrace_data.db` 或 `.json` |
| `trace_analyze.py` | 对 `trace_convert.py` 生成的 DB 做统计分析并输出 Excel | `<input>_report.xlsx` |

典型流程：

```text
trace_record.py（采集）
      ↓
trace_convert.py（转换为 DB/JSON，可与 Profiling 时间轴对齐）
      ↓
MindStudio Insight（可视化联合分析）或 trace_analyze.py（统计分析）
```

## 2. 前置依赖

- Python 3.10+
- 可选：`trace-cmd`。未安装或不可用时，`trace_record.py --backend=auto` 会回退到 tracefs/debugfs 后端。
- 使用 `trace_analyze.py` 生成 Excel 时需要 `openpyxl`。

安装示例：

```bash
# Ubuntu
sudo apt-get install trace-cmd

# CentOS
sudo yum install trace-cmd

# trace_analyze.py 依赖
pip install openpyxl
```

## 3. 采集 ftrace 数据

### 3.1 命令行采集

```bash
sudo python trace_record.py --record_time=30 --cpu=0-15 --output=trace.dat
```

常用参数：

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `--cpu` | CPU mask，支持 `0,1,4`、`0-3,8`、`0-15` 等写法 | 采集全部 CPU |
| `--output` | 输出文件路径。未指定时由后端决定默认文件名 | trace-cmd 后端默认 `trace.dat`；debugfs 后端默认 `trace.txt` |
| `--record_time` | 采集时长，单位秒；`<=0` 表示持续采集，需 `Ctrl+C` 停止 | `30` |
| `--bf_size` | ftrace ring buffer 大小，单位 KB；数据量大时建议增大 | `40960` |
| `--backend` | `auto`、`trace-cmd`、`debugfs` | `auto` |
| `--sched` | 是否采集调度事件 | `1` |
| `--irq` | 是否采集中断/软中断事件 | `1` |
| `--NSpid` | 容器场景下 dump 容器 PID 与宿主机 PID 映射 | 关闭 |

建议 CPU 采集核心数不超过 64，采集时长 30s 左右。采集范围过大、事件量过高或磁盘较慢时，停止采集和后续转换都可能耗时较长。

### 3.2 后端差异

| 后端 | 典型输入/输出 | 说明 |
| --- | --- | --- |
| `trace-cmd` | 输出 `trace.dat` | 优先推荐；依赖系统安装 `trace-cmd` |
| `debugfs` / `tracefs` | 输出 `trace.txt` | 不依赖 `trace-cmd`；停止采集时需要从 tracing 文件系统拷贝文本快照，数据量大时耗时更明显 |

强制使用 debugfs 示例：

```bash
sudo python trace_record.py --backend=debugfs --record_time=30 --cpu=0-15 --output=trace.txt --NSpid
```

如果采集结束日志出现 lost/overwritten 事件告警，说明 ring buffer 中发生过覆盖或丢失，建议优先增大 `--bf_size`，也可缩短采集时长、减少事件类型或缩小 CPU 范围后重新采集。

## 4. 转换 ftrace 数据

`trace_convert.py` 将 `trace.dat` 或 `trace.txt` 转换为 MindStudio Insight 可导入的 SQLite DB 或 Chrome Trace JSON，并可通过 Profiling 目录进行时间轴对齐。

```bash
python trace_convert.py \
  --input trace.dat \
  --output ftrace_data.db \
  --format db \
  --profiling_data /path/to/profiling/xxxx_ascend_pt \
  --pid_mapping pid_mapping.json
```

参数说明：

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `--input` | 输入的原始 ftrace 文件。trace-cmd 后端通常为 `.dat`，debugfs 后端通常为 `.txt` | `trace.dat` |
| `--output` | 输出文件路径。后缀必须与 `--format` 匹配 | `ftrace_data.db` |
| `--format` | 输出格式，支持 `db` 或 `json` | `db` |
| `--profiling_data` | Profiling 数据目录，用于 ftrace 与 Profiling 时间轴对齐 | 不启用 |
| `--pid_mapping` | 容器 PID 映射文件，用于把宿主机 PID 转为容器内 PID | 不启用 |

### 4.1 输出格式与后缀要求

`--format` 与 `--output` 文件后缀必须一致；不一致时脚本会直接报错退出，避免 JSON 内容被保存成 `.db` 或 DB 内容被保存成 `.json`。

| `--format` | `--output` 要求 | 示例 |
| --- | --- | --- |
| `db` | 必须以 `.db` 结尾 | `--format=db --output=ftrace_data.db` |
| `json` | 必须以 `.json` 结尾 | `--format=json --output=ftrace_data.json` |

常用命令：

```bash
# trace-cmd 输出：trace.dat -> SQLite DB（默认）
python trace_convert.py --input=trace.dat --output=ftrace_data.db --format=db

# debugfs 输出：trace.txt -> SQLite DB。注意需要显式指定 --input
python trace_convert.py --input=trace.txt --output=ftrace_data.db --format=db

# 输出 Chrome Trace JSON
python trace_convert.py --input=trace.dat --output=ftrace_data.json --format=json
```

> debugfs 模式输出通常为 `trace.txt`。若同目录下仍存在默认文件名 `trace.dat`，转换时未显式指定 `--input=trace.txt`，脚本会读取默认 `trace.dat`，导致转换的不是本次 debugfs 采集结果。

### 4.2 Profiling 时间轴对齐

传入 `--profiling_data` 后，`trace_convert.py` 会读取 Profiling 目录中的 `start_info` / `end_info`，将 ftrace 时间转换到 Profiling 时间轴，便于在 MindStudio Insight 中联合分析。

```bash
python trace_convert.py \
  --input=trace.dat \
  --output=ftrace_data.db \
  --format=db \
  --profiling_data=/path/to/profiling/xxxx_ascend_pt
```

多卡场景下，`--profiling_data` 可指定为包含多卡的上级目录，或任意单卡数据目录。

### 4.3 容器 PID 映射

容器未使用 `--pid=host` 启动时，Profiling 看到的是容器内 PID，而 ftrace 采集的是宿主机 PID。此时需要：

1. 采集时打开 `--NSpid`，生成 `pid_mapping.json`；
2. 转换时传入 `--pid_mapping=pid_mapping.json`。

```bash
sudo python trace_record.py --record_time=30 --cpu=0-15 --NSpid
python trace_convert.py --input=trace.dat --output=ftrace_data.db --format=db --pid_mapping=pid_mapping.json
```

## 5. 导入 MindStudio Insight

推荐优先使用 `trace_convert.py` 默认生成的 SQLite DB：

```bash
python trace_convert.py --input=trace.dat --output=ftrace_data.db --format=db --profiling_data=/path/to/profiling
```

当前版本已支持 ftrace DB 与 Profiling Text/DB 数据在同一工程中混合导入。一般流程为：

1. 在 MindStudio Insight 中先导入 Profiling 数据；
2. 在同一工程中继续导入 `ftrace_data.db`；
3. 结合 CPU Scheduling、Process Scheduling 与 Profiling 视图分析 Host Bound 问题。

JSON 输出主要用于需要 Chrome Trace JSON 格式、或外部工具只支持 JSON 的场景：

```bash
python trace_convert.py --input=trace.dat --output=ftrace_data.json --format=json
```

## 6. 统计分析：trace_analyze.py

如果需要对 ftrace DB 做离线汇总统计并生成 Excel 报告，可使用：

```bash
python trace_analyze.py --input ftrace_data.db --output ftrace_report.xlsx
```

更多输出表、指标和图表说明见 [trace_analyze_README.md](trace_analyze_README.md)。

## 7. 完整样例

- vllm-ascend 容器场景下，宿主机采集 ftrace、容器内采集 Profiling、再导入 Insight 联合分析的完整样例见 [docs/vllm_ascend_example.md](docs/vllm_ascend_example.md)。

## 8. 常见问题

### 8.1 转换后 `.db` 文件无法用 SQLite 打开

优先检查转换命令中的 `--format` 和 `--output` 是否匹配：

- `--format=db` 必须输出 `.db`；
- `--format=json` 必须输出 `.json`。

新版本 `trace_convert.py` 已强制校验该规则。如果使用旧版本脚本，可能出现 JSON 内容保存到 `.db` 文件的情况，导致 SQLite 工具无法打开。

### 8.2 使用 debugfs 采集后，为什么转换时必须指定 `--input=trace.txt`？

debugfs 后端默认生成 `trace.txt`。但 `trace_convert.py` 为兼容 trace-cmd 流程，默认输入仍是 `trace.dat`。

因此，如果目录中残留了旧的 `trace.dat`，执行以下命令会转换旧的 `trace.dat`，而不是本次 debugfs 采集得到的 `trace.txt`：

```bash
python trace_convert.py --output=ftrace_data.db --format=db
```

使用 debugfs 采集后，请显式指定：

```bash
python trace_convert.py --input=trace.txt --output=ftrace_data.db --format=db
```

### 8.3 ftrace 数据部分 CPU 核存在大面积空白

![](./assets/partial_cpu_core_blank.png)

可能原因：

1. ring buffer 发生覆盖或事件丢失。若采集结束日志出现 lost/overwritten 告警，建议增大 `--bf_size`、缩短采集时长、减少事件或缩小 CPU 范围；
2. 该 CPU 在目标时间段没有采集到目标事件，或未被业务实际使用。

### 8.4 trace-cmd record 停止超时

采集数据量较大时，`trace-cmd record` 清理时间可能较长。可缩小采集范围、缩短采集时长，或根据实际环境调整脚本中的停止等待参数。

### 8.5 能否在容器内直接使用 ftrace 采集能力？

可以，但容器启动时需要满足额外权限和挂载要求。ftrace 依赖宿主机内核 tracing 文件系统，普通非特权容器通常无法直接访问或配置这些内核接口。

建议容器启动时满足以下条件：

1. **使用特权容器**：例如 Docker 启动时添加 `--privileged`，确保容器具备配置内核 tracing 子系统所需的权限；
2. **挂载宿主机 tracing/debugfs 相关目录**：至少需要让容器内能访问 ftrace 控制文件。常见挂载目录包括：
   - `/sys/kernel/tracing`
   - `/sys/kernel/debug`
3. **确认容器内具备读写 ftrace 控制文件的权限**：采集脚本需要写入 `tracing_on`、`set_event`、`events/*/enable`、`trace` 等文件；
4. **注意 PID 命名空间差异**：
   - 如果容器未使用 `--pid=host`，容器内 PID/TID 与宿主机内核视角下的 PID/TID 不一致；
   - ftrace 采集到的是宿主机内核视角下的进程号和线程号；
   - 如果不做 PID 映射，后续与容器内 Profiling 数据联合分析时，可能无法准确对应到容器内的业务进程和线程；
   - 建议采集时开启 `--NSpid` 生成 `pid_mapping.json`，并在转换时通过 `--pid_mapping=pid_mapping.json` 完成 PID 映射。

如果无法满足以上条件，建议在宿主机侧采集 ftrace，并通过 `--NSpid` / `--pid_mapping` 处理容器 PID 映射关系。
