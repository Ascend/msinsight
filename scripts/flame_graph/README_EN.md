# flame_graph

<!-- md-trans-meta sourceCommit=d0ec3bb2b1b20f22729e05aa1165a2a01f543023 translatedAt=2026-08-17T09:37:52.141Z pushedAt=2026-08-17T09:54:16.907Z -->

# 1. Introduction

The flame_graph tool parses the Host CPU-side PyTorch API call intervals from the `ascend_pytorch_profiler_{Rank_ID}.db` database exported by `Ascend PyTorch Profiler`, reconstructs the function call stack based on the start/end time relationships within the same thread, and generates an interactive HTML flame graph that can be viewed offline.

The generated HTML file can be opened directly in a browser. It supports viewing by thread, searching for function names, hovering over nodes to view duration details, and clicking nodes to zoom in for analysis. It is suitable for locating time-consuming hotspots in the Host CPU-side function call stack.

# 2. Usage Instructions

## 2.1 Ascend PyTorch Profiler Collection Settings

Generating a flame graph depends on the `PYTORCH_API` and `STRING_IDS` tables. Therefore, Profiler collection must include Host CPU-side activity data, that is, `torch_npu.profiler.ProfilerActivity.CPU`.

Example:

```python
torch_npu.profiler.profile(
    activities=[
        torch_npu.profiler.ProfilerActivity.CPU,
        torch_npu.profiler.ProfilerActivity.NPU,
    ]
)
```

## 2.2 Generating the Flame Graph HTML

The script is located in the `resources/profiler/scripts/flame_graph` directory under the MindStudio Insight installation directory. Run the script using the following command:

```shell
python3 flamegraph.py [-h] [-o OUTPUT] db_path
```

The parameters are described as follows:

- `db_path` (required)
- Type: file path
  
- Description: Specifies the path of the `ascend_pytorch_profiler_{Rank_ID}.db` file to be parsed.
  
- Example: `/path/to/ascend_pytorch_profiler_0.db`
  
- `-o`, `--output` (optional)
- Type: directory path
  
- Description: Specifies the output directory for the generated results. If the directory does not exist, the script attempts to create it automatically and generates `flamegraph.html` in that directory.
  
- Default value: current execution directory.
  
- Example: `--output /path/to/output_dir`

Example:

```shell
python3 flamegraph.py /path/to/ascend_pytorch_profiler_0.db --output /path/to/output_dir
```

After generation is complete, open the file in a browser:

```shell
/path/to/output_dir/flamegraph.html
```

# 3. Page Functions

- Thread switching: Use the `Thread` drop-down list to view the flame graph of all threads or a specified thread.

- Function search: Enter a keyword in the `Search` input box to highlight matching functions and display the match count.

- Node details: Hover the mouse over a flame graph node to view the function name, category, call count, total duration, self duration, and proportion.

- Zoom analysis: Click a node to zoom in and view its subtree. Click the current root node or `Reset` to restore the view.

- Clear search: Click `Clear` or press `Esc` to clear the search criteria; pressing `Esc` when no search criteria exist restores the default zoom view.

- Category coloring: Nodes are classified into Python, Framework, CANN/NPU, and Other based on function name keywords, and displayed in different colors.

# 4. Data Description

The script mainly reads the following tables:

- `PYTORCH_API`: Host CPU-side PyTorch API call data, including start time, end time, thread ID, API name ID, and data type.

- `STRING_IDS`: string mapping table, used to map API name IDs to readable strings.

The script parses the Host CPU-side API call intervals in the `PYTORCH_API` table that have `startNs`, `endNs`, and `globalTid`. The script processes API call records by thread, reconstructs the call stack hierarchy based on the time interval containment relationship of `startNs` / `endNs`, and then merges the results of all threads into the same flame graph. The script filters out `ProfilerStep#` marker events and skips abnormal records whose duration is less than or equal to 0. The number of abnormal records is output through logs.

The call stack node classification rules are as follows:

- `python`: The function name contains Python-side features such as `.py` or `python`.

- `python_framework`: The function name contains framework-side features such as `torch`, `torch_npu`, `aten::`, `c10::`, and `aten_`.

- `cann`: The function name contains CANN/NPU-side features such as `cann`, `ascendcl`, `aclnn`, `aclrt`, `aclmdl`, `aclprof`, and `hccl`.

- `unknown`: Nodes that cannot be identified by the preceding keywords.

# 5. Usage Constraints

- The input file must be a valid SQLite `.db` file and contain the `PYTORCH_API` and `STRING_IDS` tables.

- The input DB file size must not exceed 10 GB.

- The output path must be an existing directory for which the current user has write permission.

- When generating JSON data, the script limits the maximum API call stack depth to 1000 levels. Child nodes beyond this limit are not expanded further, and a warning is printed through the log.

- The generated HTML is a self-contained file. When the data volume is large, the file size and browser loading time increase with the number of nodes.

# 6. FAQs

## 6.1 Prompt Indicating That the DB Lacks Required Tables

Confirm that the input file is `ascend_pytorch_profiler_{Rank_ID}.db` exported by `Ascend PyTorch Profiler`, and check whether it contains the `PYTORCH_API` and `STRING_IDS` tables.

## 6.2 The Generated Result Is Empty

Possible causes include:

- The Profiler collection did not include CPU-side activity data.

- The `PYTORCH_API` table does not contain op, queue, or python_trace type data.

- The data contains only `ProfilerStep#` marker events or abnormal time intervals.

## 6.3 Prompt Indicating That DB File Exceeds 10 GB

This tool follows the same `.db` file check policy as the project, and the input DB file size is limited to 10 GB. Trim the collected data or select a smaller profiler DB, and then regenerate.

## 6.4 Slow Opening of HTML in the Browser

Flame graph data is directly inlined into the HTML. If the DB contains a large number of API calls, the HTML file may be large, and the browser requires a certain amount of time to parse and render it.
