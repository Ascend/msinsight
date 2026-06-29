# vllm-ascend 场景 ftrace 与 Profiling 联合分析样例

本文给出一个在 Docker 容器内运行 vllm-ascend 离线推理、在宿主机采集 ftrace、再导入 MindStudio Insight 联合分析 Host Bound 问题的示例流程。

## 1. 前置准备

参考 vllm-ascend 官方文档获取镜像并启动容器：

- vllm-ascend 镜像：<https://quay.io/repository/ascend/vllm-ascend?tab=tags>
- vllm-ascend 文档：<https://docs.vllm.ai/projects/ascend/zh-cn/v0.11.0-dev/quick_start.html>

在宿主机准备 ftrace 工具脚本：

```text
ftrace_tools/
├── trace_record.py
├── trace_convert.py
└── exporters.py
```

如使用 trace-cmd 后端，需在宿主机安装 trace-cmd：

```bash
# Ubuntu
sudo apt-get install trace-cmd

# CentOS
sudo yum install trace-cmd
```

## 2. 容器内配置 Profiling 与业务环境

以下环境变量仅为示例，请按实际业务调整。

```bash
# 从 ModelScope 加速下载模型
export VLLM_USE_MODELSCOPE=True

# 降低显存碎片风险
export PYTORCH_NPU_ALLOC_CONF=max_split_size_mb:256

# 开启 vllm profiling 采集
export VLLM_TORCH_PROFILER_DIR="/path/to/profiling/data"

# 示例：将 NPU 0 亲和到 CPU 0-15
export CPU_AFFINITY_CONF=2,npu0:0-15
```

绑核策略请根据实际业务、NUMA 与 NPU/CPU 亲和关系配置。

## 3. 准备推理脚本

示例 `Qwen3_8B.py`：

```python
from vllm import LLM, SamplingParams

prompts = [
    "Hello, my name is",
    "The future of AI is",
]
sampling_params = SamplingParams(temperature=0.8, top_p=0.95)
llm = LLM(model="Qwen/Qwen3-8B", max_model_len=26240)

llm.start_profile()
outputs = llm.generate(prompts, sampling_params)
llm.stop_profile()

for output in outputs:
    print(f"Prompt: {output.prompt!r}, Generated text: {output.outputs[0].text!r}")
```

## 4. 宿主机启动 ftrace 采集

容器未使用 `--pid=host` 启动时，需要开启 `--NSpid` 生成容器 PID 与宿主机 PID 映射。

```bash
cd /home/xxx/msinsight/scripts/ftrace_tools

# record_time=-1 表示持续采集，需要业务结束后 Ctrl+C 手动停止
sudo python trace_record.py --record_time=-1 --cpu=0-15 --NSpid
```

如目标环境无法使用 trace-cmd，可强制使用 debugfs 后端：

```bash
sudo python trace_record.py --backend=debugfs --record_time=-1 --cpu=0-15 --output=trace.txt --NSpid
```

## 5. 容器内运行业务

在 ftrace 采集期间，在容器中运行推理脚本：

```bash
python Qwen3_8B.py
```

业务完成且 Profiling 结束后，回到宿主机终止 ftrace 采集进程。

采集完成后通常得到：

```text
ftrace_tools/
├── trace.dat          # trace-cmd 后端输出；debugfs 后端通常为 trace.txt
├── trace.txt          # debugfs 后端输出，与 trace.dat 二选一
├── pid_mapping.json   # 开启 --NSpid 后生成
├── trace_convert.py
└── trace_record.py
```

Profiling 目录通常类似：

```text
profiling/
└── xxx_ascend_pt/
    ├── ASCEND_PROFILER_OUTPUT
    ├── FRAMEWORK
    ├── logs
    ├── PROF_000001_...
    ├── profiler_info_0.json
    └── profiler_metadata.json
```

## 6. 转换 ftrace 数据

推荐转换为 SQLite DB，并传入 Profiling 目录做时间轴对齐：

```bash
cd /home/xxx/msinsight/scripts/ftrace_tools

python trace_convert.py \
  --input=trace.dat \
  --output=ftrace_data.db \
  --format=db \
  --profiling_data=/path/to/profiling/xxx_ascend_pt \
  --pid_mapping=pid_mapping.json
```

如果使用 debugfs 后端采集，请显式指定 `trace.txt`：

```bash
python trace_convert.py \
  --input=trace.txt \
  --output=ftrace_data.db \
  --format=db \
  --profiling_data=/path/to/profiling/xxx_ascend_pt \
  --pid_mapping=pid_mapping.json
```

注意：`--format` 与 `--output` 后缀必须匹配：

- `--format=db` 输出必须以 `.db` 结尾；
- `--format=json` 输出必须以 `.json` 结尾。

## 7. 导入 MindStudio Insight 联合分析

当前版本支持 ftrace DB 与 Profiling Text/DB 数据在同一工程中混合导入。建议流程：

1. 打开 MindStudio Insight，先导入 Profiling 数据目录；

   <img src="../assets/import_profiling_data.png" width="500">

2. 在同一工程中继续导入 `ftrace_data.db`；

   <div style="display:flex; align-items:flex-start;">
   <img src="../assets/import_within_same_project.png" width="350" style="margin-right:12px;">
   <img src="../assets/import_ftrace_data.png" width="350">
   </div>

3. 结合 CPU Scheduling、Process Scheduling 与 Profiling 时间线分析。

   ![](../assets/joint_analysis.png)

CPU Scheduling 泳道可从 CPU 视角查看进程调度情况。

![](../assets/cpu_sche.png)

Process Scheduling 泳道可查看特定进程的调度状态。

![](../assets/process_sche.png)

利用 MindStudio Insight 的泳道置顶功能，可以将感兴趣的泳道放在一起联合分析。

![](../assets/lane_pinning_feature.png)

分析思路：

- 先在 Profiling 中定位 Host Bound 或下发瓶颈时间段；
- 在 CPU Scheduling 泳道概览对应时间段的热点线程、抢占与 IRQ/SoftIRQ 情况；
- 在 Process Scheduling 泳道查看关键进程的 Running、Runnable、Sleeping 状态变化；
- 结合结果调整绑核、核隔离、流水并发或内存分配策略。

## 8. 常见注意事项

- 建议 CPU 采集核心数不超过 64，采集时长 30s 左右；长时间或大范围采集可能导致文件很大、转换耗时长。
- 若采集日志出现 lost/overwritten 事件告警，说明 ring buffer 发生覆盖或丢失，应增大 `--bf_size` 或缩小采集范围。
- 容器场景建议开启 `--NSpid`，否则 ftrace 中的宿主机 PID 可能无法与 Profiling 中的容器 PID 对齐。
- debugfs 后端输出通常为 `trace.txt`，转换时不要遗漏 `--input=trace.txt`。
