# flame_graph

# 1. Overview

The flame_graph tool parses PyTorch API call intervals on the host CPU side from the `ascend_pytorch_profiler_{Rank_ID}.db` database exported by `Ascend PyTorch Profiler`, reconstructs the function call stack based on the start/end time relationship within the same thread, and generates an interactive HTML flame graph that can be viewed offline.

The generated HTML file can be opened directly in a browser. It supports viewing by thread, searching for function names, hovering the mouse over a node to view duration details, and clicking a node to zoom in for analysis. It is suitable for locating duration hotspots in the function call stack on the host CPU side.

# 2. Usage Instructions

## 2.1 Ascend PyTorch Profiler Collection Settings

Generating the flame graph depends on the `PYTORCH_API` and `STRING_IDS` tables, so the Profiler collection must include host CPU-side activity data, that is, `torch_npu.profiler.ProfilerActivity.CPU`.

Examples are as follows:

```python
torch_npu.profiler.profile(
    activities=[
        torch_npu.profiler.ProfilerActivity.CPU,
        torch_npu.profiler.ProfilerActivity.NPU,
    ]
)
```

## 2.2 Generating the Flame Graph HTML

The script is located in the `resources/profiler/scripts/flame_graph` directory of the MindStudio Insight installation directory. The command to run the script is as follows:

```shell
python3 flamegraph.py [-h] [-o OUTPUT] db_path
```

The parameters are described as follows:

- db_path (required)
  - Type: file path
  - Description: Specifies the path of the `ascend_pytorch_profiler_{Rank_ID}.db` file to be parsed.
  - Example: `/path/to/ascend_pytorch_profiler_0.db`

- -o, --output (optional)
  - Type: directory path
  - Description: Specifies the output directory of the generated result. If the directory does not exist, the script tries to create it automatically and generates `flamegraph.html` in it.
  - Default: the current working directory.
  - Example: `--output /path/to/output_dir`

Example:

```shell
python3 flamegraph.py /path/to/ascend_pytorch_profiler_0.db --output /path/to/output_dir
```

After generation is complete, open it with a browser:

```text
/path/to/output_dir/flamegraph.html
```

# 3. Page Functions

- Thread switching: use the `Thread` drop-down list to view the flame graph of all threads or of a specified thread.
- Function search: enter a keyword in the `Search` input box to highlight the matching functions and display the number of matches.
- Node details: hover the mouse over a flame graph node to view the function name, category, call count, total duration, self duration, and proportion.
- Zoom analysis: click a node to zoom in on its subtree; click the current root node or `Reset` to restore the view.
- Clear search: click `Clear` or press `Esc` to clear the search criteria; when there is no search criterion, pressing `Esc` restores the default zoomed view.
- Category coloring: nodes are classified into Python, Framework, CANN/NPU, and Other according to function name keywords and displayed in different colors.

# 4. Data Description

The script mainly reads the following tables:

- `PYTORCH_API`: PyTorch API call data on the host CPU side, including the start time, end time, thread ID, API name ID, and data type.
- `STRING_IDS`: string mapping table, used to map API name IDs to readable strings.

The script parses the host CPU-side API call intervals that have `startNs`, `endNs`, and `globalTid` in the `PYTORCH_API` table. It processes the API call records by thread, reconstructs the call stack hierarchy according to the containment relationship of the `startNs` / `endNs` time intervals, and then merges the results of all threads into a single flame graph. The script filters out `ProfilerStep#` marker events and skips abnormal records whose duration is less than or equal to 0. The number of abnormal records is output through logs.

The call stack node classification rules are as follows:

- `python`: the function name contains Python-side features such as `.py` or `python`.
- `python_framework`: the function name contains framework-side features such as `torch`, `torch_npu`, `aten::`, `c10::`, or `aten_`.
- `cann`: the function name contains CANN/NPU-side features such as `cann`, `ascendcl`, `aclnn`, `aclrt`, `aclmdl`, `aclprof`, or `hccl`.
- `unknown`: nodes that cannot be identified by the keywords above.

# 5. Usage Constraints

- The input file must be a valid SQLite `.db` file and must contain the `PYTORCH_API` and `STRING_IDS` tables.
- The size of the input DB file cannot exceed 10GB.
- The output path must be an existing directory on which the current user has write permission.
- When generating JSON data, the script limits the maximum API call stack depth to 1000 levels; child nodes beyond the limit are not expanded further, and a warning is printed through logs.
- The generated HTML is a self-contained file; when the data volume is large, the file size and the browser loading time increase with the number of nodes.

# 6. FAQs

## 6.1 The DB is reported to be missing required tables

Make sure that the input file is an `ascend_pytorch_profiler_{Rank_ID}.db` exported by `Ascend PyTorch Profiler`, and check whether it contains the `PYTORCH_API` and `STRING_IDS` tables.

## 6.2 The generated result is empty

Possible causes include:

- The Profiler collection did not include CPU-side activity data.
- The `PYTORCH_API` table does not contain data of the op, queue, or python_trace type.
- The data contains only `ProfilerStep#` marker events or abnormal time intervals.

## 6.3 The DB file is reported to exceed 10GB

This tool is consistent with the `.db` file check policy in the project, and the input DB file size is limited to 10GB. Trim the collected data or select a smaller profiler DB and generate the flame graph again.

## 6.4 Opening the HTML in a browser is slow

The flame graph data is inlined directly into the HTML. If the number of API calls in the DB is large, the HTML file may be large, and the browser needs some time to parse and render it.
