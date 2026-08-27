# MindStudio Insight Design Specification for device Memory Analysis Feature

<!-- md-trans-meta sourceCommit=63323a8f22b6b37afd86f8821f5e7972ffe625b0 translatedAt=2026-08-12T11:40:26.938Z pushedAt=2026-08-12T11:57:31.082Z -->

**Copyright © 2026 Ascend Community**

## Revision History

| Date | Revision Version | Revision Description | Author | Reviewer |
| --- | --- | --- | --- | --- |
| 2026-01-15 | v1.0 | Initial draft | Liu Pengcheng | Liao Yan |
| 2026-06-11 | v1.1 | Cleaned up template placeholders, supplemented verifiable device memory analysis scope, data sources, security and verification descriptions | - | - |

## Glossary

| Acronym | Full Name | Chinese Explanation |
| --- | --- | --- |
| msInsight | MindStudio Insight | MindStudio Insight performance analysis tool |
| Snapshot | PyTorch Memory Snapshot | PyTorch memory snapshot data |
| msMemScope | MindStudio Memory Scope | device memory collection and analysis tool |
| OOM | Out Of Memory | Out of memory |

# 1. Feature Overview

In large model training or inference performance tuning scenarios, device-side memory optimization is a key direction. Developers need to understand memory allocation, deallocation, peak usage, fragmentation, call stacks, and memory pool status to identify issues such as OOM, memory leaks, and inefficient device memory usage. MindStudio Insight provides graphical memory analysis capabilities to help developers analyze device-side memory usage.

## 1.1 Scope

This document covers the development design notes related to device-side memory analysis in MindStudio Insight, focusing on the following:

1. PyTorch Snapshot Memory Analysis: displays the memory block lifecycle, memory pool status, and memory event details.

2. MindStudio Memory Scope / msMemScope Memory Analysis: displays the call stack, memory block lifecycle, memory details exploded view, and memory details table.

3. Host-device memory copy analysis capabilities are described only as related capabilities. For specific design, refer to the corresponding module documentation and code.

This document does not supplement internal database schemas, algorithm details, or performance metrics that have not been verified in the repository source code, User Guide, or Release Notes.

## 1.2 Feature Requirement List

| Requirement ID | Requirement Name | Feature Description | Verifiable Material |
| --- | --- | --- | --- |
| 1 | Support PyTorch Snapshot Memory Analysis | Supports memory lifetime display, memory pool status analysis, and memory event details display in snapshots | `docs/zh/user_guide/memory_tuning.md` |
| 2 | Support MindStudio Memory Scope Memory Analysis | Supports call stack, memory block lifecycle, and memory classification breakdown after importing msMemScope data | `docs/zh/user_guide/memory_tuning.md` |

# 2. Requirement Scenario Analysis

## 2.1 Feature Requirement Sources and Value Overview

In reinforcement learning, large model training, and inference scenarios, memory changes may fluctuate with phase transitions. For example, phases such as `generate_sequence` and `actor_update` may exhibit different memory peaks or fragmentation characteristics. Using Snapshot or msMemScope data, developers can identify OOM, memory fragmentation, memory peaks, and inefficient device memory usage.

## 2.2 Typical Use Scenarios

### PyTorch Snapshot Scenario

1. Before running the model, call `torch_npu.npu.memory._record_memory_history()` to enable memory history recording.

2. Run the training or inference code to be analyzed.

3. Call `torch_npu.npu.memory._dump_snapshot("snapshot.pickle")` to export the `pickle` file.

4. Import the Memory Snapshot file in MindStudio Insight to view the Memory Block Lifecycle, Memory Pool Status, and Memory Event details.

### msMemScope Scenario

1. Use the msMemScope tool to collect memory result files.

2. Import result files in the `memscope_dump_{timestamp}.db` format.

3. View the Call Stack Flame Graph, Memory Block Lifetime Graph, Memory Details Exploded View, and Memory Details table in MindStudio Insight.

## 2.3 Constraints and Limitations

### 2.3.1 Hardware Limitations

Ascend NPU-related memory analysis scenarios are supported. For the specific hardware support scope, refer to the MindStudio Insight release notes and the corresponding data collection tool documentation.

### 2.3.2 System Limitations

The recommended environment documented in existing documentation is as follows:

1. Windows 10+.

2. Linux: `glibc > 2.30` is recommended.

3. macOS 13.0+.

4. It is recommended that the device running MindStudio Insight has more than 16 GB of memory.

5. It is recommended that the memory data file does not exceed 1 GB.

If the actual support matrix changes, the installation guide, release notes, and build scripts shall prevail.

### 2.3.3 Security and Sensitive Information

MindStudio Insight is a local analysis tool, but input files may contain user paths, function names, call stacks, symbol names, model execution information, and other content. Development and documentation examples should follow these principles:

- Use sanitized paths in examples, such as `/home/xxx/demo.py`.

- Do not include real usernames, site information, keys, passwords, or public network addresses in the documentation.

- Perform validity checks on the file paths, file types, and file sizes imported by users.

- If exception information needs to be output in logs, avoid printing sensitive paths and complete call stacks, and perform desensitization when necessary.

# 3. Overall Solution

## 3.1 Data Source

| Data Source | Data Format | Main Display Content | Description |
| --- | --- | --- | --- |
| PyTorch Snapshot | `pickle` | Memory Block Lifecycle, Memory Pool Status, Memory Event Details | Exported by `torch_npu.npu.memory._dump_snapshot()` |
| msMemScope | `memscope_dump_{timestamp}.db` | Call Stack Flame Graph, Memory Block Lifetime Graph, Memory Details Exploded View, Memory Details Table | Collected and generated by the msMemScope tool |

## 3.2 Processing Flow

1. The user collects Snapshot or msMemScope data.

2. The user imports the data file in MindStudio Insight.

3. The backend parses the data and establishes the data structures or database connections required for queries.

4. The frontend initiates query requests based on page interactions.

5. The backend returns charts, tables, details, or filtering results.

6. The frontend displays the memory lifecycle, call stack, memory pool status, and detailed data.

## 3.3 Relationship with Other Documents

- For detailed Snapshot design, see `support_snapshot_analysis.md`.

- For the user-side collection and usage process, see `docs/zh/user_guide/memory_tuning.md`.

- For general Memory module development instructions, see `Memory.md`.

# 4. Use Case 1: PyTorch Snapshot Memory Analysis

## 4.1 Design Goal

Supports importing PyTorch Snapshot data and displaying the memory pool status managed by PyTorch during model execution, assisting in locating memory fragmentation, memory peaks, and potential leak issues.

## 4.2 User Entry Point

Users collect and export a `pickle` file through the PyTorch NPU Memory Snapshot API. The collection example in the User Guide is as follows:

```python
torch_npu.npu.memory._record_memory_history(stacks='python')
# Run the model code.
torch_npu.npu.memory._dump_snapshot("model_memory_snapshot.pickle")
```

## 4.3 Page Capability

The Snapshot page can display:

- Memory Block Lifetime Graph.

- Memory Pool Status Diagram.

- Memory Block View.

- Memory Event view.

- Selected details.

- Automatically filter unreleased memory blocks within the interval.

For field meanings and user operations, refer to the "PyTorch Snapshot Data Memory Details (Memory Snapshot)" section in `docs/zh/user_guide/memory_tuning.md`.

## 4.4 Items to be Confirmed

The following content needs to be further confirmed from the source code or design review before being supplemented:

- Specific code path of the Snapshot parser.

- Backend database table structure or intermediate data structure.

- Complete request/response fields of the query interface.

- Performance metrics and exception return format for large files.

# 5. Use Case 2: msMemScope Memory Analysis

## 5.1 Design Goal

Supports importing DB result files collected by msMemScope, and graphically displays the device memory allocation and deallocation lifecycle, call stacks, and memory exploded view information.

## 5.2 Data Preparation

Supports importing files in the `memscope_dump_{timestamp}.db` format. The display capabilities documented in the User Guide include:

- Memory Block Lifetime Graph.

- Memory Details Exploded View.

- Python Call Stack Diagram.

- Memory Block View.

- Memory Event View.

- Inefficient device Memory Filtering.

## 5.3 Page Capability

After msMemScope data is imported, the page can display:

1. Call Stack Flame Graph: View Python call stacks by thread and function.

2. Memory Block Lifetime Graph: View memory allocation, deallocation, and access events.

3. Memory Details Exploded View: Display memory hierarchies such as CANN and PTA by type.

4. Memory Details Table: View details in the Memory Block view and Memory Event view.

For field meanings and user operations, refer to the "MemScope Data Memory Details" section in `docs/zh/user_guide/memory_tuning.md`.

# 6. DFX Design

## 6.1 Performance Design

- It is recommended to perform performance verification for large file imports, long time range queries, and zoom and pan operations.

- It is recommended to perform import and query verification for memory data files within 1 GB.

- Specific response times are not committed in this document; specific metrics shall be subject to performance test results.

## 6.2 Exception Handling Design

The following exception scenarios are recommended to be covered:

- File format mismatch.

- File corruption or missing fields.

- File too large, causing parsing failure or excessive processing time.

- The query time range is empty.

- Call stacks or access events are not collected.

- Some events in the Snapshot lack allocation or deallocation records.

## 6.3 Security Design

- No new external listening ports are added.

- No new authentication methods are added.

- File import paths and file types must undergo validity verification.

- Examples and logs must avoid exposing real user paths, passwords, keys, or site information.

- If a new third-party component is introduced, license and vulnerability checks must be completed in accordance with the project's third-party component import process.

## 6.4 Testability Design

Recommended test coverage:

- Normal import and detail query of snapshots.

- Normal import and detail query of msMemScope DB.

- Empty data, missing call stacks, and missing access events.

- Large file import.

- Incorrect file format.

- Page interactions such as filtering, zooming, dragging, searching, and copying.

# 7. Verification Methods

## 7.1 Static Verification

- Check whether relative links in the document exist.

- Check whether example paths have been desensitized.

- Check whether template placeholders, empty images, or unclosed Markdown tags still exist.

## 7.2 Functional Verification

- Import a PyTorch Snapshot `pickle` file and verify the lifetime diagram, memory block lifetime graph, and details table.

- Import an msMemScope DB file and verify the call stack flame graph, memory block lifetime graph, exploded view, and details table.

- Verify the prompts for abnormal files, empty data, and missing field scenarios.

# 8. Items to be Confirmed

- Backend parser paths, database structures, and complete interface definitions for Snapshot and msMemScope.

- Ownership of the development design document for Host-Device Memory Copy Analysis.

- Performance metrics, error codes, and alarm formats.
