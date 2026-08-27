# Communication Design Document

<!-- md-trans-meta sourceCommit=63323a8f22b6b37afd86f8821f5e7972ffe625b0 translatedAt=2026-08-12T11:38:18.177Z pushedAt=2026-08-12T11:57:31.084Z -->

## 1. Document Purpose and Scope

This document describes the data sources, interface commands, and frontend/backend code entry points related to the `cluster/communication` page, targeting developers who need to maintain the communication matrix, communication latency, operator list, bandwidth, and distribution chart.

- Both TEXT and DB data scenarios are supported.

- Screenshots on the page are for reference only. The actual key interfaces, paths, and data mappings may vary.

## 2. Interfaces and Data Mapping

### 2.1 Raw Data (ATT-Processed Files)

#### TEXT

![communication_text_data](./figures/communication_text_data.png)

#### DB

![communication_db_data](./figures/communication_db_data.png)

### 2.2 Processed DB Data Content

#### TEXT

![communication_processed_text_data](./figures/communication_processed_text_data.png)

#### DB

![communication_processed_db_data](./figures/communication_processed_db_data.png)

### 2.3 Page API Overview

| Page Data | URL Request | DB Data Type | Text Data Type | Description |
| --- | --- | --- | --- | --- |
| ![communication_page_data_1](./figures/communication_page_data_1.png) | `communication/matrix/bandwidthInfo` | ![communication_db_data_1](./figures/communication_db_data_1.png) | ![communication_text_data_1_1](./figures/communication_text_data_1_1.png) ![communication_text_data_1_2](./figures/communication_text_data_1_2.png) | Matrix bandwidth details. |
| ![communication_duration_iterations_1](./figures/communication_duration_iterations_1.png) | `communication/duration/iterations` | ![communication_duration_iterations_2](./figures/communication_duration_iterations_2.png) | ![communication_duration_iterations_3](./figures/communication_duration_iterations_3.png) | Iteration list and communication latency range. |
| ![communication_matrix_group_1](./figures/communication_matrix_group_1.png) | `communication/matrix/group` | ![communication_matrix_group_2](./figures/communication_matrix_group_2.png) | ![communication_matrix_group_3](./figures/communication_matrix_group_3.png) The underlying data comes from: ![communication_matrix_group_4](./figures/communication_matrix_group_4.png) | Communication matrix group information. |
| ![communication_sortOpNames_1](./figures/communication_sortOpNames_1.png) | `communication/matrix/sortOpNames` |  | ![communication_sortOpNames_2](./figures/communication_sortOpNames_2.png) Underlying data: ![communication_sortOpNames_3](./figures/communication_sortOpNames_3.png) | Operator name sorting and aggregation results. Whether the DB scenario is supported depends on the source code implementation. |
| ![communication_operatorNames_1](./figures/communication_operatorNames_1.png) | `communication/duration/operatorNames` | ![communication_operatorNames_2](./figures/communication_operatorNames_2.png) | ![communication_operatorNames_3](./figures/communication_operatorNames_3.png) Data: ![communication_operatorNames_4](./figures/communication_operatorNames_4.png) ![communication_operatorNames_5](./figures/communication_operatorNames_5.png) ![communication_operatorNames_6](./figures/communication_operatorNames_6.png) | Operator name list in the communication latency view. |
| ![communication_operatorLists_1](./figures/communication_operatorLists_1.png) | `communication/operatorLists` | ![communication_operatorLists_2](./figures/communication_operatorLists_2.png) | ![communication_operatorLists_3](./figures/communication_operatorLists_3.png) Data: ![communication_operatorLists_4](./figures/communication_operatorLists_4.png) | Operator list view. |
| ![communication_duration_list_1](./figures/communication_duration_list_1.png) | `communication/duration/list` | ![communication_duration_list_2](./figures/communication_duration_list_2.png) | ![communication_duration_list_3](./figures/communication_duration_list_3.png) ![communication_duration_list_4](./figures/communication_duration_list_4.png) | Communication latency detail list. It is recommended that experts derive this from data computation. |
| ![communication_operatorDetails_1](./figures/communication_operatorDetails_1.png) | `communication/operatorDetails` | ![communication_operatorDetails_2](./figures/communication_operatorDetails_2.png) | ![communication_operatorDetails_3](./figures/communication_operatorDetails_3.png) | Operator details. |
| ![communication_distribution_1](./figures/communication_distribution_1.png) | `communication/distribution` | ![communication_distribution_2](./figures/communication_distribution_2.png) | ![communication_distribution_3](./figures/communication_distribution_3.png) | Communication distribution chart. |
| ![communication_bandwidth_1](./figures/communication_bandwidth_1.png) | `communication/bandwidth` | ![communication_bandwidth_2](./figures/communication_bandwidth_2.png) | ![communication_bandwidth_3](./figures/communication_bandwidth_3.png) | Bandwidth analysis. |

### 2.4 Code Entry

- Frontend request encapsulation: `modules/cluster/src/utils/RequestUtils.ts`

- Backend command constants: `server/src/modules/defs/ProtocolDefs.h`

- Backend plugin: `server/src/modules/communication/CommunicationPlugin.h`

- Plugin registration: `server/src/modules/Plugins.cpp`

- Protocol test: `server/src/test/modules/communication/protocol/CommunicationProtocolUtilTest.cpp`

- Request sample: `server/src/test/test_data/request.csv`

### 2.5 Notes

- `text` and `db` only indicate different data sources; the page capabilities and interface naming remain consistent.

- The images in the tables are retained to assist in quickly locating UI elements, but they are not the sole source of information.

- The DB scenario support status of `sortOpNames`, the specific algorithms for expert advice, and the complete response fields of each interface may vary according to the source code and test results.
