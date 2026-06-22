# trace\_analyze.py — ftrace 数据统计分析与 Excel 报告生成

## 工具简介

`trace_analyze.py` 是 ftrace 数据分析工具，用于对 `trace_convert.py` 生成的 SQLite DB 数据进行**统计汇聚分析**，并自动生成包含图表的 Excel 报告。本工具侧重于**量化统计**，帮助开发者从数据维度快速定位热点进程、异常调度与 IRQ 打断问题。

## 前置依赖

| 依赖           | 说明          |
| ------------ | ----------- |
| Python 3.10+ | 运行环境        |
| openpyxl     | Excel 文件生成库 |

安装 openpyxl：

```bash
pip install openpyxl
```

## 使用说明

### 基本用法

```bash
python trace_analyze.py --input <db_file> [--output <excel_file>]
```

| 参数               | 说明                                   | 默认值                      |
| ---------------- | ------------------------------------ | ------------------------ |
| `--input`, `-i`  | 输入的 db 文件路径（由 `trace_convert.py` 生成） | `ftrace_data.db`（脚本同目录下） |
| `--output`, `-o` | 输出的 Excel 报告路径                       | `<input>_report.xlsx`    |

### 示例命令

**1. 使用默认输入输出**

```bash
python trace_analyze.py
```

读取当前目录下的 `ftrace_data.db`，生成 `ftrace_data_report.xlsx`。

**2. 指定输入文件**

```bash
python trace_analyze.py -i /path/to/ftrace_data.db
```

**3. 指定输入和输出路径**

```bash
python trace_analyze.py -i /path/to/ftrace_data.db -o /path/to/my_analysis.xlsx
```

## 典型工作流

```text
trace_record.py（采集）
      ↓
trace_convert.py（转换为 DB）
      ↓
trace_analyze.py（统计分析 → Excel 报告）
```

1. 使用 `trace_record.py` 采集 ftrace 数据
2. 使用 `trace_convert.py` 将原始数据转换为 SQLite DB 格式（默认输出 `ftrace_data.db`）
3. 使用 `trace_analyze.py` 对 DB 进行统计分析，生成 Excel 报告

## 分析指标说明

### 进程运行时间

| 指标           | 说明                  |
| ------------ | ------------------- |
| **Running**  | 进程实际在 CPU 上运行的时间    |
| **Sleeping** | 进程处于睡眠（阻塞等待）状态的时间   |
| **Runnable** | 进程已就绪但等待调度的时间（调度延迟） |

### 上下文切换（Context Switch）

| 指标                         | 说明                                  |
| -------------------------- | ----------------------------------- |
| **cs\_count**              | 上下文切换总次数                            |
| **cs\_involuntary\_count** | 非自愿切换次数（进程仍在 Ready 状态被强制换下，即**抢占**） |

> 非自愿切换占比高，说明进程经常被抢占，可考虑绑核优化。

### IRQ / SoftIRQ 打断

| 指标          | 说明            |
| ----------- | ------------- |
| **IRQ**     | 硬中断打断进程的次数与耗时 |
| **SoftIRQ** | 软中断打断进程的次数与耗时 |

> IRQ 打断频繁会导致进程运行碎片化，影响性能稳定性。

## 输出格式

Excel 报告包含 **4 个工作表**和 **3 个柱状图**：

### 工作表 1：task\_summary\_by\_comm

按\*\*进程名（comm）\*\*汇总所有 PID 和 CPU 的统计数据。

| 列名                     | 说明               |
| ---------------------- | ---------------- |
| comm                   | 进程名              |
| running\_us            | Running 总时间（微秒）  |
| sleeping\_us           | Sleeping 总时间（微秒） |
| runnable\_us           | Runnable 总时间（微秒） |
| cs\_count              | 上下文切换总次数         |
| cs\_involuntary\_count | 非自愿切换次数          |

**图表**：Top10 Running Time 柱状图（按进程名排序）

### 工作表 2：task\_summary\_by\_pid

按**进程名:PID**汇总所有 CPU 的统计数据。适用于同一进程有多个实例的场景。

| 列名                     | 说明               |
| ---------------------- | ---------------- |
| comm                   | 进程名              |
| pid                    | 进程 ID            |
| running\_us            | Running 总时间（微秒）  |
| sleeping\_us           | Sleeping 总时间（微秒） |
| runnable\_us           | Runnable 总时间（微秒） |
| cs\_count              | 上下文切换总次数         |
| cs\_involuntary\_count | 非自愿切换次数          |

### 工作表 3：task\_summary

每行一个 **(comm, pid, cpu\_id)** 维度的详细数据，不合并。适用于分析进程在特定 CPU 上的行为。

| 列名                     | 说明              |
| ---------------------- | --------------- |
| comm                   | 进程名             |
| pid                    | 进程 ID           |
| cpu\_id                | CPU 编号          |
| running\_us            | Running 时间（微秒）  |
| sleeping\_us           | Sleeping 时间（微秒） |
| runnable\_us           | Runnable 时间（微秒） |
| cs\_count              | 上下文切换总次数        |
| cs\_involuntary\_count | 非自愿切换次数         |

### 工作表 4：proc\_irq\_detail

IRQ / SoftIRQ 打断明细数据，从进程视角展示被中断的情况。

| 列名        | 说明                      |
| --------- | ----------------------- |
| comm      | 被中断的进程名                 |
| pid       | 被中断的进程 ID               |
| cpu\_id   | 发生中断的 CPU 编号            |
| irq\_type | 中断类型（`irq` 或 `softirq`） |
| irq\_name | 中断名称 / action           |
| count     | 打断次数                    |
| time\_us  | 打断总耗时（微秒）               |

**图表**：

- Top10 IRQ Time 柱状图（按耗时排序）
- Top10 IRQ Count 柱状图（按次数排序）

> IRQ 图表的 X 轴标签格式为 `comm:pid@cpu_id`，例如 `python3:12345@0`。

## 注意事项

- **输入依赖**：输入文件必须由 `trace_convert.py` 生成，且包含 `slice` 和 `thread` 表
- **旧版数据兼容**：如果使用旧版 `trace_convert.py`（缺少 cpu 字段），CPU 列将为空，工具会打印告警日志
- **空闲进程过滤**：`<idle>` 进程的 IRQ 统计会被自动跳过
- **内核进程过滤**：`swapper`、`kworker`、`migration` 等内核进程的上下文切换事件会被自动跳过
- **数据量建议**：推荐 CPU 采集核心数不超过 64，采集时长 30s 左右，否则统计分析耗时可能较长
