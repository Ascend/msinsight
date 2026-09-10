# Summary Design Document

<!-- md-trans-meta sourceCommit=49122e1f1d6621e5b8a8bbc1c0fdae9583659f48 translatedAt=2026-08-12T11:40:18.938Z pushedAt=2026-08-12T11:57:31.085Z -->

## 1. Document Objective and Scope

This document describes the data sources, analysis tools, interface commands, and display logic related to the `cluster/summary` page, targeting developers who need to maintain cluster overview, parallel strategy, communication domain performance, and single-rank details.

- Supports both TEXT and DB cluster data scenarios.

- This document currently covers the Summary display after importing PyTorch training profiler data.

- Page screenshots are provided for auxiliary understanding; key interfaces, data sources, and maintenance notes are subject to the tables in the main text.

## 2. Data Source and Preprocessing

### 2.1 Raw Cluster Data

The Summary page relies on summary data after cluster analysis. The raw data comes from PyTorch training profiling results.

#### TEXT Scenario

The typical directory structure is as follows:

```text
localhost.localdomain_139247_20230628101435_ascend_pt
├── profiler_info.json
├── profiler_metadata.json
└── ASCEND_PROFILER_OUTPUT
    ├── communication.json
    └── communication_matrix.json
```

Where:

- `communication.json`: Provides a data foundation for visualizing communication time consumption in scenarios involving multiple ranks or clusters where communication exists.

- `communication_matrix.json`: Provides basic information about small communication operators.

- Generating the above files typically requires configuring `profiler_level` in `experimental_config` to `torch_npu.profiler.ProfilerLevel.Level1` or `torch_npu.profiler.ProfilerLevel.Level2`.

#### DB Scenario

The typical directory structure is as follows:

```text
localhost.localdomain_139247_20230628101435_ascend_pt
├── profiler_info.json
├── profiler_metadata.json
└── ASCEND_PROFILER_OUTPUT
    └── analysis.db
```

When `export_type=torch_npu.profiler.ExportType.Db`, a `.db` file is generated under `ASCEND_PROFILER_OUTPUT`, and other JSON/CSV files are typically no longer generated.

For the collection method, see [Ascend Profiling Collection Documentation](https://www.hiascend.com/document/detail/zh/mindstudio/82RC1/T&ITools/Profiling/atlasprofiling_16_0090.html).

### 2.2 mstt Cluster Analysis Tool

The Summary and Communication pages depend on the processing results of the mstt cluster analysis tool. For tool instructions, see [mstt Cluster Analysis Tool](https://gitcode.com/Ascend/msprof-analyze/blob/master/README.md).

| Platform | Launch Method |
| --- | --- |
| Windows | `cluster_analysis.exe -d . -m mode` |
| Linux | `python3 cluster_analysis.py -d . -m mode` |
| macOS | `cluster_analysis -d . -m mode` |

On Linux, the tool is typically launched via Python because most Linux distributions include a Python interpreter by default. On Windows and macOS, to reduce the risk of users not having Python installed, PyInstaller is usually used to package the Python interpreter and analysis scripts into an executable file.

DB format data is usually appended with:

```shell
--data_simplification
```

The `mode` options include:

| Mode | Description |
| --- | --- |
| `all` | Process all cluster analysis data |
| `communication_time` | Process communication time related data |
| `communication_matrix` | Process communication matrix related data |

### 2.3 mstt Output Files

#### TEXT Output

```text
cluster_analysis_output
├── cluster_step_trace_time.csv
├── cluster_communication_matrix.json
├── cluster_communication.json
└── communication_group.json
```

TEXT data parsing process:

![TEXT Format Data Parsing Process](./figures/ff203f07-5738-41e8-9432-fade7bf2bd90.png)

- When `mode == communication_matrix`, it primarily processes `cluster_communication_matrix.json`, `cluster_step_trace_time.csv`, and `communication_group.json`.

- When `mode == communication_time`, it primarily processes `cluster_communication.json`.

#### DB Output

```text
cluster_analysis_output
└── cluster_analysis.db
```

DB data parsing process:

![DB format data parsing process](./figures/9d03b22e-a128-4255-b4ea-25f31eb1b79b.png)

## 3. Summary Page Data Display

### 3.1 Page Capability Overview

| Page Area | Interface Command | TEXT Data Source | DB Data Source | Description |
| --- | --- | --- | --- | --- |
| Basic Information | `summary/queryTopData` | `cluster_base_info` table | `ClusterBaseInfo` table | Displays top-level summary information such as cluster tasks, card count, and communication. |
| Parallel Strategy Generation | `summary/set/parallelStrategy` | Parallel strategy parameters from user configuration or file reading | Parallel strategy parameters from user configuration or file reading | Generates a parallel strategy based on parameters such as TP/CP/EP/DP/PP. |
| Parallel Strategy Display | `parallelism/arrangement/all` | No fixed data table | No fixed data table | Calculates and displays card group relationships based on parallel strategy parameters. |
| Card Time Proportion Within Communication Domain | `parallelism/performance/data` | `step_statistic_info` table, data from `cluster_step_trace_time.csv` | `ClusterStepTraceTime` table | Displays the time proportion of different ranks within the communication domain. |
| Details | `summary/statistic` | Single Card Information | Single Card Information | Displays single-rank dimension statistical details. |

### 3.2 Basic Information

The Basic Information area displays the top-level summary information after cluster data parsing.

- Interface Command: `summary/queryTopData`

- TEXT Data Source: `cluster_base_info` table

- DB Data Source: `ClusterBaseInfo` table

TEXT data source illustration:

![summary_text_base_info](./figures/c9371099-f385-47ae-be4c-d48fc09c943b.png)

DB data source illustration:

![summary_db_base_info](./figures/e4e29247-0f8e-4029-9f5e-81692fcd9727.png)

### 3.3 Parallel Strategy Generation

The Parallel Strategy Generation area is used to generate a parallel strategy based on parameters entered by the user or read from a file.

![Parallel Strategy Generation Interface](./figures/43c5975a-cdfe-4b5f-81fe-81b3fff26b61.png)

- Interface Command: `summary/set/parallelStrategy`

- Data Source: Parallel strategy parameters configured by the user or read from a file

Points to note during maintenance:

1. Whether the frontend input parameter names are consistent with the backend protocol fields.

2. Whether the default values, value ranges, and constraint relationships of TP, CP, EP, DP, and PP are clearly defined.

3. Whether parameter changes synchronously affect the Parallel Strategy Display interface.

### 3.4 Parallel Strategy Display

The Parallel Strategy Display is used to visualize card group relationships. The meanings of common abbreviations are as follows:

| Abbreviation | Meaning |
| --- | --- |
| TP | tensor parallel |
| CP | context parallel |
| EP | expert parallel |
| DP | data parallel |
| PP | pipeline parallel |

Taking 16-card data as an example, if Algorithm selects `TP-CP-EP-DP-PP`, with `TP=2`, `CP=2`, `EP=1`, `DP=2`, and `PP=2`, the grouping process is as follows:

1. Initially, ranks 0 to 15 each form a separate group.

2. Because `TP=2`, two adjacent groups perform TP parallelism, for example, 0-1, 2-3, and so on, and the groups change to 8 groups. Two groups performing TP parallelism are enclosed by a single box in the display.

3. Because `CP=2`, two adjacent groups perform CP parallelism, for example, the 0-1 group and the 2-3 group perform CP parallelism, and the groups change to 4 groups. Two groups performing CP parallelism are separated by two boxes in the display.

4. Because `EP=1`, the groups do not change.

5. Because `DP=2`, two adjacent groups perform DP parallelism, and the groups change to 2 groups. Two groups performing DP parallelism are separated by two boxes in the display.

6. Because `PP=2`, two adjacent groups perform PP parallelism, and the groups finally change to 1 group. Two groups performing PP parallelism are enclosed by a single box in the display.

Display illustration:

![Parallel Strategy Display](./figures/ead77096-c3fb-44e3-b45f-98a3e76526ed.png)

- Interface Command: `parallelism/arrangement/all`

- Data Source: Generated by computing from Parallel Strategy parameters, with no fixed data table.

### 3.5 Card Time Occupation Display Within Communication Domain

The card time occupation within a communication domain is used to display the time occupation ratio of different ranks in a specified communication domain, helping to identify slow ranks or communication imbalance issues.

Page illustration:

![Card Time Occupation Display Within Communication Domain 1](./figures/b35ef53b-6299-4f9b-bb7e-f6515bcdf522.png)

![Card Time Occupation Display Within Communication Domain 2](./figures/67a348ed-3187-4580-a756-3134896d0c59.png)

- Interface Command: `parallelism/performance/data`

- TEXT Data Source: `step_statistic_info` table, with underlying data from `cluster_step_trace_time.csv`

- DB Data Source: `ClusterStepTraceTime` table

TEXT data source illustration:

![step_statistic_info](./figures/0bbea588-d08d-4b4d-96ee-5e14cfac295f.png)

During maintenance, pay attention to the following:

1. Whether the field names and units are consistent between TEXT and DB scenarios.

2. Whether filter criteria such as Communication Domain, rank, step, or iteration are consistent with the frontend request.

3. If expert suggestions such as slow ranks and slow links depend on this data, verify the display on both the Summary and Communication sides synchronously.

### 3.6 Details

The Details area displays statistical information at the single-rank dimension.

![Details Interface](./figures/049e4e63-f3b5-42fd-9afb-68dabd723059.png)

- Interface Command: `summary/statistic`

- Data Source: Single Card Information

The current document does not expand the complete request/response fields of <code>summary/statistic</code>. If supplemented later, the field names, types, and default values should be confirmed first from the frontend request encapsulation, backend protocol definitions, and test cases.

## 4. Code Entry

When maintaining the Summary page, you can first check the implementation at the following locations. The specific paths may change as the code evolves, so always refer to the repository source code when modifying the documentation.

| Direction | Code Entry | Description |
| --- | --- | --- |
| Frontend module | `modules/cluster` | Summary and Communication both belong to the cluster frontend module. |
| Frontend request encapsulation | `modules/cluster/src/utils/RequestUtils.ts` | You can confirm the request commands related to Summary, Communication, and parallelism. |
| Backend command constants | `server/src/modules/defs/ProtocolDefs.h` | You can confirm the request command strings. |
| Backend Summary module | `server/src/modules/summary` | You can confirm the Summary handler, protocol, and database/process logic. |
| Backend plugin registration | `server/src/modules/Plugins.cpp` | You can confirm whether the Summary plugin is registered. |
| Communication-related document | `Communication.md` | Both Summary and Communication depend on the cluster analysis results, and their data sources are related. |

## 5. Development Steps for Adding or Modifying Summary Capabilities

1. **Confirm the data scenario**: First confirm whether the change affects both TEXT and DB scenarios.

2. **Confirm the data source**: Clarify whether the field comes from the CSV/JSON of `cluster_analysis_output`, `cluster_analysis.db`, or user input parameters.

3. **Supplement backend query or computing logic**: Add queries, computations, and exception handling in the relevant Summary handler/process/database.

4. **Supplement protocol fields**: Update request/response structures, JSON conversion logic, and command constants.

5. **Synchronize frontend display**: Update request encapsulation, page components, table/chart fields, and i18n text.

6. **Verify consistency between TEXT and DB**: For the same page capability, ensure that field meanings, units, and sorting are consistent across the two data scenarios as much as possible.

7. **Synchronize documentation**: If new interfaces, fields, data sources, or interactions are added, update this document and the relevant user guide accordingly.

## 6. Verification Methods

### 6.1 Static Verification

- Check whether the image paths referenced in this document exist.

- Check whether the interface commands can be found in the frontend request encapsulation or backend protocol definitions.

- Check whether the TEXT/DB data source table names are consistent with the parsing logic.

- Check whether the external links are still accessible.

### 6.2 Functional Verification

It is recommended to cover at least the following scenarios:

1. Import TEXT cluster data and verify the Basic Information, Parallel Strategy, Communication Domain time occupation, and detail display.

2. Import DB cluster data and verify that the same page capabilities are displayed correctly.

3. Modify TP/CP/EP/DP/PP parameters and verify the Parallel Strategy Generation and display results.

4. Verify whether the card time occupation ratio within the communication domain meets expectations under different rank, step, or iteration conditions.

5. Verify abnormal scenarios: missing `cluster_analysis_output`, missing key tables, empty data, missing fields, or incorrect data format.

## 7. Items to Be Confirmed

The following content is not elaborated in this document yet and should be supplemented after confirmation in the source code or testing:

- Complete request/response fields for `summary/queryTopData`, `summary/set/parallelStrategy`, `parallelism/arrangement/all`, `parallelism/performance/data`, and `summary/statistic`.

- Exact class names and call chains of the Summary module's backend handler, protocol, and database/process.

- Complete constraint relationships, default values, and exception return formats of Parallel Strategy parameters.

- The precise dependency between expert suggestions (such as slow ranks and slow links) and the data on the Summary page.
