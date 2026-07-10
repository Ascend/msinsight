# CPU Binding Tools

This directory contains a set of Python tools that help developers and system engineers **collect, analyze, and visualize CPU binding information for processes and threads** on Ascend/NPU platforms. These tools identify NUMA affinity and generate topology diagrams, list process-tree relationships for model-critical processes and threads, provide core partitioning and CPU binding recommendations, and check whether CPU binding assignments are reasonable with visual analysis. All scripts are Python-based and must run in a Linux environment with the `npu-smi` command available.

---

## 📌 Background

Training and inference workloads on Ascend devices can suffer severe performance degradation if CPU binding is not configured correctly. Common issues include:

- The main scheduling thread being preempted by other processes
- `sq_task` threads contending for CPU cores
- Increased latency caused by cross-NUMA memory access
- Cache pollution caused by mixing I/O and compute workloads
- Frequent context switches that make CPU utilization look artificially high

To address these issues, engineers need to:

1. **Visualize the CPU/NPU/NUMA topology** for planning.
2. **Inspect processes and threads for model-critical PIDs/TIDs** before CPU binding.
3. **Generate core partitioning and CPU binding strategy recommendations** for the binding operation.
4. **Collect affinity and scheduling information from `/proc` and `npu-smi`** to verify that CPU binding is correct.

`cpu_binding_tools` provides these capabilities.

---

## ✨ Key Features

- **Topology diagram**: `topology_visualizer.py` generates an interactive HTML diagram that shows NUMA nodes, NPUs, and their interconnects.
- **Key process tree**: `key_pstree_visualizer.py` finds NPU-related PIDs, builds subtrees, and supports CLI search.
- **CPU binding recommendations**: `cpu_binding_suggestion.py` generates a Markdown guide with example commands.
- **Data collection**: `cpu_affinity_data_collection.py` scans NPU processes, `dev*_sq` tasks, and datawork processes, then outputs PSR and affinity information.
- **Visualization Notebook**: `cpu_affinity_data_visualizer.py` draws heatmaps and affinity charts in Jupyter, with interactive filtering support.

---

## 🛠 Installation and Environment Setup

```bash
# A virtual environment is recommended. Python 3.8 or later and a Jupyter Notebook/JupyterLab environment are required.
conda create -n XXX python=3.11 jupyterlab=4.3.5

# Install dependencies
pip install -r requirements.txt
```

> Note: The host must have the Ascend driver and tools installed, `npu-smi` must be in `PATH`, the Python environment must be able to access `/proc`, and Jupyter Notebook/JupyterLab is required to use this toolset.

---

## 📂 Tool Usage Examples

The following steps match the workflow in the Notebook (`cpu_binding_visual.ipynb`). In practice, run the corresponding cells in a Jupyter environment.

### 1. Install dependencies

```bash
pip install -r requirements.txt
```

Run this once when opening the Notebook for the first time.

### 2. Topology diagram

```bash
python topology_visualizer.py
```

This generates `ascend_topo.html`, which includes:

- Server nodes, NUMA nodes, and NPUs
- Various interconnects, including HCCS, PIX, PXB, PHB, SYS, and SIO

You can view it in a browser or embed it in Jupyter.

### 3. Key process tree visualization

```bash
python key_pstree_visualizer.py
```

The script automatically locates NPU processes and `dev*_sq` tasks. You can also call it from code to pass additional PIDs, names, or regular expressions:

```python
from key_pstree_visualizer import KeyPstreeVisualizer
kv = KeyPstreeVisualizer()
roots = kv.build_pstree(extra_input=["python", 1234])
kv.print_tree(roots)
kv.interactive_search(roots)
```

### 4. CPU binding recommendations

```python
from cpu_binding_suggestion import CpuBindingSuggestion
from IPython.display import Markdown, display

display(Markdown(CpuBindingSuggestion.generate_markdown()))
```

This generates a CPU binding guide in Markdown format. You can add example PIDs to customize the content.

### 5. CPU binding data collection and visualization

```bash
python cpu_affinity_data_collection.py [--csv] [--npu-process kw1 kw2...] [--datawork-process kw1 kw2...]
```

- Outputs affinity information to the screen; add `--csv` to write it to CSV.
- You can specify keywords to filter thread names.

```python
from cpu_affinity_data_visualizer import run_notebook_app
run_notebook_app("path/to/your/data.csv")
```

- Starts the visualization application.
- Parameter: path to the data file (`str`).

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

After data collection finishes, the visualization UI can start automatically for interactive heatmap, scatter plot, and filtering analysis.

**Q: `npu-smi` is not found or times out.**

A: Confirm that the Ascend driver and tools are installed and available in `$PATH`. Use `which npu-smi` to verify, and run `npu-smi info` manually.

**Q: The display is blank or errors occur in Jupyter.**

A: Make sure all dependencies are installed, including `pyvis`, `plotly`, and `ipywidgets`. If widgets do not work, run `jupyter nbextension enable --py widgetsnbextension`.

**Q: After running `pip install -r requirements.txt` when starting Jupyter Notebook for the first time, charts still do not render.**

A: Refresh the web page and rerun the cell. If charts still do not render, restart Jupyter Notebook.

**Q: The topology HTML has no interactive behavior.**

A: The script injects `topo_interactions.js` to enable mouse zooming. Make sure this file is in the same directory as the HTML file and that the directory is writable.

**Q: Some PIDs are missing from the process tree.**

A: The tool only collects PIDs reported by `npu-smi` or tasks that match `dev*_sq`. You can add other process names or regular-expression matches through `--extra` or by calling `resolve_user_input`.

**Q: Do the scripts require root privileges?**

A: Reading `/proc/<pid>/task/...` may require permission to access processes owned by other users. Start the tool as the same user that runs the application, or elevate privileges.

**Q: Can the tools run on non-Ascend hardware?**

A: Some NPU-related parsing features will return empty results. The topology visualizer can still show NUMA/CPU information, but there will be no NPU nodes.

---

## 🏁 Summary

`cpu_binding_tools` provides a lightweight, standalone toolset for diagnosing and planning CPU binding on Ascend/NPU systems. By collecting affinity data, drawing topology diagrams, and providing actionable recommendations, it helps engineers optimize performance and avoid common issues.

Contributions and improvements are welcome. For guidelines, see `CONTRIBUTING.md` in the project root directory.

---
