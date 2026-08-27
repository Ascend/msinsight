# prof2memscope

<!-- md-trans-meta sourceCommit=effd4fc699d4189b3cce8b80693b84cc443ba14b translatedAt=2026-08-17T09:37:42.600Z pushedAt=2026-08-17T10:46:39.224Z -->

# 1. Introduction

The prof2memscope tool is used to parse the tuning data collected by `Ascend PyTorch Profiler` and convert it into a .db file in the memscope (formerly msleaks) output format. After being imported into insight, the data can be displayed in "MemScope", allowing you to analyze the PTA memory allocation lifecycle and its association with call stacks in the same view, thereby enhancing memory issue locating and memory tuning usability.

# 2. Usage Instructions

## 2.1 Ascend PyTorch Profiler Collection Settings

- When collecting with the profiler, the parameters `torch_npu.profiler.ProfilerActivity.CPU, torch_npu.profiler.ProfilerActivity.NPU` must be included.

- To view the memory block graph, configure the parameter `profile_memory=True` during profiler collection.

- To view the memory call stack graph, configure the parameter `with_stack=True` during profiler collection.

The following is an example:

```python
torch_npu.profiler.profile(
    activities=[
        torch_npu.profiler.ProfilerActivity.CPU,
        torch_npu.profiler.ProfilerActivity.NPU
        ],
    profile_memory=True,
    with_stack=True)
```

## 2.2 Parsing Profile Data into memscope Data

```shell
# Execute in the root directory of the memscope_adaptors project
python3 prof2memscope/dump.py [-h] [-s START] [-d DURATION] [-o OUTPUT_PATH] profiler_path
```

The parameters are described as follows:

- `profiler_path` (required)

  - Type: path

  - Description: Specifies the path to the PyTorch Profiler data directory to be parsed. 

  - Example: `/path/to/profiler_data/xxx_ascend_pt`

- `-s`, `--start` (optional)

  - Type: int, must be a positive integer.

  - Description: Specifies the start (unix) timestamp for data parsing and cropping, in nanoseconds (ns). If not provided, cropping starts from the start time of the Profiler data.

  - Default value: the earliest position of the Profiler data

  - Example: `--start 1752808501531484300` or `-s 1752808501531484300`

- `-d`, `--duration` (optional)

  - Type: int, must be a positive integer.

  - Description: Specifies the duration of data to be trimmed starting from the start time, in nanoseconds (ns). If not provided, the data will be trimmed from the start time to the end position of the Profiler data.

  - Default value: from the start time to the end of the data

  - Example: 5s duration `--duration 5000000000` or `-d 5000000000`

- `-o`, `--output_path` (optional)

  - Type: path

  - Description: Specifies the output file path for the parsing result. By default, the output file is saved in the dump_data subdirectory under the Profiler path, with the file name leaks_dump_{timestamp}.db.

  - Default value: parsed into the dump_data subdirectory under the specified profiler directory path (created automatically if it does not exist), with the file name `leaks_dump_{timestamp}.db`. 

  - Example: `--output_path /path/to/output/dump_data.db`

# 3. Usage Constraints

- The parsed data source is the `torch.xx` binary file in the `[profiler_data_dir]/FRAMEWORK` directory. Before parsing, check and ensure that the data exists.

- The parsed data only supports viewing the Python call stack, memory block graph/line chart, and memory block/event table. **The memory breakdown feature is unavailable.**

- The parsing environment depends on `torch_npu`. Ensure that the parsing environment is consistent with the collection environment, or that the `torch_npu` version in the parsing environment is higher than that in the collection environment.

- During parsing, a .db file needs to be created under the specified `output_path` (or under `[profiler_data_dir]/dump_data` by default). Ensure that the user running this script has write permission on the corresponding directory.
