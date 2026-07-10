# CPU 绑核工具

本目录包含一组 Python 工具，用于帮助开发者和系统工程师在 Ascend/NPU 平台上**收集、分析和可视化进程/线程的 CPU 绑核信息**。工具可以识别 NUMA 亲和性并生成拓扑关系图、列举模型关键进程/线程的进程树关系、给出核区分和绑核建议，同时检查绑核分配是否合理并给出可视化分析。所有脚本均基于 Python，需在带有 `npu-smi` 命令的 Linux 环境下运行。

---

## 📌 问题背景

Ascend 设备上的训练/推理工作负载如果没有正确绑核，可能出现严重的性能下降。常见问题包括：

- 主调度线程被其他进程抢占
- `sq_task` 线程争抢 CPU 核
- 跨 NUMA 的内存访问导致延迟增大
- IO 与计算混合引起缓存污染
- 频繁上下文切换导致 CPU 利用率虚高

为了解决这些问题，工程师需要：

1. **可视化 CPU/NPU/NUMA 拓扑**以便规划。
2. **检查模型关键 PID/TID 的进程/线程**以便进行绑核前置信息收集。
3. **给出核分区和绑核策略建议**以便进行绑核操作。
4. **从 `/proc` 和 `npu-smi` 收集亲和性与调度信息**以便校验绑核正确性。

`cpu_binding_tools` 包含了上述功能。

---

## ✨ 功能亮点

- **拓扑图**：`topology_visualizer.py` 生成显示 NUMA 节点、NPU 及互连关系的交互式 HTML 图。
- **关键进程树**：`key_pstree_visualizer.py` 查找 NPU 相关 PID，构建子树并支持 CLI 搜索。
- **绑核建议**：`cpu_binding_suggestion.py` 生成包含示例命令的 Markdown 指南。
- **数据采集**：`cpu_affinity_data_collection.py` 扫描 NPU 进程、`dev*_sq` 任务、datawork 进程，并输出 PSR/亲和性。
- **可视化 Notebook**：通过 `cpu_affinity_data_visualizer.py` 在 Jupyter 中绘制热力图、亲和图，并支持交互筛选。

---

## 🛠 安装与环境准备

```bash
# 推荐使用虚拟环境，Python版本3.8以上，并需要Jupyter Notebook/JupyterLab环境
conda create -n XXX python=3.11 jupyterlab=4.3.5

# 安装依赖
pip install -r requirements.txt
```

> 注意：宿主机需安装 Ascend 驱动与工具，且 `npu-smi` 在 `PATH` 中；Python 环境可访问 `/proc`；同时使用该工具需要Jupyter Notebook/JupyterLab环境。

---

## 📂 各工具使用示例

以下为按照 Notebook (`cpu_binding_visual.ipynb`) 中的流程说明，具体使用在Jupyter环境下执行对应Cell即可：

### 1. 安装依赖

```bash
pip install -r requirements.txt
```

首次打开 Notebook 时执行一次即可。

### 2. 拓扑图

```bash
python topology_visualizer.py
```

生成 `ascend_topo.html`，图中包含：

- Server 节点、NUMA 节点、NPUs
- 各类互连（HCCS、PIX、PXB、PHB、SYS、SIO 等）

可在浏览器中查看或在 Jupyter 内嵌展示。

### 3. 关键进程树可视化

```bash
python key_pstree_visualizer.py
```

脚本会自动定位 NPU 进程和 `dev*_sq` 任务。可通过代码调用以传入额外 PID/名称/正则：

```python
from key_pstree_visualizer import KeyPstreeVisualizer
kv = KeyPstreeVisualizer()
roots = kv.build_pstree(extra_input=["python", 1234])
kv.print_tree(roots)
kv.interactive_search(roots)
```

### 4. 绑核建议

```python
from cpu_binding_suggestion import CpuBindingSuggestion
from IPython.display import Markdown, display

display(Markdown(CpuBindingSuggestion.generate_markdown()))
```

生成 Markdown 格式的绑核指南，可添加示例 PID 定制内容。

### 5. 绑核数据采集与可视化

```bash
python cpu_affinity_data_collection.py [--csv] [--npu-process kw1 kw2...] [--datawork-process kw1 kw2...]
```

- 输出亲和信息到屏幕；加 `--csv` 写入 CSV。
- 可指定关键字过滤线程名。

```python
from cpu_affinity_data_visualizer import run_notebook_app
run_notebook_app("path/to/your/data.csv")
```

- 启动可视化应用
- 参数：数据文件的路径 (str)

Notebook 示例：

```python
import os
from cpu_affinity_data_visualizer import run_notebook_app

NPU_PROCESS = "CommWorker DataWorker"
DATAWORK_PROCESS = ""
OUTPUT_FILE = "affinity_data.csv"
cmd = f"python3 cpu_affinity_data_collection.py --csv {('--npu-process '+NPU_PROCESS) if NPU_PROCESS else ''} {('--datawork-process '+DATAWORK_PROCESS) if DATAWORK_PROCESS else ''} > {OUTPUT_FILE}"
os.system(cmd)

if os.path.exists(OUTPUT_FILE) and os.path.getsize(OUTPUT_FILE) > 0:
    run_notebook_app(OUTPUT_FILE)
```

采集完成后可自动启动可视化界面，交互式分析热力图、散点图和筛选。

**Q：提示找不到 `npu-smi` 或超时。**

A：确认 Ascend 驱动/工具已安装并在 `$PATH` 中，用 `which npu-smi` 验证，手动运行 `npu-smi info`。

**Q：在 Jupyter 中显示为空白或报错。**

A：确保安装了所有依赖（`pyvis`、`plotly`、`ipywidgets` 等）。若控件无效，执行 `jupyter nbextension enable --py widgetsnbextension`。

**Q：第一次启动Jupyter Notebook时，运行完pip install -r requirements.txt后，还是无法渲染出图表**

A：请刷新网页重新运行Cell，如果仍然无法渲染出图表，请重新启动Jupyter Notebook。

**Q：拓扑 HTML 无交互效果。**

A：脚本会注入 `topo_interactions.js` 实现鼠标缩放。确保该文件与 HTML 同目录且有写权限。

**Q：进程树缺失某些 PID。**

A：工具只收集 `npu-smi` 报告的 PID 或符合 `dev*_sq` 的任务。可通过 `--extra` 或调用 `resolve_user_input` 添加其它名称/正则匹配进程。

**Q：脚本需要 root 权限吗？**

A：读取 `/proc/<pid>/task/...` 可能需要对其他用户进程有权限。请使用运行应用的用户启动，或提升权限。

**Q：可以在非 Ascend 硬件上运行吗？**

A：部分功能（NPU 相关解析）会返回空。拓扑可视化仍会展示 NUMA/CPU 信息，只是没有 NPU 节点。

---

## 🏁 总结

`cpu_binding_tools` 提供了一个轻量级、独立的工具集，用于诊断和规划 Ascend/NPU 系统上的 CPU 绑核。通过收集亲和数据、绘制拓扑、提供可操作建议，帮助工程师优化性能并规避常见问题。

欢迎贡献和改进——有关指南请参阅项目根目录下的 `CONTRIBUTING.md`。

---
