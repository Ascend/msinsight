# MindStudio Insight 版本发布说明

本文档记录 MindStudio Insight 所有正式版本的发布说明，包括新特性、优化项和修复的缺陷。

---

## 版本对比

| 版本 | 类型 | 发布日期 | 主要特性 |
|------|------|---------|---------|
| 26.0.0 | 稳定版 | 2026-04-29 | ftrace 联合分析、PyTorch snapshot 分析、Triton 片上内存可视化、Host-Device 内存拷贝分析 |
| 26.0.0-alpha.1 | 预览版 | 2026-02-04 | Host Bound 定位、RL 性能分析、Timeline 增强 |
| 8.3.0 | 稳定版 | 2026-02-03 | 集群性能分析、算子性能分析、内存分析、服务化分析 |

---

## 版本配套关系

| MindStudio Insight 版本 | CANN 版本 | 说明 |
|------------------------|-----------|------|
| 26.0.0 | 9.0.0 及以前 | [CANN 9.0.0 下载](https://www.hiascend.com/cann/download) |
| 26.0.0-alpha.1 | 8.5.0 及以前 | [CANN 8.5.0 下载](https://www.hiascend.com/cann/download) |
| 8.3.0 | 8.0.RC2 及以前 | [CANN 8.0.RC2 下载](https://www.hiascend.com/cann/download) |

---

## 版本列表

### 26.0.0 (最新稳定版)

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

- **ftrace 数据联合分析**: 提供简单易用的 trace-cmd 采集控制工具，能够指定 CPU 控制和采集时长，并将采集到的 ftrace 数据转换为 MindStudio Insight 可直接解析的格式，以便于查看 Timeline 并自动分析 CPU 调度、中断及进程/线程打断等统计信息
- **CPU 与运行进程间关系展示**: 在粗粒度绑核脚本中增加查询 CPU 和运行进程的可视化能力，辅助验证绑核是否生效
- **CPU/NPU/NUMA 拓扑关系展示**: 增加可视化能力，实现 CPU/NPU/NUMA 拓扑关系展示
- **容器与宿主机 pid 映射关系展示**: 支持容器内使用绑核分析场景
- **PyTorch 框架 snapshot 分析**: 能够导入和分析 PyTorch Profiler 生成的 snapshot 文件，提供类似于 memory_viz 的内存使用细节查看功能，并且能够处理更大（数十 GB 级）的 snapshot 文件，支持强化学习场景下的内存问题定位
- **Triton 片上内存使用过程可视化**: 支持展示 Triton 算子开发过程中 UB 移除问题的内存情况
- **Host-Device 间内存拷贝专项分析**: 内存拷贝按流按类型统计、内存拷贝该流按类型查询详细算子信息、算子点击跳转 timeline 位置
- **ACLGraph 的 JSONPrint 输出展示**: 保证相关的 Record 事件和 Wait 事件能同时结束，且 Wait 事件的起始时间应该小于 Record 时间的起始时间，展现从 Record 事件发向 Wait 事件的唤醒信息
- **ACLGraph 场景下 Stream 合并**: 实现自动合并 Stream 泳道的功能，从而减少前端需要显示的泳道数量
- **集成 Python 代替 PyInstaller**: Python 解释器 + 集群分析工具使用的三方库 + 集群分析 Python 脚本

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
