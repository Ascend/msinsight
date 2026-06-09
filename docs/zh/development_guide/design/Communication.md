# Communication部分设计文档

## 接口与数据映射关系

### 原始数据（ATT处理后文件）

#### text场景

![communication_text_data](./figures/communication_text_data.png)

#### db场景

![communication_db_data](./figures/communication_db_data.png)

### 处理后db数据内容

#### text场景

![communication_processed_text_data](./figures/communication_processed_text_data.png)

#### db场景

![communication_processed_db_data](./figures/communication_processed_db_data.png)

| 页面数据 | URL请求 | db数据类型 | text数据类型 |
| --- | --- | --- | --- |
| ![communication_page_data_1](./figures/communication_page_data_1.png) | communication/matrix/bandwidthInfo | ![communication_db_data_1](./figures/communication_db_data_1.png) | ![communication_text_data_1_1](./figures/communication_text_data_1_1.png) ![communication_text_data_1_2](./figures/communication_text_data_1_2.png) |
| ![communication_duration_iterations_1](./figures/communication_duration_iterations_1.png) | communication/duration/iterations | ![communication_duration_iterations_2](./figures/communication_duration_iterations_2.png) | ![communication_duration_iterations_3](./figures/communication_duration_iterations_3.png) |
| ![communication_matrix_group_1](./figures/communication_matrix_group_1.png) | communication/matrix/group | ![communication_matrix_group_2](./figures/communication_matrix_group_2.png) | ![communication_matrix_group_3](./figures/communication_matrix_group_3.png) 底层数据来源于：![communication_matrix_group_4](./figures/communication_matrix_group_4.png) |
| ![communication_sortOpNames_1](./figures/communication_sortOpNames_1.png) | communication/matrix/sortOpNames |  | ![communication_sortOpNames_2](./figures/communication_sortOpNames_2.png) 底层数据：![communication_sortOpNames_3](./figures/communication_sortOpNames_3.png) |
| ![communication_operatorNames_1](./figures/communication_operatorNames_1.png) | communication/duration/operatorNames | ![communication_operatorNames_2](./figures/communication_operatorNames_2.png) | ![communication_operatorNames_3](./figures/communication_operatorNames_3.png) 数据：![communication_operatorNames_4](./figures/communication_operatorNames_4.png) ![communication_operatorNames_5](./figures/communication_operatorNames_5.png)![communication_operatorNames_6](./figures/communication_operatorNames_6.png)|
| ![communication_operatorLists_1](./figures/communication_operatorLists_1.png) | communication/operatorLists | ![communication_operatorLists_2](./figures/communication_operatorLists_2.png) | ![communication_operatorLists_3](./figures/communication_operatorLists_3.png) 数据：![communication_operatorLists_4](./figures/communication_operatorLists_4.png) |
| ![communication_duration_list_1](./figures/communication_duration_list_1.png) | communication/duration/list | ![communication_duration_list_2](./figures/communication_duration_list_2.png)专家建议是由以上数据计算得到 | ![communication_duration_list_3](./figures/communication_duration_list_3.png) ![communication_duration_list_4](./figures/communication_duration_list_4.png)|
| ![communication_operatorDetails_1](./figures/communication_operatorDetails_1.png) | communication/operatorDetails | ![communication_operatorDetails_2](./figures/communication_operatorDetails_2.png) | ![communication_operatorDetails_3](./figures/communication_operatorDetails_3.png) |
| ![communication_distribution_1](./figures/communication_distribution_1.png) | communication/distribution | ![communication_distribution_2](./figures/communication_distribution_2.png) | ![communication_distribution_3](./figures/communication_distribution_3.png) |
| ![communication_bandwidth_1](./figures/communication_bandwidth_1.png) | communication/bandwidth | ![communication_bandwidth_2](./figures/communication_bandwidth_2.png) | ![communication_bandwidth_3](./figures/communication_bandwidth_3.png) |
