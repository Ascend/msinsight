# Communication部分设计文档

## 1. 文档目标与范围

本文说明 `cluster/communication` 页面相关的数据来源、接口命令和前后端代码入口，面向需要维护通信矩阵、通信耗时、算子列表、带宽和分布图的开发者。

- 支持 TEXT 与 DB 两种数据场景。
- 页面中的截图仅作为辅助，关键接口、路径和数据映射以正文为准。

## 2. 接口与数据映射关系

### 2.1 原始数据（ATT 处理后文件）

#### text 场景

![communication_text_data](./figures/communication_text_data.png)

#### db 场景

![communication_db_data](./figures/communication_db_data.png)

### 2.2 处理后 DB 数据内容

#### text 场景

![communication_processed_text_data](./figures/communication_processed_text_data.png)

#### db 场景

![communication_processed_db_data](./figures/communication_processed_db_data.png)

### 2.3 页面接口总览

| 页面数据 | URL 请求 | db 数据类型 | text 数据类型 | 说明 |
| --- | --- | --- | --- | --- |
| ![communication_page_data_1](./figures/communication_page_data_1.png) | `communication/matrix/bandwidthInfo` | ![communication_db_data_1](./figures/communication_db_data_1.png) | ![communication_text_data_1_1](./figures/communication_text_data_1_1.png) ![communication_text_data_1_2](./figures/communication_text_data_1_2.png) | 矩阵带宽详情。 |
| ![communication_duration_iterations_1](./figures/communication_duration_iterations_1.png) | `communication/duration/iterations` | ![communication_duration_iterations_2](./figures/communication_duration_iterations_2.png) | ![communication_duration_iterations_3](./figures/communication_duration_iterations_3.png) | 迭代列表与通信耗时范围。 |
| ![communication_matrix_group_1](./figures/communication_matrix_group_1.png) | `communication/matrix/group` | ![communication_matrix_group_2](./figures/communication_matrix_group_2.png) | ![communication_matrix_group_3](./figures/communication_matrix_group_3.png) 底层数据来源于：![communication_matrix_group_4](./figures/communication_matrix_group_4.png) | 通信矩阵分组信息。 |
| ![communication_sortOpNames_1](./figures/communication_sortOpNames_1.png) | `communication/matrix/sortOpNames` |  | ![communication_sortOpNames_2](./figures/communication_sortOpNames_2.png) 底层数据：![communication_sortOpNames_3](./figures/communication_sortOpNames_3.png) | 算子名排序与聚合结果；DB 场景是否支持需以源码实现为准。 |
| ![communication_operatorNames_1](./figures/communication_operatorNames_1.png) | `communication/duration/operatorNames` | ![communication_operatorNames_2](./figures/communication_operatorNames_2.png) | ![communication_operatorNames_3](./figures/communication_operatorNames_3.png) 数据：![communication_operatorNames_4](./figures/communication_operatorNames_4.png) ![communication_operatorNames_5](./figures/communication_operatorNames_5.png) ![communication_operatorNames_6](./figures/communication_operatorNames_6.png) | 通信耗时视图中的算子名列表。 |
| ![communication_operatorLists_1](./figures/communication_operatorLists_1.png) | `communication/operatorLists` | ![communication_operatorLists_2](./figures/communication_operatorLists_2.png) | ![communication_operatorLists_3](./figures/communication_operatorLists_3.png) 数据：![communication_operatorLists_4](./figures/communication_operatorLists_4.png) | 算子列表视图。 |
| ![communication_duration_list_1](./figures/communication_duration_list_1.png) | `communication/duration/list` | ![communication_duration_list_2](./figures/communication_duration_list_2.png) | ![communication_duration_list_3](./figures/communication_duration_list_3.png) ![communication_duration_list_4](./figures/communication_duration_list_4.png) | 通信耗时明细列表，专家建议由数据计算得到。 |
| ![communication_operatorDetails_1](./figures/communication_operatorDetails_1.png) | `communication/operatorDetails` | ![communication_operatorDetails_2](./figures/communication_operatorDetails_2.png) | ![communication_operatorDetails_3](./figures/communication_operatorDetails_3.png) | 算子详情。 |
| ![communication_distribution_1](./figures/communication_distribution_1.png) | `communication/distribution` | ![communication_distribution_2](./figures/communication_distribution_2.png) | ![communication_distribution_3](./figures/communication_distribution_3.png) | 通信分布图。 |
| ![communication_bandwidth_1](./figures/communication_bandwidth_1.png) | `communication/bandwidth` | ![communication_bandwidth_2](./figures/communication_bandwidth_2.png) | ![communication_bandwidth_3](./figures/communication_bandwidth_3.png) | 带宽分析。 |

### 2.4 代码入口

- 前端请求封装：`modules/cluster/src/utils/RequestUtils.ts`
- 后端命令常量：`server/src/modules/defs/ProtocolDefs.h`
- 后端插件：`server/src/modules/communication/CommunicationPlugin.h`
- 插件注册：`server/src/modules/Plugins.cpp`
- 协议测试：`server/src/test/modules/communication/protocol/CommunicationProtocolUtilTest.cpp`
- 请求样例：`server/src/test/test_data/request.csv`

### 2.5 说明

- `text` 与 `db` 仅表示数据来源不同，页面能力和接口命名保持一致。
- 表格中的图片仍保留，用于辅助快速定位 UI，但不作为唯一信息来源。
- `sortOpNames` 的 DB 场景支持情况、专家建议的具体算法、每个接口的完整响应字段，需以源码和测试结果为准。
