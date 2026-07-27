# MindStudio Insight 版本发布说明

本文档记录 MindStudio Insight 所有正式版本的发布说明，包括新特性、优化项和修复的缺陷。

---

## 版本对比

| 版本           | 类型   | 发布日期   | 主要特性                                                                                  |
| -------------- | ------ | ---------- | ----------------------------------------------------------------------------------------- |
| 26.1.0         | 稳定版 | 2026-07-25 | ftrace 主动分析、内存快照分析增强、容器化与 Web 访问、Timeline 交互增强、场景化资料完善   |
| 26.0.0         | 稳定版 | 2026-04-29 | ftrace 联合分析、PyTorch Snapshot 分析、Triton 片上内存可视化、Host-Device 内存拷贝分析   |
| 26.0.0-alpha.1 | 预览版 | 2026-02-04 | Host Bound 定位、强化学习性能分析、Timeline 增强                                         |
| 8.3.0          | 稳定版 | 2026-02-03 | 集群性能分析、算子性能分析、内存分析、服务化分析                                          |

---

## 版本配套关系

| MindStudio Insight 版本 | CANN 版本      | 说明                                                     |
| ----------------------- | -------------- | -------------------------------------------------------- |
| 26.1.0                  | 9.1.0 及以前   | [CANN 9.1.0 下载](https://www.hiascend.com/cann/download)   |
| 26.0.0                  | 9.0.0 及以前   | [CANN 9.0.0 下载](https://www.hiascend.com/cann/download)   |
| 26.0.0-alpha.1          | 8.5.0 及以前   | [CANN 8.5.0 下载](https://www.hiascend.com/cann/download)   |
| 8.3.0                   | 8.0.RC2 及以前 | [CANN 8.0.RC2 下载](https://www.hiascend.com/cann/download) |

---

## 版本列表

### 26.1.0 (最新稳定版)

- **发布日期**: 2026-07-25
- **发布标签**: [tag_MindStudio_26.1.0.B100_002](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.1.0.B100_002)
- **兼容性**: 兼容昇腾 CANN 9.1.0 及以前版本

#### 版本概述

MindStudio Insight 26.1.0 围绕内存快照分析、ftrace 主动分析、容器化部署与 Web 访问、Timeline 交互效率和资料场景化进行了增强，持续提升昇腾 AI 性能调优场景下的数据分析效率、问题定位能力和易用性。本版本核心亮点如下：

- 增强 ftrace 数据采集、转换、联合导入和主动分析能力
- 增强 PyTorch Snapshot 与内存快照分析能力，支持潜在泄漏 Tensor 和 segment 详情分析
- 支持容器化部署、Docker 快速启停和 IPv6 访问场景
- 优化 Timeline 查找、置顶、时间对齐、硬件指标泳道和 Python 调用栈展示体验
- 补充算子调优、内存分析、ftrace 分析、容器化部署等场景化资料

#### 主要更新

- **ftrace 采集与分析能力增强**: 支持 tracefs/debugfs 模式 ftrace 采集，在无法安装或使用 trace-cmd 的环境中也可采集 ftrace 数据；支持 ftrace DB 格式输出并适配 Timeline 展示；增加 ftrace 数据结构化统计能力，支持上下文切换、Running/Sleeping/Runnable 时间、IRQ 耗时和次数等维度分析，并可生成包含多工作表和图表的 Excel 分析报告。
- **Profiling 与 ftrace 数据联合导入**: 支持 profiling 数据和 ftrace 数据联合导入，导入后 System View 可根据数据类型展示对应分析页签，便于在同一工程中联合分析设备侧 Profiling 和 Host 侧调度数据。
- **ftrace 转换效率优化**: trace_convert 支持按 CPU 范围过滤转换，可仅转换指定 CPU 的 CPU Scheduling、IRQ/SoftIRQ 和 Process Scheduling 事件，减少大规模 trace 数据的转换耗时和输出体积。
- **内存快照潜在泄漏分析**: memsnapshot 增加潜在泄漏 Tensor 查询能力，可识别选中事件区间内申请但未在区间内释放的内存块，并展示潜在泄漏 Tensor 的总大小、最大大小和最小大小。
- **内存池 segment 详情展示**: 内存池状态图支持独立展示 segment 详情，包含 segment size、allocated size、gap size、block count、gap count、max gap size 等基础统计；在来源 alloc/map 事件存在时同步展示事件上下文，事件缺失时仍可查看 segment 级统计。
- **内存快照观测能力增强**: 支持 memsnapshot reservedSize 折线观测、新增内存快照解析进度加载提示，并为后续基于 Snapshot 的对比分析补充数据类型区分能力。
- **容器化部署与 Web 访问**: 新增 MindStudio Insight 镜像 Dockerfile 和相关文件，提供 Docker 镜像构建基础；新增 streamer 容器启停脚本，支持 HTTP、HTTPS + mTLS、数据目录挂载、证书目录挂载、指定端口和动态端口等场景，便于本地或 PoC 环境快速拉起 Web 分析服务。
- **IPv6 场景支持**: 前端、后端和底座支持 IPv6 地址访问，启动参数支持合法 IPv6 地址；JupyterLab 插件适配 IPv6 访问场景，提升纯 IPv6 或双栈网络环境下的可用性。
- **Timeline 交互增强**: Timeline 支持多硬件指标泳道合并层级展示，新增悬浮工具栏，支持同名泳道置顶泛化、右键时间对齐、通信算子一键对齐、查找窗口二级筛选和右键菜单滚动条等交互能力，提升多卡和大规模 Timeline 数据下的定位效率。
- **Python 调用栈展示优化**: Python 调用栈支持独立泳道展示，并对 Python stack 相关性能进行优化，降低大规模数据下的展示和交互开销。
- **网络与通信数据展示增强**: 支持展示 NIC 表和 ROCE 表中的网络相关数据，并统一优化 Byte、Packet 等单位展示；COMMUNICATION_OP 表适配 deviceId 列，提升多 device 场景下通信泳道、搜索、事件视图和发现列表等查询能力。
- **算子与链路分析能力增强**: 算子调优新增 Top Wall Reason 等分析视图，支持 stall top reason 图表对比；Kernel E2E 增加后端表格和整体耗时分析能力，辅助定位算子下发和执行链路中的耗时问题。
- **集群分析日志落盘**: 集群分析工具执行过程中的标准输出和标准错误可重定向到数据文件夹日志文件，便于 MindStudio Insight 调用集群分析能力时进行问题定位。

#### 资料更新

- **算子调优指导**: 补充算子调优下流水图常见问题的调优指导，帮助用户结合图表结果识别和优化算子执行瓶颈。
- **内存快照分析案例**: 补充 verl 场景下 Snapshot 数据采集和分析案例、PyTorch Snapshot 分析案例和 profiler memory analysis case，覆盖强化学习和大模型内存分析场景。
- **ftrace 工具和分析资料**: 补充 trace_analyze.py 用户指南、ftrace_tools 中英文 README、trace-cmd 与 Linux 内核规格约束、采集无数据告警说明等内容，完善 Host Bound 和 CPU 调度分析资料链路。
- **容器化部署资料**: 按 Docker 镜像容器规范补充容器化部署资料，并新增 streamer 中英文说明，覆盖镜像选择、容器启动、停止、端口配置、数据目录挂载和 HTTPS + mTLS 使用方式。
- **系统调优与场景化案例**: 补充 system_tuning 数据采集说明、单卡下发分析案例、慢 rank 调优案例，以及 Top War Stall Reason、算子类型汇总和 Ftrace 任务统计等资料内容。
- **资料结构与易用性优化**: 优化快速开始、用户指南、开发指南和 README 资料，调整资料目录结构，统一术语、图表编号、产品系列名称和链接格式，提升资料阅读和检索体验。

#### 下载地址

| 平台                     | 下载链接 |
| ------------------------ | -------- |
| Windows                  | [MindStudio-Insight_26.1.0_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_win.exe)   |
| Linux x86_64 | [MindStudio-Insight_26.1.0_linux_x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_linux_x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_26.1.0_linux_aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_linux_aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_26.1.0_macos_x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_macos_x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_26.1.0_macos_aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_macos_aarch64.dmg) |
| JupyterLab Linux x86_64 | [mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_x86_64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_x86_64.whl) |
| JupyterLab Linux aarch64 | [mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_aarch64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_aarch64.whl) |

---

### 26.0.0 (稳定版)

- **发布日期**: 2026-04-29
- **发布标签**: [tag_MindStudio_26.0.0.B120_0012](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.0.0.B120_0012)
- **兼容性**: 兼容昇腾 CANN 9.0.0 及以前版本

#### 版本概述

MindStudio Insight 26.0.0 提供昇腾 AI 全流程可视化调优能力，主要面向昇腾 AI 调优场景的开发者。本版本核心亮点如下：

- Host CPU 侧性能数据采集和分析能力
- 支持 PyTorch 框架 snapshot 内存分析
- 支持 Triton 算子调优片上内存使用分析
- 强化学习场景性能增强分析

#### 主要更新

- **ftrace 数据联合分析**: 提供简单易用的 trace-cmd 采集控制工具，能够指定 CPU 控制和采集时长，并将采集到的 ftrace 数据转换为 MindStudio Insight 可直接解析的格式，以便于查看 Timeline 并自动分析 CPU 调度、中断及进程/线程打断等统计信息。
- **CPU 与运行进程间关系展示**: 在粗粒度绑核脚本中增加查询 CPU 和运行进程的可视化能力，辅助验证绑核是否生效。
- **CPU/NPU/NUMA 拓扑关系展示**: 增加可视化能力，实现 CPU/NPU/NUMA 拓扑关系展示。
- **容器与宿主机 pid 映射关系展示**: 支持容器内使用绑核分析场景。
- **PyTorch 框架 snapshot 分析**: 能够导入和分析 PyTorch Profiler 生成的 snapshot 文件，提供类似于 memory_viz 的内存使用细节查看功能，并且能够处理更大（数十 GB 级）的 snapshot 文件，支持强化学习场景下的内存问题定位。
- **Triton 片上内存使用过程可视化**: 支持展示 Triton 算子开发过程中 UB 溢出问题的内存情况。
- **Host-Device 间内存拷贝专项分析**: 内存拷贝按流按类型统计、内存拷贝该流按类型查询详细算子信息、算子点击跳转 timeline 位置。
- **ACLGraph 的 JSONPrint 输出展示**: 保证相关的 Record 事件和 Wait 事件能同时结束，且 Wait 事件的起始时间应该小于 Record 时间的起始时间，展现从 Record 事件发向 Wait 事件的唤醒信息。
- **ACLGraph 场景下 Stream 合并**: 实现自动合并 Stream 泳道的功能，从而减少前端需要显示的泳道数量。
- **集成 Python 代替 PyInstaller**: Python 解释器 + 集群分析工具使用的三方库 + 集群分析 Python 脚本。

#### 下载地址

| 平台 | 下载链接 |
|------|---------|
| Windows | [MindStudio-Insight_26.0.0_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_win.exe) |
| Linux x86_64 | [MindStudio-Insight_26.0.0_linux_x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_linux_x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_26.0.0_linux_aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_linux_aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_26.0.0_macos_x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_macos_x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_26.0.0_macos_aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_macos_aarch64.dmg) |
| JupyterLab Linux x86_64 | [mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_x86_64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_x86_64.whl) |
| JupyterLab Linux aarch64 | [mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_aarch64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_aarch64.whl) |

---

### 26.0.0-alpha.1 (预览版)

- **发布日期**: 2026-02-04
- **发布标签**: [tag_MindStudio_26.0.0-alpha.1](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.0.0-alpha.1)
- **兼容性**: 兼容昇腾 CANN 8.5.0 及以前版本

#### 主要更新

- **Host Bound 问题定位**: 支持 Linux Kernel Trace、ftrace 等 Host 性能分析
- **RL 性能分析**: 支持 MindStudio Insight Timeline 场景分析
- **Timeline 功能增强**: 支持 func/算子/通信/内存多维度分析
- **JupyterLab 插件**: 支持 Python 包安装

#### 下载地址

| 平台 | 下载链接 |
|------|---------|
| Windows | [MindStudio-Insight_26.0.0-alpha.1_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_win.exe) |
| Linux x86_64 | [MindStudio-Insight_26.0.0-alpha.1_linux_x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_linux_x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_26.0.0-alpha.1_linux_aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_linux_aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_26.0.0-alpha.1_macos_x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_macos_x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_26.0.0-alpha.1_macos_aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_macos_aarch64.dmg) |
| JupyterLab Linux x86_64 | [mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_x86_64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_x86_64.whl) |
| JupyterLab Linux aarch64 | [mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_aarch64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_aarch64.whl) |

---

### 8.3.0 (稳定版)

- **发布日期**: 2026-02-03
- **发布标签**: [tag_MindStudio_8.3.0](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_8.3.0)
- **兼容性**: 兼容昇腾 CANN 8.0.RC2 及以前版本

#### 主要更新

- MindStudio Insight 支持集群 vllm 场景性能分析
- MindStudio Insight 支持算子性能分析
- MindStudio Insight 支持内存分析
- MindStudio Insight 支持服务化分析

#### 下载地址

| 平台 | 下载链接 |
|------|---------|
| Windows | [MindStudio-Insight_8.3.0_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_win.exe) |
| Linux x86_64 | [MindStudio-Insight_8.3.0_linux-x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_linux-x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_8.3.0_linux-aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_linux-aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_8.3.0_darwin-x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_darwin-x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_8.3.0_darwin-aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_darwin-aarch64.dmg) |

---

## 相关链接

- [GitCode Releases 页面](https://gitcode.com/Ascend/msinsight/releases)
- [MindStudio Insight 安装指南](../install_guide/mindstudio_insight_install_guide.md)

---

*最后更新: 2026-06-09*
