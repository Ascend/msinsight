# MindStudio Insight Release Notes

<!-- md-trans-meta sourceCommit=5026f5636b5a83b070ea810f3db20a00975088fc translatedAt=2026-08-12T11:44:22.495Z pushedAt=2026-08-12T11:57:31.128Z -->

This document records the release notes of all official versions of MindStudio Insight, including new features, optimizations, and fixed defects.

## Version Comparison

| Version        | Type           | Release Date | Major Features                                                                                         |
| -------------- | -------------- | ------------ | ------------------------------------------------------------------------------------------------------ |
| 26.1.0         | Stable | 2026-07-25   | ftrace proactive analysis, Memory Snapshot analysis enhancement, containerization and web access, Timeline interaction enhancement, scenario-based documentation improvement |
| 26.0.0         | Stable | 2026-04-29   | ftrace joint analysis, PyTorch Snapshot analysis, Triton on-chip memory visualization, Host-Device memory copy analysis |
| 26.0.0-alpha.1 | Preview | 2026-02-04  | Host Bound localization, reinforcement learning performance analysis, Timeline enhancement             |
| 8.3.0          | Stable | 2026-02-03   | cluster performance analysis, operator performance analysis, memory analysis, service-oriented analysis |

## Version Compatibility

| MindStudio Insight | CANN                | Description                                                  |
| ------------------ | ------------------- | ------------------------------------------------------------ |
| 26.1.0             | 9.1.0 and earlier   | [CANN 9.1.0 Download](https://www.hiascend.com/cann/download) |
| 26.0.0             | 9.0.0 and earlier   | [CANN 9.0.0 Download](https://www.hiascend.com/cann/download) |
| 26.0.0-alpha.1     | 8.5.0 and earlier   | [CANN 8.5.0 Download](https://www.hiascend.com/cann/download) |
| 8.3.0              | 8.0.RC2 and earlier | [CANN 8.0.RC2 Download](https://www.hiascend.com/cann/download) |

## Version List

### 26.1.0 (Latest Stable Version)

- **Release Date**: 2026-07-25

- **Release Tag**: [tag_MindStudio_26.1.0.B100_002](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.1.0.B100_002)

- **Compatibility**: Ascend Compatibility with CANN 9.1.0 and earlier versions

#### Version Overview

MindStudio Insight 26.1.0 has been enhanced in terms of Memory Snapshot analysis, proactive ftrace analysis, Containerized Deployment and web access, Timeline interaction efficiency, and scenario-based documentation, continuously improving data analysis efficiency, issue localization capabilities, and ease of use in Ascend AI performance tuning scenarios. The key highlights of this version are as follows:

- Enhanced ftrace data collection, conversion, federated import, and proactive analysis capabilities.

- Enhanced PyTorch Snapshot and Memory Snapshot analysis capabilities, supporting detailed analysis of potential leaking tensors and segments.

- Added support for containerized deployment, quick Docker start/stop, and IPv6 access scenarios.

- Optimized the Timeline search, pinning, time alignment, hardware metric unit, and Python call stack display experience.

- Supplemented scenario-based documentation for operator tuning, memory analysis, ftrace analysis, and containerized deployment.

#### Major Updates

- **Enhanced ftrace collection and analysis capabilities**: Added support for ftrace collection in tracefs/debugfs mode, enabling ftrace data capture in environments where trace-cmd is not available or installable. Added DB format export for ftrace data with **Timeline** visualization support. Introduced structured statistical analysis for ftrace data, covering context-switch counts, Running/Sleeping/Runnable durations, and IRQ latency/frequency metrics, with the ability to generate multi‑worksheet Excel analysis reports including charts.

- **Joint import of Profiling and ftrace data**: Added support for combined import of profiling and ftrace datasets. After import, **System View** displays corresponding analysis tabs based on data type, enabling joint analysis of device‑side profiling and host‑side scheduling data within the same project.

- **ftrace conversion efficiency optimization**: trace_convert now supports CPU‑filtered conversion, allowing selective processing of CPU Scheduling, IRQ/SoftIRQ, and Process Scheduling events per specified CPU. This reduces conversion time and output size for large‑scale trace data.

- **Memory Snapshot leak analysis**: Added potential leak tensor query capability to memsnapshot, identifying memory blocks allocated within a selected event interval but not released before the interval ends. The total, maximum, and minimum sizes of these potential leak tensors can be displayed.

- **Memory pool segment details display**: The memory pool status chart supports independent display of segment details, including basic statistics such as segment size, allocated size, gap size, block count, gap count, and max gap size. When the source alloc/map events exist, the event context is displayed synchronously; when events are missing, segment-level statistics can still be viewed.

- **Enhanced Memory Snapshot observation capabilities**: Added memsnapshot reservedSize line chart support, introduced loading progress prompts for Memory Snapshot parsing, and enriched data type differentiation capabilities for future snapshot‑based comparative analysis.

- **Containerized deployment and web access**: Added Dockerfile and related files for the MindStudio Insight image, providing a foundation for Docker image builds. Added streamer container start/stop scripts that support HTTP, HTTPS+mTLS, data directory mounting, certificate directory mounting, port specification, and dynamic port allocation, enabling rapid local or PoC deployment of Web analysis services.

- **IPv6 scenario support**: The frontend, backend, and base now support IPv6 address access, with startup parameters accepting valid IPv6 addresses. The JupyterLab plugin has been adapted for IPv6 access scenarios, improving usability in IPv6-only or dual-stack network environments.

- **Timeline interaction enhancement**: The **Timeline** now supports merged hierarchical display of units for multiple hardware metrics. Added a floating toolbar, unit pinning, right-click time alignment, one-click alignment for communication operators, secondary filtering in the find window, and a right-click menu scroll bar, significantly improving navigation efficiency for multi-rank and large-scale **Timeline** data.

- **Python call stack display optimization**: Python call stacks now support dedicated unit visualization, with performance optimizations to reduce rendering and interaction overhead for large-scale data.

- **Network and Communication data display enhancement**: Added support for displaying network-related data from NIC and ROCE tables, with unified unit formatting for byte, packet, and others. The **Communication_Op** table now includes a **deviceId** column, improving query capabilities for communication units, search, event views, and discovery lists in multi-rank scenarios.

- **Operator and link analysis capability enhancement**: Operator tuning introduces new analysis views such as Top Wall Reason, enabling stall top reason chart comparisons. Kernel E2E introduces backend tables and overall latency analysis to help locate bottlenecks in operator dispatch and execution pipelines.

- **Cluster analysis log persistence**: Stdout and stderr from cluster analysis tool execution can now be redirected to log files in the data folder, facilitating issue diagnosis when invoking cluster analysis capabilities via MindStudio Insight.

#### Download Links

| Platform                     | Download Link |
| ------------------------ | -------- |
| Windows                  | [MindStudio-Insight_26.1.0_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_win.exe)   |
| Linux x86_64 | [MindStudio-Insight_26.1.0_linux_x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_linux_x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_26.1.0_linux_aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_linux_aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_26.1.0_macos_x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_macos_x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_26.1.0_macos_aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_26.1.0_macos_aarch64.dmg) |
| JupyterLab Linux x86_64 | [mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_x86_64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_x86_64.whl) |
| JupyterLab Linux aarch64 | [mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_aarch64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/mindstudio_insight_jupyterlab-26.1.0-py3-none-linux_aarch64.whl) |
| Docker Image Ubuntu 22.04 x86_64 | [MindStudio-Insight_docker_image_26.1.0-ubuntu22.04_py3.10_x86_64.tar](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_docker_image_26.1.0-ubuntu22.04_py3.10_x86_64.tar) |
| Docker Image Ubuntu 22.04 aarch64 | [MindStudio-Insight_docker_image_26.1.0-ubuntu22.04_py3.10_aarch64.tar](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_docker_image_26.1.0-ubuntu22.04_py3.10_aarch64.tar) |
| Docker Image openEuler 24.03 x86_64 | [MindStudio-Insight_docker_image_26.1.0-openeuler24.03_py3.11_x86_64.tar](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_docker_image_26.1.0-openeuler24.03_py3.11_x86_64.tar) |
| Docker Image openEuler 24.03 aarch64 | [MindStudio-Insight_docker_image_26.1.0-openeuler24.03_py3.11_aarch64.tar](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.1.0.B100_002/MindStudio-Insight_docker_image_26.1.0-openeuler24.03_py3.11_aarch64.tar) |

### 26.0.0 (Stable Version)

- **Release Date**: 2026-04-29

- **Release Tag**: [tag_MindStudio_26.0.0.B120_0012](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.0.0.B120_0012)

- **Compatibility**: Compatible with Ascend CANN 9.0.0 and earlier versions

#### Version Overview

MindStudio Insight 26.0.0 provides visualized tuning capabilities for the full Ascend AI workflow, primarily targeting developers in Ascend AI tuning scenarios. The key highlights of this version are as follows:

- Host CPU-side profile data collection and analysis capabilities

- Support for PyTorch framework snapshot memory analysis

- Support for Triton operator tuning on-chip memory usage analysis

- Reinforcement learning scenario performance enhancement analysis

#### Major Updates

- **Ftrace data joint analysis:** Added a user-friendly `trace-cmd` collection control tool that supports CPU core specification and collection duration configuration. Collected ftrace data can be converted into a format directly parsable by MindStudio Insight, enabling **Timeline** visualization and automated analysis of CPU scheduling, interrupts, and process/thread preemption statistics.

- **CPU–process relationship visualization:** Enhanced coarse-grained core binding scripts with visual query capabilities to display CPU–process mappings, facilitating validation of core binding effectiveness.

- **CPU/NPU/NUMA topology visualization:** Added visualization capabilities to display CPU/NPU/NUMA topology relationships.

- **Container-to-host pid mapping visualization:** Supports core binding analysis scenarios within containers.

- **PyTorch framework snapshot analysis:** Enables importing and analyzing snapshot files generated by PyTorch Profiler, providing memory usage detail inspection similar to `memory_viz`. Capable of handling larger snapshot files (tens of GB), supporting memory issue diagnosis in reinforcement learning scenarios.

- **Triton on-chip memory visualization:** Supports visualizing memory status for UB overflow issues during Triton operator development.

- **Host–Device memory copy specialized analysis:** Memory copies are now aggregated by stream and type, with per-stream, per-type queries for detailed operator information, and click‑through navigation from operator details to the corresponding Timeline position.

- **ACLGraph JSONPrint output visualization:** Ensures that related Record and Wait events end simultaneously, with Wait event start times occurring before Record event start times, exposing wake‑up information from Record events to Wait events.

- **ACLGraph stream merging:** Automatically merges Stream units to reduce the number of lanes displayed in the frontend.

- **Python integration replacing PyInstaller:** Adopts a Python interpreter + third‑party libraries used by cluster analysis tools + cluster analysis Python scripts as the packaging approach.

#### Download Links

| Platform | Download Link |
|------|---------|
| Windows | [MindStudio-Insight_26.0.0_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_win.exe) |
| Linux x86_64 | [MindStudio-Insight_26.0.0_linux_x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_linux_x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_26.0.0_linux_aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_linux_aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_26.0.0_macos_x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_macos_x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_26.0.0_macos_aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/MindStudio-Insight_26.0.0_macos_aarch64.dmg) |
| JupyterLab Linux x86_64 | [mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_x86_64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_x86_64.whl) |
| JupyterLab Linux aarch64 | [mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_aarch64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0.B120_0012/mindstudio_insight_jupyterlab-26.0.0-py3-none-linux_aarch64.whl) |

### 26.0.0-alpha.1 (Preview Version)

- **Release Date**: 2026-02-04

- **Release Tag**: [tag_MindStudio_26.0.0-alpha.1](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_26.0.0-alpha.1)

- **Ascend Compatibility**: Compatible with Ascend CANN 8.5.0 and earlier versions

#### Major Updates

- **Host Bound issue localization**: Supports Host performance analysis such as Linux Kernel Trace and ftrace.

- **RL performance analysis**: Supports MindStudio Insight Timeline scenario analysis.

- **Timeline enhancement**: Supports multi-dimensional analysis of func/operator/communication/memory.

- **JupyterLab plugin**: Supports Python package installation.

#### Download Links

| Platform | Download Link |
|------|---------|
| Windows | [MindStudio-Insight_26.0.0-alpha.1_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_win.exe) |
| Linux x86_64 | [MindStudio-Insight_26.0.0-alpha.1_linux_x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_linux_x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_26.0.0-alpha.1_linux_aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_linux_aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_26.0.0-alpha.1_macos_x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_macos_x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_26.0.0-alpha.1_macos_aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/MindStudio-Insight_26.0.0-alpha.1_macos_aarch64.dmg) |
| JupyterLab Linux x86_64 | [mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_x86_64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_x86_64.whl) |
| JupyterLab Linux aarch64 | [mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_aarch64.whl](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_26.0.0-alpha.1/mindstudio_insight_jupyterlab-26.0.0a1-py3-none-linux_aarch64.whl) |

### 8.3.0 (Stable Version)

- **Release Date**: 2026-02-03

- **Release Tag**: [tag_MindStudio_8.3.0](https://gitcode.com/Ascend/msinsight/releases/tag_MindStudio_8.3.0)

- **Compatibility**: Ascend Compatibility with CANN 8.0.RC2 and earlier versions

#### Major Updates

- MindStudio Insight supports performance analysis for cluster vLLM scenarios.

- MindStudio Insight supports operator performance analysis.

- MindStudio Insight supports Memory Analysis.

- MindStudio Insight supports service-oriented analysis.

#### Download Links

| Platform | Download Link |
|------|---------|
| Windows | [MindStudio-Insight_8.3.0_win.exe](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_win.exe) |
| Linux x86_64 | [MindStudio-Insight_8.3.0_linux-x86_64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_linux-x86_64.zip) |
| Linux aarch64 | [MindStudio-Insight_8.3.0_linux-aarch64.zip](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_linux-aarch64.zip) |
| macOS x86_64 | [MindStudio-Insight_8.3.0_darwin-x86_64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_darwin-x86_64.dmg) |
| macOS aarch64 | [MindStudio-Insight_8.3.0_darwin-aarch64.dmg](https://gitcode.com/Ascend/msinsight/releases/download/tag_MindStudio_8.3.0/MindStudio-Insight_8.3.0_darwin-aarch64.dmg) |

## Related Links

- [GitCode Releases](https://gitcode.com/Ascend/msinsight/releases)

- [MindStudio Insight Installation Guide](../install_guide/mindstudio_insight_install_guide.md)
