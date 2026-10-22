# prof2memscope

# 1. Overview

The prof2memscope tool parses the tuning data collected by `Ascend Pytorch Profiler` and converts it into the db output format of memscope (formerly named msleaks).
After it is imported into Insight, the data can be displayed on the Memory Details ("MemScope") page, where the lifecycle of PTA memory allocation and the associated call stack can be analyzed in the same view, improving the usability of locating memory problems and tuning memory.

# 2. Usage Instructions

## 2.1 Ascend Pytorch Profiler Collection Settings

- The profiler collection must include the parameters `torch_npu.profiler.ProfilerActivity.CPU,
        torch_npu.profiler.ProfilerActivity.NPU`
- To view the memory block graph, set the parameter `profile_memory=True` during profiler collection
- To view the memory call stack graph, set the parameter `with_stack=True` during profiler collection

Examples are as follows:

```python
torch_npu.profiler.profile(
    activities=[
        torch_npu.profiler.ProfilerActivity.CPU,
        torch_npu.profiler.ProfilerActivity.NPU
        ],
    profile_memory=True,
    with_stack=True)
```

## 2.2 Converting Profiling Data into memscope Data

```shell
# Run this in the root directory of the memscope_adaptors project
python3 prof2memscope/dump.py [-h] [-s START] [-d DURATION] [-o OUTPUT_PATH] profiler_path
```

The parameters are described as follows

- profiler_path (required)
  - Type: path
  - Description: Specifies the path of the PyTorch Profiler data directory to be parsed.
  - Example: `/path/to/profiler_data/xxx_ascend_pt`
- -s, --start (optional)
  - Type: int; must be a positive integer.
  - Description: Specifies the start (Unix) timestamp for data parsing and trimming, in nanoseconds (ns). If it is not provided, trimming starts from the beginning of the Profiler data.
  - Default: the earliest start position of the Profiler data
  - Example: `--start 1752808501531484300` or `-s 1752808501531484300`
- -d, --duration (optional)
  - Type: int; must be a positive integer.
  - Description: Specifies the duration of the data to be trimmed from the start time, in nanoseconds (ns). If it is not provided, the data is trimmed from the start time to the end of the Profiler data.
  - Default: from the start time to the end of the data
  - Example: a duration of 5s: `--duration 5000000000` or `-d 5000000000`

- -o, --output_path (optional)
  - Type: path
  - Description: Specifies the output file path of the parsing result. By default, the output file is saved in the `dump_data` subdirectory of the Profiler path, with the file name `leaks_dump_{timestamp}.db`.
  - Default: in the `dump_data` subdirectory (created automatically if it does not exist) of the specified profiler directory path, with the file name `leaks_dump_{timestamp}.db`.
  - Example: --output_path /path/to/output/dump_data.db

# 3. Usage Constraints

- The parsed data source comes from the `torch.xx` binary in the `[profiler_data_dir]/FRAMEWORK` directory; check that the data exists before parsing.
- The parsed data supports only viewing the Python call stack, the memory block graph/line chart, and the memory block/event tables; **the memory disassembly function is unavailable**.
- The parsing environment depends on `torch_npu`; make sure that the parsing environment is the same as the collection environment or that its `torch_npu` version is higher than that of the collection environment.
- Parsing requires creating a db file under the specified `output_path` (or `[profiler_data_dir]/dump_data` by default); make sure that the user running the script has write permission on the corresponding directory.
