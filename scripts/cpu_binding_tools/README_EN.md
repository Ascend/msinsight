# CPU Binding Tools

<!-- md-trans-meta sourceCommit=abe5598d30081dacce42893e4424c38ecf0aec6b translatedAt=2026-08-17T09:37:52.227Z pushedAt=2026-08-17T09:41:11.214Z -->

This directory contains a set of Python tools that help developers and system engineers **collect, analyze, and visualize CPU binding information of processes/threads** on Ascend/NPU platforms. The tools can identify NUMA affinity and generate topology diagrams, list the process tree relationships of critical processes/threads of a model, provide core partitioning and CPU binding suggestions, and check whether the CPU binding allocation is reasonable while providing visual analysis. All scripts are based on Python and must run in a Linux environment with the `npu-smi` command available.

## 📌 Problem Background

Training/Inference workloads on Ascend devices may suffer severe performance degradation if CPU binding is not configured correctly. Common issues include:

- The main scheduling thread is preempted by other processes.

- `sq_task` threads contend for CPU cores.

- Cross-NUMA memory access increases latency.

- Cache pollution caused by the mixture of I/O and computation.

- High CPU utilization caused by frequent context switching.

To address these problems, engineers need to:

1. **Visualize the CPU/NPU/NUMA topology** for planning.

2. **Check the processes/threads of critical model PIDs/TIDs** to collect prerequisite information for CPU binding.

3. **Provide core partitioning and binding strategy recommendations** to facilitate the binding operation.

4. **Collect affinity and scheduling information from `/proc` and `npu-smi`** to verify the correctness of the binding.

`cpu_binding_tools` includes the preceding functions.

## ✨ Feature Highlights

- **Topology diagram**: `topology_visualizer.py` generates an interactive HTML diagram showing NUMA nodes, NPUs, and their interconnect relationships.

- **Critical process tree**: `key_pstree_visualizer.py` locates NPU-related PIDs, builds subtrees, and supports CLI search.

- **CPU binding suggestions**: `cpu_binding_suggestion.py` generates a Markdown guide containing example commands.

- **Data collection**: `cpu_affinity_data_collection.py` scans NPU processes, `dev*_sq` tasks, and datawork processes, and outputs PSR/affinity.

- **Visualization Notebook**: Use `cpu_affinity_data_visualizer.py` to draw heatmaps and affinity diagrams in Jupyter, with support for interactive filtering.

## 🛠 Installation and Environment Preparation

```bash
# It is recommended to use a virtual environment with Python 3.8 or later, and a Jupyter Notebook/JupyterLab environment is required
conda create -n XXX python=3.11 jupyterlab=4.3.5

# Install dependencies
pip install -r requirements.txt
```

> Note: The host must have the Ascend driver and tools installed, with `npu-smi` available in `PATH`; the Python environment must be able to access `/proc`; and a Jupyter Notebook/JupyterLab environment is required to use this tool.

## 📂 Usage Examples of Each Tool

The following describes the workflow in the Notebook (`cpu_binding_visual.ipynb`). To use it, execute the corresponding cells in the Jupyter environment:

### 1. Dependency Installation

```bash
pip install -r requirements.txt
```

Run this once when you open the Notebook for the first time.

### 2. Topology Diagram

```bash
python topology_visualizer.py
```

Generate `ascend_topo.html`, which contains:

- Server nodes, NUMA nodes, and NPUs

- Various interconnections (HCCS, PIX, PXB, PHB, SYS, SIO, etc.)

You can view it in a browser or embed it in Jupyter.

### 3. Critical Process Tree Visualization

```bash
python key_pstree_visualizer.py
```

The script automatically locates NPU processes and `dev*_sq` tasks. You can invoke it through code to pass additional PIDs/names/regular expressions:

```python
from key_pstree_visualizer import KeyPstreeVisualizer
kv = KeyPstreeVisualizer()
roots = kv.build_pstree(extra_input=["python", 1234])
kv.print_tree(roots)
kv.interactive_search(roots)
```

### 4. CPU Binding Recommendations

```python
from cpu_binding_suggestion import CpuBindingSuggestion
from IPython.display import Markdown, display

display(Markdown(CpuBindingSuggestion.generate_markdown()))
```

Generate a CPU binding guide in Markdown format. You can add sample PIDs to customize the content.

### 5. CPU Binding Data Collection and Visualization

```bash
python cpu_affinity_data_collection.py [--csv] [--npu-process kw1 kw2...] [--datawork-process kw1 kw2...]
```

- Output affinity information to the screen; add `--csv` to write to a CSV file.

- You can specify keywords to filter thread names.

```python
from cpu_affinity_data_visualizer import run_notebook_app
run_notebook_app("path/to/your/data.csv")
```

- Start the visualization app

- Parameters: data file path (str)

Notebook example:

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

After collection is complete, the visualization interface can be launched automatically for interactive analysis of heatmaps, scatter plots, and filtering.

**Q: A message is displayed indicating that `npu-smi` cannot be found or a timeout occurs.**

A: Confirm that the Ascend driver/tools are installed and available in `$PATH`, verify with `which npu-smi`, and manually run `npu-smi info`.

**Q: The display is blank or an error is reported in Jupyter.**

A: Ensure that all dependencies are installed (`pyvis`, `plotly`, `ipywidgets`, etc.). If the widgets do not work, run `jupyter nbextension enable --py widgetsnbextension`.

**Q: When Jupyter Notebook is started for the first time, charts still cannot be rendered after running pip install -r requirements.txt.**

A: Refresh the web page and rerun the cell. If the charts still cannot be rendered, restart Jupyter Notebook.

**Q: The topology HTML has no interactive effects.**

A: The script injects `topo_interactions.js` to implement mouse zooming. Ensure that this file is in the same directory as the HTML file and has write permission.

**Q: The process tree misses some PIDs.**

A: The tool only collects PIDs reported by `npu-smi` or tasks matching `dev*_sq`. You can add other processes by name or regex matching via `--extra` or by calling `resolve_user_input`.

**Q: Does the script require root privileges?**

A: Reading `/proc/<pid>/task/...` may require permissions over other users' processes. Start it with the user running the app, or elevate privileges.

**Q: Can it run on non-Ascend hardware?**

A: Some features (NPU-related parsing) return empty results. The topology visualization still displays NUMA/CPU information, but without NPU nodes.

## 🏁 Summary

`cpu_binding_tools` provides a lightweight, standalone toolset for diagnosing and planning CPU binding on Ascend/NPU systems. By collecting affinity data, drawing topology diagrams, and offering actionable suggestions, it helps engineers optimize performance and avoid common issues.

Contributions and improvements are welcome. For guidelines, see `CONTRIBUTING.md` in the project root directory.
