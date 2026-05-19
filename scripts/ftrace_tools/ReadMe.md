# 使用MindStudio Insight加载Linux Kernel数据以联合分析Host Bound问题

## 问题背景

在大模型中，CPU主要负责任务的下发，NPU负责计算任务的执行。无论训练还是推理领域，Host Bound都是现网高发问题。分析Host Bound问题通常需要采集Linux Kernel ftrace数据，分析CPU上的进程调度情况。

目前缺乏一种工具，能够统合profiling数据和Linux Kernel ftrace数据，做到联合分析。MindStudio Insight在本仓库中，提出一些工具脚本，帮助开发者实现两种数据的联合分析，提高Host Bound问题定位的效率。

## 特性

+ 支持**命令行**和**API接口**两种方式的ftrace数据采集
+ 支持将ftrace格式数据转换为SQLite DB格式（默认）或Chrome Trace Json格式，**联合Profiling数据导入**MindStudio Insight，进行可视化展示
+ 支持**容器内**运行模型，宿主机采集Linux Kernel ftrace数据，并进行无缝PID映射

## Host Bound问题定位思路

1. 尝试通用的调度优化手段，包括绑核、流水优化、内存分配库替换三板斧。PyTorch框架调度优化可参考：[调度优化-Ascend Extension for PyTorch-昇腾社区](https://www.hiascend.com/document/detail/zh/Pytorch/720/ptmoddevg/trainingmigrguide/performance_tuning_0059.html)
2. 若通用优化手段效果不及预期，可采集数据，进一步深入分析。建议同时采集ftrace和profiling数据。
3. 将ftrace数据转换为MindStudio Insight可识别的数据格式。
4. 同时导入ftrace数据与profiling数据，分析进程调度情况。

## 模型Profiling采集

参考昇腾社区有关profiling采集相关的文档：[简介-CANN商用版8.3.RC1-昇腾社区](https://www.hiascend.com/document/detail/zh/canncommercial/83RC1/devaids/Profiling/atlasprofiling_16_0001.html)

> 请注意Profiling采集配置。当前仅支持ftrace与Text类型的Profiling联合导入，暂不支持与DB类型的Profiling数据联合导入。

## Linux Kernel ftrace数据采集

### 1. Linux Kernel ftrace数据简介

Linux内核内置了多种跟踪（trace）工具，其中ftrace作为从2.6.27版本开始引入主流内核的跟踪框架，可用于监控和调试内核中发生的各类事件，帮助开发人员深入分析系统运行时的内部行为。ftrace支持多种跟踪器，例如函数调用跟踪、上下文切换跟踪、中断延迟分析等，能够有效辅助定位内核态性能问题与调度异常。
在本仓库中，提供了与CPU进程调度相关的事件（sched）、中断 / 软中断相关事件（irq）的采集能力。

trace-cmd是一个命令行工具，它封装了trace采集的过程，提供了更加简易的命令接口。

### 2. 数据采集前置准备

+ 安装trace-cmd命令

  Ubuntu安装命令：`sudo apt-get install trace-cmd`

  CentOs安装命令：`sudo yum install trace-cmd`

+ 获取仓库中提供的采集、转换脚本`trace_record.py`，`trace_convert.py`，推荐profiling和ftrace数据同步采集。

### 3. ftrace数据采集

支持**命令行**和**API接口**两种方式的ftrace数据采集。

#### 方式一：命令行采集

这种方式不需要修改现有代码，将`trace_record.py`脚本作为整体使用，易上手，但自定义程度相对有限。

**参数说明**

| 参数 | 说明 | 示例 | 默认值 |
|-----|-----|-----|-----|
| `--cpu` | cpu_mask 列表，指定采集的 CPU 核心。支持单个数字、逗号分隔及连字符范围。| `--cpu=0,1,4 (指定核)`<br>`--cpu=0-3,8 (混合写法)`<br>`--cpu=0-15 (范围写法)` | None，采集 **全部** CPU 核心 |
| `--output` | 输出文件路径与文件名 | `--output=my_trace_data.dat` | `trace.dat` |
| `--record_time` | 采集持续时间（单位：秒）<br>• 正值：采集指定秒数后自动停止<br>• ≤0：持续采集，需 `Ctrl+C` 手动终止，长时间采集请注意磁盘空间占用 | `--record_time=30` | 30 |
| `--bf_size` | trace-cmd 使用的 **ring buffer 大小（单位：KB）**，用于在内核侧缓存 ftrace 事件。<br>当采集数据量较大时，可适当增大该值，避免环形缓冲区数据覆盖导致 **trace event 丢失** | `--bf_size=4096` | `2820` |
| `--sched` | 是否采集 **CPU 调度相关事件**，包括任务切换、唤醒、新任务启动等（如 `sched_switch`、`sched_wakeup`、`sched_wakeup_new`）。<br>主要用于分析 **调度延迟、任务切换频率、线程唤醒关系**。<br>• `1`：开启<br>• `0`：关闭 | `--sched=1`<br>`--sched=0` | `1`（开启） |
| `--irq` | 是否采集 **中断 / 软中断相关事件**，包括硬中断与 softirq 的进入、退出及触发行为。<br>主要用于分析 **中断负载、软中断抖动、CPU 被中断打断的情况**。<br>• `1`：开启<br>• `0`：关闭 | `--irq=1`<br>`--irq=0` | `1`（开启） |
| `--NSpid` | 容器场景下开启该开关，用于获取容器与宿主机 PID 映射关系，详见[容器内外PID命名空间隔离场景下采集](#5-容器内外pid命名空间隔离场景下采集) | `--NSpid` | 关闭 |

**使用示例:**

场景：采集CPU核心`0-4`的30秒训练数据

**1. 启动数据采集（需root权限）：**

```bash
sudo python trace_record.py --record_time=30 --cpu=0,1,2,3,4
```

**注意**：此处的`--cpu=0,1,2,3,4`仅为示例，实际使用时，建议根据绑核策略，选取一定量的CPU核心进行采集（**推荐CPU采集核心数不超过64，采集时长30s左右，否则采集数据量可能过大，解析落盘耗时较长，需耐心等待**）。

**2. 执行训练任务（在新终端中）：**

```bash
python train.py
```

**3. 采集结果：**

脚本运行30秒后自动停止，默认生成 `trace.dat` 文件（或通过 --output 参数指定文件名）。

#### 方式二：API接口采集模式

`trace_record.py`中提供两个接口，分别控制ftrace采集的启停，允许开发者将数据采集逻辑精细地嵌入到应用程序中。这种方式适合需要动态控制采集时机、条件触发采集或与业务逻辑深度集成的场景。

**1. 开始采集接口**

```python
ftrace_record_start(cpu_mask=None, bf_size=DEFAULT_TRACE_BUFFER_SIZE, event_cfg: TraceEventConfig = None, args=None)
```

**参数说明**

| 参数 | 说明 | 默认值 |
|-----|-----|-----|
| `cpu_mask` | cpu_mask 列表，指定采集的 CPU 核心。支持 `List[int]` 或字符串（如 `"0-3,8"`）| None，采集 **全部** CPU 核心 |
| `bf_size` | trace-cmd 使用的 **ring buffer 大小（单位：KB）**，用于在内核侧缓存 ftrace 事件。<br>当采集数据量较大时，可适当增大该值，避免环形缓冲区数据覆盖导致 **trace event 丢失** | `--bf_size=2820` |
|`event_cfg`| 事件采集配置（`TraceEventConfig`），用于控制采集事件类型：`sched`（调度）、`irq`（中断）。`1`表示打开，`0`表示关闭。|`TraceEventConfig(sched=1, irq=1)`|

**注意：推荐CPU采集核心数不超过64，采集时长30s左右，否则采集数据量可能过大，解析落盘耗时较长，需耐心等待。**

**2. 停止采集并保存数据接口**

```python
def ftrace_record_stop(output: str)
```

**参数说明**

| 参数 | 说明 | 默认值 |
|-----|-----|-----|
| `output` | 输出文件的路径和名称 | `trace.dat` |

**使用示例**:

```python
import trace_record

def train(): 
    # 方式一：传入字符串范围（可混合）
    trace_record.ftrace_record_start(cpu_mask="0-4,7,10") 
    
    # 方式二：传入列表
    trace_record.ftrace_record_start(cpu_mask=[0,1,2,3,4]) 
    
    profiling_start() 

    # 模型运行...

    profiling_stop() 
    trace_record.ftrace_record_stop(output="trace.dat")
```

### 4. 数据采集后处理

`trace_convert.py`脚本用于将原始 ftrace 数据转换为 SQLite DB 格式（默认）或 Chrome Trace JSON 格式，并与所采集的profiling数据时间轴对齐，以便导入 MindStudio Insight 可视化工具联合展示与分析。
> 请注意Profiling采集配置。当前仅支持ftrace与Text类型的Profiling联合展示，暂不支持与DB类型的Profiling数据联合展示。当ftrace导出为db格式时，如果需要与Text类型的Profiling联合分析，请使用 `--format=json` 回退到 JSON 格式。

**使用方法**

```bash
python trace_convert.py [-h] [--input INPUT] [--output OUTPUT] [--format FORMAT] [--profiling_data PROFILING_DATA] [--pid_mapping PID_MAPPING]
```

**参数说明**

| 参数 | 说明 | 示例值 | 默认值 |
|------|------|--------|--------|
| `--input` | 输入的原始 trace 文件路径，通过 `trace_record.py` 生成 | `/path/to/trace_data.dat` | `trace.dat` |
| `--output` | 输出的文件路径 | `trace.db` 或 `trace.json` | `ftrace_mindstudio_insight_data.db` |
| `--format` | 指定输出的数据格式，支持 `db` 或 `json` | `json` | `db` |
| `--profiling_data` | 同步采集的Profiling数据文件路径，用于时间轴对齐，以便导入MindStudio Insight联合分析 | `/profiling/xxxx_ascend_pt` | - |
| `--pid_mapping` | 容器场景下，可传入pid映射文件路径，进行容器与宿主机PID转换，详见[容器内外PID命名空间隔离场景下采集](#5-容器内外pid命名空间隔离场景下采集) | `pid_mapping.json`| - |

**使用示例**

假设第一步采集的profiling数据在目录`result_dir/xxxx_ascend_pt`下。

执行命令：

```bash
# --input默认为trace.dat
python trace_convert.py --profiling_data=result_dir/xxxx_ascend_pt
```

多卡场景下，`--profiling_data`可指定为包含多卡的上级目录，或任意单卡数据目录。

### 5. 容器内外PID命名空间隔离场景下采集

默认情况下（即容器启动时，未设置`--pid=host`参数），Docker容器有自己独立的PID命名空间。在容器中运行模型，profiling采集的进程号，为容器内部进程号。而ftrace采集的时Linux Kernel内核数据，**与容器内进程号存在命名空间隔离，无法直接对齐**。

针对该问题，`trace_record.py`脚本中提供了接口，用于获取容器内PID与宿主机PID的映射关系（基于遍历`/proc/$PID/status`文件的方式）。

#### 方式一：命令行采集

1. 使用`trace_record.py`脚本采集ftrace数据时，打开`--NSpid`开关，得到`pid_mapping.json`交付件，记录了PID映射关系。
2. 使用`trace_convert.py`脚本进行数据后处理时，通过`--pid_mapping`参数，传入`pid_mapping.json`路径。脚本执行时，会自动将采集到的进程号转换为容器内进程号，以便于和profiling所采集进程号对应。

#### 方式二：API接口采集模式

可单独使用 trace_record.py 脚本中的 ContainerPidMapper 类，提供以下对外接口：

**1. 类构造函数**

```python
def __init__(self, output_file: str = "pid_mapping.json")
```

**2. PID映射关系dump功能**

```python
// 开启接口
def start(self, duration=None)
// 停止接口
def stop(self)
```

**说明**：参数`duration`代表映射关系dump采集的开启周期，当传入`None`时，仅dump一次，且无需调用stop接口；传入其它有效值时，每`duration`秒dump一次，并通过stop接口停止。

## 采集样例：基于vllm-ascend场景的ftrace数据与profiling数据联合分析

本样例给出一个在**Docker容器**内基于vllm-ascend（v0.11.0）进行离线推理服务，同步采集Profiling数据，在**宿主机**通过命令行方式采集ftrace数据，并将采集结果**导入MindStudio Insight进行联合分析**的简单样例，帮助用户快速上手。

### 1. 前置准备

vllm-Ascend镜像获取地址：<https://quay.io/repository/ascend/vllm-ascend?tab=tags>

vllm-Ascend文档：<https://docs.vllm.ai/projects/ascend/zh-cn/v0.11.0-dev/quick_start.html>

按照文档所示，获取镜像，启动容器。

**安装trace-cmd**：

+ Ubuntu安装命令：sudo apt-get install trace-cmd

+ CentOs安装命令：sudo yum install trace-cmd

**获取ftrace采集与转换脚本**

下载本仓库提供的ftrace采集与转换脚本至本地。

```bash
├── ftrace_tools
│   ├── trace_convert.py
│   └── trace_record.py
```

**环境变量配置**：

```bash
#  Load model from ModelScope to speed up download
export VLLM_USE_MODELSCOPE=True
# Set `max_split_size_mb` to reduce memory fragmentation and avoid out of memory
export PYTORCH_NPU_ALLOC_CONF=max_split_size_mb:256

# 开启vllm profiling采集（请根据实际需要指定profiling输出路径）
export VLLM_TORCH_PROFILER_DIR="/path/to/profiling/data"

# 开启绑核，将NPU 0 绑至CPU 0-15核
export CPU_AFFINITY_CONF=2,npu0:0-15
```

此处绑核仅为示例，请根据实际业务需要确定CPU绑核区间，建议采用NPU与CPU亲和性绑核。详见：[绑核优化-Ascend Extension for PyTorch-昇腾社区](https://www.hiascend.com/document/detail/zh/Pytorch/720/ptmoddevg/trainingmigrguide/performance_tuning_0060.html)

### 2. 进入容器，运行vllm-ascend离线推理任务，同步采集ftrace与Profiling数据

可参考以下推理脚本`Qwen3_8B.py`进行vllm-ascend离线推理任务，脚本将同步采集Profiling数据：

```python
import os
from vllm import LLM, SamplingParams

prompts = [
    "Hello, my name is",
    "The future of AI is",
]
sampling_params = SamplingParams(temperature=0.8, top_p=0.95)
llm = LLM(
        model="Qwen/Qwen3-8B",
        max_model_len=26240
)
# 开启profiling采集
llm.start_profile()
outputs = llm.generate(prompts, sampling_params)
# 停止profiling采集
llm.start_profile()
for output in outputs:
    prompt = output.prompt
    generated_text = output.outputs[0].text
    print(f"Prompt: {prompt!r}, Generated text: {generated_text!r}")
```

> 请注意Profiling采集配置。当前仅支持ftrace与Text类型的Profiling联合展示，暂不支持与DB类型的Profiling数据联合展示。

在**宿主机**启动ftrace采集：

```bash
# 进入脚本所在目录
cd /home/xxx/msinsight/scripts/ftrace_tools

# record_time为-1代表持续采集，需Ctrl+C手动终止
# 针对CPU0-15核采集，打开容器内外PID映射开关
python trace_record.py --record_time=-1 --cpu=0-15 --NSpid 
```

ftrace采集期间，同步在**容器中**运行推理脚本：

```bash
python Qwen3_8B.py
```

**注意：`--record_time`为-1，代表持续采集模式，ftrace采集完成后，需及时`Ctrl+C`终止采集进程**。

**采集结果**
vllm-ascend离线推理任务运行，profiling成功采集，打屏回显如下：

```bash
......

[INFO] [1070251] profiler.py: Start parsing profiling data: /home/tangke/result_dir/profiling0113/ubuntu122_1069691_20260113031336165_ascend_pt
[INFO] [1070260] profiler.py: CANN profiling data parsed in a total time of 0:00:06.039457
[INFO] [1070251] profiler.py: All profiling data parsed in a total time of 0:00:34.928982
Prompt: 'Hello, my name is', Generated text: ' Lucy and I am an 8 year old who loves to draw and write stories'
Prompt: 'The future of AI is', Generated text: ' a topic that has been widely discussed, with many people expressing both excitement and concern'
```

得到如下形式的profiling采集交付件：

```bash
.
└── profiling
    └── ubuntu122_1069691_20260113031336165_ascend_pt
        ├── ASCEND_PROFILER_OUTPUT
        ├── FRAMEWORK
        ├── logs
        ├── PROF_000001_20260113031336168_JJIHFMPCABFRIEEB
        ├── profiler_info_0.json
        └── profiler_metadata.json
```

ftrace采集成功打屏回显如下：

```bash
......
[2026-02-11 08:16:24,486] [INFO]:Ending record, cleaning up...
[2026-02-11 08:16:24,487] [INFO]:Stopping trace-cmd record process
[2026-02-11 08:16:28,488] [INFO]:trace-cmd record process stopped
[2026-02-11 08:16:28,488] [INFO]:Run command/usr/bin/trace-cmd clear
[2026-02-11 08:16:28,573] [INFO]:Run command/usr/bin/trace-cmd reset
[2026-02-11 08:16:30,912] [INFO]:Cleanup finished
```

得到如下形式的ftrace采集结果（默认保存在ftrace脚本同级目录下）：

```bash
.
├── ftrace_tools
│   ├── trace.dat # trace-cmd record采集结果
│   ├── pid_mapping.json #容器内外PID映射信息
│   ├── trace_convert.py
│   └── trace_record.py
```

### 3. 数据后处理

> 该步骤将原始 ftrace 数据转换为 Chrome Trace JSON 格式，并与所采集的profiling数据时间轴对齐，以便导入 MindStudio Insight 可视化工具联合展示与分析。

假设所采集的profiling数据在目录`/path/to/profiling/xxxx_ascend_pt`下。trace-cmd采集结果`trace.dat`与容器内外PID映射信息`pid_mapping.json`在`trace_convert.py`同级目录下。

执行命令：

```bash
# 进入脚本所在目录
cd /home/xxx/msinsight/scripts/ftrace_tools
# --input默认为trace.dat
python trace_convert.py --profiling_data=/path/to/profiling/xxxx_ascend_pt --pid_mapping=pid_mapping.json
```

> 多卡场景下，`--profiling_data`可指定为包含多卡的上级目录，或任意单卡数据目录。

转换结果`ftrace_mindstudio_insight_data.db`默认保存在当前目录下。可将其导入MindStudio Insight，进行可视化分析。

```bash
.
├── ftrace_tools
│   ├── trace.dat
│   ├── ftrace_mindstudio_insight_data.db # ftrace转换结果
│   ├── pid_mapping.json
│   ├── trace_convert.py
│   └── trace_record.py
```

### 4. 导入MindStudio Insight联合分析
>
>**注意**：当前暂不支持Text类型数据与DB类型数据混合显示。若Profiling为Text和DB混合场景，需要提前删除其中的DB交付件`analysis.db`与`ascend_pytorch_profiler_x.db`。
![](./assets/hybrid_text_db_data.png)

打开MindStudio Insight可视化软件，首先导入Profiling数据：

<img src="./assets/import_profiling_data.png" width="500">

随后，在同一工程中导入ftrace数据：

<div style="display:flex; align-items:flex-start;">
<img src="./assets/import_within_same_project.png" width="350" style="margin-right:12px;">
<img src="./assets/import_ftrace_data.png" width="350" >
</div>

即可联合分析Profiling数据与ftrace数据：

![](./assets/joint_analysis.png)

CPU Scheduling泳道，可从CPU视角查看进程调度情况。
![](./assets/cpu_sche.png)

Process Scheduling泳道，可查看特定进程的调度状态。
![](./assets/process_sche.png)

利用MindStudio Insight的泳道置顶功能，可以将感兴趣的泳道放在一起联合分析。
![](./assets/lane_pinning_feature.png)

**联合分析思路**

一般而言，若在Profiling中观察到下发瓶颈点，可先通过CPU Scheduling泳道，概览性地观察该时间段内，下发流水中的热点线程，如Pytorch主线程、前向算子下发、反向算子下发、PTA二级流水下发（aclThread）等，是否存在进程抢占、软中断等情况。随后，观察特定进程的Process Scheduling泳道，进一步了解进程状态。最后，根据分析结果，进行针对性优化，例如改进绑核方案、核隔离、流水优化等。

## 常见问题FAQ

### 1. Profiling数据与ftrace数据联合导入失败，报错`File Conflict`

![](./assets/joint_import_failure.png)

**答：**
当前暂不支持Text类型数据与DB类型数据混合显示。由于 ftrace 数据默认导出为 DB 格式，如果此时您导入的 Profiling 数据仅为 Text 类型，则会产生格式冲突。

**解决方案**：若 Profiling 为 Text 和 DB 混合场景，需要提前删除 Profiling 中的 DB 交付件 `analysis.db` 与 `ascend_pytorch_profiler_x.db`，并在转换 ftrace 数据时，使用 `--format=json` 回退到 JSON 格式。
![Text与DB混合数据](./assets/hybrid_text_db_data.png)

### 2. 采集后ftrace数据部分CPU核存在大面积空白

![](./assets/partial_cpu_core_blank.png)

**可能性1**：ftrace采用环形缓冲区，这意味着缓冲区满后新数据会覆盖旧数据，可以在脚本`trace_record.py`中调整缓冲区大小`buffer_size`，例如调整至`buffer_size = '4096'`

**可能性2**：该核在此时段确实没有数据（可能性较小）

### 3. trace-cmd record 停止超时

![trace-cmd record超时](./assets/trace_cmd_record_timeout.png)
采集数据量较大时，trace-cmd record进程清理时间较长，超出了预设值范围。适当调大`trace_record.py`脚本中的`_INT_INTERVAL_SEC`与`_WAIT_TIMEOUT_SEC`参数即可。
