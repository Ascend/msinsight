# 源码（Source）<a id="source"></a>

## 功能说明

源码（Source）界面用于展示算子指令热点图，支持查看算子源码与指令集的映射关系和耗时情况，在昇腾Ascend C算子开发过程中，使能开发者进行性能分析。

## 界面介绍

源码（Source）界面包含筛选栏（区域一）、源文件代码属性表（区域二）和指令表（区域三）三个部分组成，如[**图 1**  源码界面](#源码界面)所示。

**图 1**  源码界面<a id="源码界面"></a>
![](../figures/operator_tuning/source_code_interface_1.png "源码界面")

- 区域一：筛选栏，可通过计算核（Core）和源码（Source）进行筛选需要查看的内容。
- 区域二：源文件代码属性表，查看各行代码和其相应的执行时长和次数，表中字段解释如[**表 1**  源文件代码属性表字段说明](#源文件代码属性表字段说明)所示。

    **表 1**  源文件代码属性表字段说明<a id="源文件代码属性表字段说明"></a>

    |中文字段|英文字段|说明|示例|
    |--|--|--|--|
    |#|#|代码行号。|100|
    |源码|Source|源文件代码。|-|
    |执行指令数|Instructions Executed|该行代码在每个Core上执行的指令数量。|100|
    |时钟周期|Cycles|该行代码在每个Core上执行消耗的Cycles（时钟周期）。|100|
    |通用寄存器数|GPR Count|该行代码在每个Core上执行时使用的通用寄存器次数。仅当使用msopprof simulator采集的数据支持显示该参数。|10|
    |L2 Cache命中率|L2 Cache Hit Rate|该行代码在所有Core上执行的L2 Cache命中率。仅当使用msopprof采集的数据支持显示该参数。|100%|
    |处理数据量(Bytes)|Process Bytes|该行代码在每个Core上执行处理的数据量之和，单位Byte。|2048|

- 区域三：指令表，查看指令记录，包括地址、内容、数量、次数等，表中字段解释如[**表 2**  指令表字段说明](#指令表字段说明)所示。

    **表 2**  指令表字段说明<a id="指令表字段说明"></a>

    |中文字段|英文字段|说明|示例|
    |--|--|--|--|
    |#|#|序号。|100|
    |地址|Address|指令所处的偏移地址。|0x1122a828|
    |指令队列|Pipe|指令所处的Pipe（指令队列）。|MTE2|
    |源码|Source|指令内容。|BAR PIPE:ALL|
    |执行指令数|Instructions Executed|该行指令在每个Core上执行的指令数量。|100|
    |通用寄存器数|GPR Count|该行指令在每个Core上执行时使用的通用寄存器次数。仅当使用msopprof simulator采集的数据支持显示该参数。|10|
    |寄存器状态|GPR Status|寄存器依赖信息，以图形化展示。由多列带箭头的直线组成，每一列代表一个寄存器。其中左实心箭头表示为写，右空心箭头为读，竖线表示当前寄存器仍在使用。当鼠标悬停在寄存器上时，会显示寄存器信息。<br> 仅Ascend 950PR&950DT系列产品导出的数据支持显示该参数。|-|
    |时钟周期|Cycles|该行指令在每个Core上执行消耗的Cycles（时钟周期）。|100|
    |L2 Cache命中率|L2 Cache Hit Rate|该行指令在所有Core上执行的L2 Cache命中率。仅当使用msopprof采集的数据支持显示该参数。|72%|
    |处理数据量(Bytes)|Process Bytes|该行指令在每个Core上执行处理的数据量，单位Byte。|2048|
    |UB单元读冲突|UB Read Conflict|Vector计算类指令在UB Bank上读的冲突情况。仅当使用msopprof simulator采集的数据支持显示该参数。|1|
    |UB单元写冲突|UB Write Conflict|Vector计算类指令在UB Bank上写的冲突情况。仅当使用msopprof simulator采集的数据支持显示该参数。|0|
    |Vector计算单元利用率百分比|Vector Utilization Percentage|Vector计算单元的利用率，单位%。仅当使用msopprof simulator采集的数据支持显示该参数。|35.29|

## 使用说明

**源码搜索功能**

在源文件代码属性表区域，使用键盘上的Ctrl + F键可调出搜索框，在搜索框中输入所需关键字，可按需选择大小写匹配等功能，按Enter回车键查询，会高亮显示匹配的关键字，如[**图 2**  搜索源码](#搜索源码)所示，搜索框中图标功能如[**表 3**  图标功能说明](#图标功能说明)所示。

> [!NOTE]
>
> macOS系统中，需使用键盘上的Command + F键调出搜索框。

**图 2**  搜索源码<a id="搜索源码"></a>
![](../figures/operator_tuning/search_source_code_1.png "搜索源码")

**表 3**  图标功能说明<a id="图标功能说明"></a>

|图标|功能说明|
|--|--|
|![](../figures/operator_tuning/zh-cn_image_0000002500200386.png)|大小写匹配。选定该图标后，可匹配搜索到所输入的关键字，区分大小写。|
|![](../figures/operator_tuning/zh-cn_image_0000002532040315.png)|全词匹配。选定该图标后，可搜索到完整匹配的关键字。|
|![](../figures/operator_tuning/zh-cn_image_0000002531920303.png)|向上切换搜索结果。|
|![](../figures/operator_tuning/zh-cn_image_0000002500040430.png)|向下切换搜索结果。|
|![](../figures/operator_tuning/zh-cn_image_0000002500200416.png)|框选源码区域。选定该图标后，可使用鼠标左键选定源码区域，则可在选定区域内进行搜索。|
|![](../figures/operator_tuning/zh-cn_image_0000002532040345.png)|关闭搜索框，退出搜索功能。也可以按键盘上的Esc键退出。|

**查看关联指令**

单击源文件代码属性表中任意一行代码，则在指令表将高亮显示此行代码相关指令。指令表上方会显示当前选中代码的行数（Line）和此行代码关联的指令总数（Related Instructions Count），如[**图 3**  查看关联指令](#查看关联指令)所示，此行代码所处位置是第10行，且关联的指令总数为112个。

**图 3**  查看关联指令<a id="查看关联指令"></a>
![](../figures/operator_tuning/view_associated_instructions_1.png "查看关联指令")

勾选指令表上的“仅查看关联指令”则会进行过滤，只显示此行代码关联的指令，如[**图 4**  过滤关联指令](#过滤关联指令)所示。

**图 4**  过滤关联指令<a id="过滤关联指令"></a>
![](../figures/operator_tuning/filter_associated_instructions_1.png "过滤关联指令")

**查看关联代码**

单击指令表中任意一行，则在源文件代码属性表中高亮显示所关联的代码，并且在指令表中高亮显示此行代码所有关联指令，如[**图 5**  查看关联代码](#查看关联代码)所示。

**图 5**  查看关联代码<a id="查看关联代码"></a>
![](../figures/operator_tuning/view_associated_code_1.png "查看关联代码")

> [!NOTE]
>
> 如果有多行关联代码时，只会高亮显示最上层代码。

**筛选指令表**

在指令表中，单击表头每个字段后方的![](../figures/operator_tuning/zh-cn_image_0000002532040347.png)可以筛选需要查看的内容，如[**图 6**  筛选指令表](#筛选指令表)所示。

**图 6**  筛选指令表<a id="筛选指令表"></a>
![](../figures/operator_tuning/filter_instruction_table_1.png "筛选指令表")
