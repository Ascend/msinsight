# vLLM 运行时进程关注案例

## 1. 目标

本文沉淀 vLLM / vLLM-Ascend 类 LLM Serving 场景中，Host CPU 绑核诊断需要重点关注的运行时进程和 CPU 热路径。它是 `mindstudio-cpu-binding` 的单独案例文档，用于指导只读进程发现、Snapshot 解读和报告解释。

核心原则：**只根据采集证据识别角色；不要把上游模块名直接等同于一定存在的 OS 进程。**

## 2. 为什么 vLLM 不能只看 Worker

vLLM 的 CPU 侧关键路径不只包含模型执行 worker。上游 vLLM V1 架构中，API server/frontend、engine core、scheduler、worker process、输入输出处理和 IPC 队列共同影响 TTFT、TPOT、tokens/s、QPS 和 p99 latency。

因此，CPU 绑核诊断应区分：

- API server / frontend：HTTP 请求、输入准备、streaming 输出。
- Engine core / scheduler：调度、KV cache、batching、执行协调。
- Model worker / TP worker：模型执行和设备交互，通常是 locality 绑定重点。
- Tokenizer / input / output / detokenizer：CPU 热路径；只有采集到进程或线程证据时才作为具体对象展示。
- Tensor IPC / queue：scheduler-worker 通信路径；用于解释 CPU/IPC 抖动。
- Ray / Ascend runtime：扩展场景，缺少证据时标记信息缺口，不做确定结论。

## 3. 角色优先级

| 优先级 | 角色 | `role_hint` | 识别线索 | 绑核关注点 |
|---|---|---|---|---|
| P0 | Engine Core | `vllm_engine_core` | `vllm.v1.engine.core`、`EngineCore`、`engine_core` | CPU 侧调度主路径，影响 batching、TTFT、TPOT 和 p99 |
| P0 | Scheduler | `vllm_scheduler` | `scheduler`、`scheduler.schedule` 且属于 vLLM 命令 | 调度和请求编排，避免与 worker 高负载线程过度竞争 |
| P0 | TP Worker | `vllm_tp_worker` | `VllmWorker-<rank>`、`--local-rank`、`--rank`、tensor-parallel 线索 | 最接近设备执行，优先观察 NPU/NUMA locality |
| P0 | Worker | `vllm_worker` | `vllm` + `worker`、`WorkerProc`、`worker_main` | 模型执行进程，关注 CPU range、NPU 映射和跨 NUMA |
| P1 | API Server / Frontend | `vllm_api_server` | `vllm.entrypoints.openai.api_server`、`vllm` + `api_server` | 影响 HTTP 请求、排队、输入准备、streaming 和 TTFT |
| P2 | Tokenizer | `vllm_tokenizer` | `vllm` + `tokenizer`、`tokenizer_group` | CPU 热路径；如果独立出现或 CPU 高，应在报告中突出 |
| P2 | Input Processor | `vllm_input_processor` | `vllm.v1.engine.input_processor`、`input_processor` | 输入处理 CPU 热路径，不默认假设为独立进程 |
| P2 | Detokenizer | `vllm_detokenizer` | `vllm.v1.engine.detokenizer`、`detokenizer` | 输出处理 CPU 热路径，不默认假设为独立进程 |
| P2 | Tensor IPC | `vllm_ipc` | `vllm.v1.engine.tensor_ipc`、`tensor_ipc`、`shm_broadcast` | scheduler-worker 通信路径，辅助解释 IPC 抖动 |
| P3 | Ray Worker | `ray_worker` | `ray::`、`RayWorker`、`raylet` | Ray executor 扩展候选，第一版只识别不强诊断 |
| P3 | Ascend/HCCL/SQ 线程 | `communication` / `sq_task` / `npu_fixed` | `hccl`、`dev*_sq_task`、NPU fixed 线程名 | vLLM-Ascend 中基于线程证据展示，不写成上游 vLLM 固定进程 |

## 4. 与报告章节的关系

### 当前 CPU 绑定状态

该章节应展示每个目标 PID 的 `角色标识`、NPU、当前 CPU range、有效 CPU range 和推荐 CPU range。vLLM 场景中，`角色标识` 可以是 `api`、`engine`、`tp0`、`tp1`，也可以是只读发现得到的 `vllm_api_server`、`vllm_engine_core`、`vllm_tp_worker`。

不要把这些角色统一改写为数字 Rank。`tp0`、`api`、`engine` 往往比 `rank0` 更能表达真实职责。

### 进程关系 / 父子进程树

vLLM worker 可能由 engine/main process 通过 multiprocessing 创建。进程树用于观察 API server、engine core、worker、tokenizer 或 Ray worker 的父子关系，帮助判断哪些进程属于同一服务实例。

进程树只展示进程级关系，不展开线程明细。线程细节留给“关键进程与线程”。

### 关键进程与线程

如果 Snapshot 中识别到 HCCL、SQ、NPU fixed、top CPU 线程或 tokenizer/input/output 相关高 CPU 线程，应在该章节展示。只有采集到证据时才提升这些线程的重要性；缺失时应列为信息缺口，而不是推断其不存在或一定存在。

## 5. vLLM-Ascend 和内部 fork 扩展原则

- 上游 vLLM baseline 不强假设 Ascend runtime 独立进程。
- vLLM-Ascend 中如果采集到 `hccl`、`dev*_sq_task` 或 NPU fixed 线程，应使用现有 `communication_threads`、`sq_task_threads`、`npu_fixed_threads` 机制展示。
- 内部 fork 可能使用 `api`、`engine`、`tp0`、`tp1` 等角色标识；这些标识应保留，并在报告中用 `角色标识` 呈现。
- Ray executor 是扩展路径。只读发现可以把 `ray::`、`RayWorker`、`raylet` 标成候选，但第一版不基于 Ray placement group 做确定绑核建议。

## 6. 诊断边界

当前阶段只把 vLLM 角色用于识别、解释和报告呈现，不直接改变 CPU range 推荐算法。

后续在有真实 vLLM-Ascend Snapshot 后，可以再将角色与诊断/规划联动：

- worker / TP worker：作为 NPU locality 绑定优先目标。
- engine core / scheduler：分配稳定 CPU 子集，避免与高负载 worker 过度重叠。
- API server：避免被 worker 挤压，用于解释 TTFT、queueing latency、streaming 抖动。
- tokenizer/input/output：作为 CPU 热路径和线程过载判断补充。
- Ray / Ascend runtime：基于真实采集证据再提升为一等诊断角色。
