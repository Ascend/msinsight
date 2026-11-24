# 新版 Communication

本项目独立实现新版通信页。旧版仍在 `../cluster`；新版不导入旧版业务组件、Session 或 RequestUtils。需求依据 [RFC #733](https://gitcode.com/Ascend/msinsight/issues/733)。

页面依赖的 [公共请求与上下文 PR #1444](https://gitcode.com/Ascend/msinsight/merge_requests/1444) 和 [版本切换 PR #1450](https://gitcode.com/Ascend/msinsight/merge_requests/1450) 已合入 master。Summary 与 Communication 共用的版本切换、入口和消息约定见 [集群页面接入协议](../framework/src/cluster/README.md)。

两版均跟随 framework 的语言设置，通信文案统一维护在 `../lib/src/i18n/communication/{zh,en}.json`。在组件中使用 `useTranslation('communication')`：含义相同的文案直接复用原键，如 `t('searchCriteria.CommunicationGroup')`；只有新版使用或需要不同措辞的文案放在 `modern` 下，如 `t('modern.rangeRatio')`。旧版词条保留原路径；版本切换由 framework 决定，无需在组件中按版本选择词典。

## 本地运行

在 `modules` 执行：

```sh
pnpm install --frozen-lockfile
pnpm --filter @insight/communication start
pnpm --filter @insight/framework start
```

上面的两个开发服务需要分别运行。真实模式页面由 framework 的 Communication iframe 承载，端口 3012。需要运行中的宿主、后端和已导入数据。Web 端首次打开 DB 数据时显示旧版，可通过工具栏切换到新版，之后使用上次手动选择的版本。TEXT、未知格式及 VSCode 保持旧版。

远程后端联调：在 `../framework/.env.development.local` 设置 `REACT_APP_BACKEND_HOST=<后端 IP 或主机名>`（不含协议或端口），重启 framework 后访问 `http://localhost:5174/?port=<后端端口>`；不传 port 时使用 9000。此本地文件不提交 Git，配置仅作用于 Web 开发的直接连接，生产、VSCode、JupyterLab 和代理路径沿用原连接方式。Communication 继续通过 framework 转发请求，导入路径使用后端机器上的数据路径。同一后端实例只支持一个客户端连接。

独立交互预览：

```sh
pnpm --filter @insight/communication start:mock
```

仅独立打开页面、`NODE_ENV=development` 且 `REACT_APP_COMMUNICATION_MOCK=true` 时启用 wire fixture。嵌入 framework 时始终走真实接口，避免真实工程名称与示例图表数据混用。生产构建不加载 fixture；任何接口异常均显示错误及重试，不能使用 Mock 兜底。

```sh
pnpm --filter @insight/communication build
node testing/run.cjs --module communication
```

分发目录为 `plugins/Communication/index.html`，已接入 `sync.mjs` 和 `build/build.py`。

## 实现边界

- `store/CommunicationStore.ts`：每个页面实例独立创建的 MobX Store，管理宿主上下文、公共迭代/通信域、定位意图和重置规则；通过 Context 提供，各区块用 observer 订阅所需字段。首次收到完整集群上下文、主题和语言之前，仅显示初始化加载状态，避免把尚未同步的空路径误显示为“请选择集群数据”。
- `bridge/HostBridge.ts`：校验宿主消息来源，通过 Store 方法更新状态。业务请求复用 lib 的 `createRequest` 和 `RequestChannel`，不设前端超时；使用 `signal` 结束单次等待，工程/集群重置和页面卸载时取消全部等待。取消不会终止后端计算。请求沿用数字 ID 协议，新页面不从零重新编号，宿主也不会把旧 iframe 的在途响应转发给同名替换页面。
- `api/index.ts`：统一封装页面所有接口，每个方法注明用途和关键参数语义。负责参数组装、响应转换与分页处理；四个新统计接口使用 2026-11-19 后端契约，旧接口参数保持原协议。
- 接口和宿主消息通过 `@insight/lib/utils/validation` 校验响应字段，通过 `@insight/lib/connection` 检查请求取消。迭代和 Rank ID 的兼容转换保留在 `api/index.ts`。
- `state/useQuery.ts`：键控查询和取消；晚到成功/失败都不能覆盖新筛选结果。
- `app/App.tsx` 管理顶部集群和迭代筛选。`app/Analysis.tsx` 管理通信域筛选和分析区块；作用域变化卸载域内图表及抽屉，底部专家建议保持挂载并管理集群诊断。
- `sections/`：按完整分析区块管理请求、加载状态和局部筛选。图表数据继续由 useQuery 管理，不进入 MobX；Store 中的宿主列表和通信域描述使用浅引用。通信耗时区块负责尾对齐及 Timeline 定位，矩阵区块管理独立算子条件，统计表区块管理带宽抽屉及主页面交互恢复。
- `components/Selector.tsx`：基于 lib Select 统一筛选条件的展示。
- `features/operatorTimeline/`：缩略图的组件、WebGL2 渲染器、数据索引、ECharts dataZoom、tooltip、导航和测试集中维护；默认展示全部 rank，按视口筛选算子，WebGL 不可用时回退 ECharts custom series。
- `features/advice/`：集群诊断卡片；复用 lib ResizeTable、Tooltip 与原有中英文诊断文案，完整展示诊断数据及解决建议。
- `features`：图表渲染、分页表格、模态抽屉及快慢卡；矩阵使用稀疏链路与坐标缩放，卡级表使用分页。

所有图表禁用对比。顶部在集群后选择迭代 ID，图 01～06 共用该迭代。图 01 展示该迭代下所有通信域的极差比和带宽，图 02～06 还受通信域筛选影响。图 07 仅使用 `communication/advisor`，沿用集群范围，四类诊断直接展示在“专家建议”下方，仅切换集群时重新加载，不受迭代/通信域/算子筛选影响。诊断请求失败时保留重试，成功返回空诊断时隐藏整个底部建议模块；新版不再请求或展示 `communication/duration/list` 的耗时、带宽统计建议。图 02 和图 04 的算子条件互不影响。图 06 从图 05 行操作打开。抽屉打开时对整个 Communication main 设置 inert，并由 Drawer 管理焦点与滚动恢复。

单位：旧缩略图 `startTime/duration` 为 ns，显示时除以 1e6；旧快慢卡数据为 us，显示时除以 1000；旧矩阵和传输表的带宽 GB/s、大小 MB、时间 ms 沿用协议。

## 四个新统计接口（2026-11-19 后端契约）

继续通过 framework 现有 WebSocket 和 lib 请求层调用，module 为 `communication`。成功响应已由公共层解包为 body，失败继续抛出带 code 的错误；页面不重复处理外层信封。

| command | 参数（另带 clusterPath） | body |
| --- | --- | --- |
| communication/group/rangeRatio | iterationId | `{ items: [{ groupName, groupId, rangeRatio }] }` |
| communication/group/bandwidth | iterationId, transportType | `{ items: [{ groupName, groupId, bandwidth }] }` |
| communication/operator/fluctuation | iterationId, groupName | `{ items: [{ opName, rangeTime }] }` |
| communication/rank/statistics | iterationId, groupName, currentPage, pageSize, orderBy, order；按带宽排序另带 bandType | `{ items: [{ rankId, bandwidths: [{ bandType, bandwidth }] }], total, currentPage, pageSize }` |

- `iterationId` 保留前导零。通信域选项按「groupId · 并行策略」显示，取值分别来自 `groupId.compare` 和 `parallelStrategy`，悬停时展示 `group` 中的 Rank 列表。域内统计请求的 `groupName` 取自 `groupIdHash.compare`，始终按字符串传递。
- 极差比是最大耗时 / 最小耗时，无量纲；不是差值或百分比。域聚合带宽是总传输量 / 总传输时间；算子极差时长是最大耗时 - 最小耗时。前三图保留服务端顺序，不自行按指标值排名。
- 卡级表的五个带宽列表头标注 GB，数值固定显示四位小数。极差比、域带宽和算子波动图按接口原值绘制，纵轴标题在轴线正上方居中，隐藏图例。提示保留六位小数，极差比无单位，带宽和耗时分别在数值后标注 GB/s、ms。指标名称由前端国际化词条提供。后端未返回异常阈值或标记，不推断红色高亮。
- 极差比和域带宽的 `null` 保留为空值，真实 `0` 保留；卡级缺失带宽显示 `-`，不补 0。适配层保留全部带宽类型，表格仍只显示设计图中的 RDMA、SDMA、HCCS、PCIE、SIO 五列。
- 卡级表使用服务端分页（默认每页 20 卡）和全量排序，`orderBy` 仅为 `rankId` 或 `bandwidth`；排序带宽列通过 `bandType` 指定。切换集群/公共迭代/通信域重建表格，排序或每页大小变化回到第 1 页，取消排序恢复 rankId 升序。总数以 `total` 为准，空页仍保留分页入口。
- 图 05 的响应只有带宽、没有通信耗时。打开抽屉后直接按当前集群、迭代、通信域和 rankId 请求旧 `distribution`、`bandwidth` 接口；后端不读取单卡 dbPath，不需要额外查询 `operatorLists`，也不依赖通信耗时数据解析完成。
- 3108 显示“当前数据不支持该通信统计能力”，3109 显示“通信统计数据异常”，其他失败保留通用错误和重试；不将失败当作空数据。局部请求继续通过 useQuery 取消等待并隔离迟到响应，不设置前端超时。
- 文档建议的点击域下钻未启用，沿用已确认的“没有新的联动”。

## framework 与 Summary 协作

- Web 中，framework 依据当前所选集群的 `DB`/`DB_CLUSTER` 类型开放新版。`TEXT_CLUSTER` 或类型未确认时，两个页签使用旧版并隐藏 ToolBox 切换入口，原有 DB 版本偏好会保留。
- `framework/src/cluster/viewVersion.ts` 保存两页共用的版本偏好 `insight.cluster.viewVersion`，首次打开默认使用旧版。切换时同时重建两个页签。工具栏首次点击立即切换，之后 500 毫秒内的重复点击会被忽略。
- 两个页面的默认入口在 `framework/src/moduleConfig.ts` 中注册。`REACT_APP_SUMMARY_MODERN_SRC` 可以单独覆盖 Summary 入口。页面加载失败时可统一切回旧版。
- 新页面挂载后向父窗口发送 JSON `{event:'clusterPage/ready',body:{version:'modern'}}`；Communication 兼容事件名为 `communication/ready`。宿主按消息源验证就绪后补发 `frame/loaded`、主题、语言和集群对比状态。
- 页面选择集群时发送 `{event:'clusterPage/selectCluster',body:{project,clusterPath}}`。宿主核验当前工程及集群列表后同步上下文，新旧页面切换保留这个选择。旧版 ClusterSelect 已补充这一通知，不改动其图表和请求实现。
- `navigation.ts` 只缓冲每页最后一个未就绪定位，并按工程/集群核验后投递；版本切换清空缓冲。新版接收原 `locateCommunication`、`viewCommunicationDurationAnalysis`。
- 旧版集群对比及设置基线请求进行期间不能切新版。新版保留右键菜单中的原对比入口，点击后警告提示“新版暂不支持集群对比，请切回旧版进行对比”，不修改基线或发起对比请求；比较操作入口及请求转发层继续阻止集群对比；Timeline 的 rank 对比继续使用原实现。
- VSCode 的旧版内联容器分支保留。新版标准 iframe 的插件资源 URL/内联注入映射仍须与实际 VSCode 宿主联合验证，不能据标准 Web 构建通过宣称该环境已验收。

## 尚未完成的联合验收

1. 四个新接口已按文档完成前端适配；仍需使用实际后端和新统计表联合验收，并核对域带宽原值与 GB/s 的口径。
2. 图 07 已接入旧 `communication/advisor` 的带宽抢占、字节对齐、通信重传与小包分析，按已确认的集群级语义展示。若后续需要域级诊断，仍需与后端确认范围能力，当前不做前端推断过滤。
3. 使用真实 DB 工程验收 Summary 与 Communication 的双页切换。
4. 其他数据集上的兼容性及万卡性能/资源释放验收。合成 10,000 行绘制测试只检查全量概览、缩放后的有界绘制及末卡定位，不代表真实万卡性能达标。

## 公共组件与旧功能基准

页面布局、筛选栏、专家建议和带宽抽屉使用 Emotion，颜色与字号取自 lib theme。两处专家建议共用 `components/AdvicePanel.ts`，筛选标签共用 `components/filters.ts`。原生 CSS 仅保留在缩略图和矩阵目录，用于画布焦点效果及图表生成的 tooltip。

优先使用 `@insight/lib` 的 Button、Select、Checkbox、InputNumber、Tooltip、Dropdown、CollapsiblePanel、ResizeTable、MIChart，以及 SharedConfigProvider、GlobalStyles 和统一 theme。仅 lib 未提供的 Alert、Drawer、Slider 直接使用 antd，并继承系统样式；不按设计稿另建图表主题。

旧图表按旧接口与交互语义重构：缩略图默认全卡、双轴缩放/平移、焦点范围内的快捷键、Ctrl/Shift 滚轮、尾对齐横向视口保留、原算子颜色映射、右键跳转；快慢卡保留专家建议与卡级汇总主表，按新版要求移除“可能的慢卡列表”标题、算子子表及其高亮联动；矩阵保留域内空卡坐标、卡内开关、数值区间、固定链路颜色、重复链路明细和缩放标签；通信包保留缺失/空数据区别与 SDMA 子链路层级。例外仅为已确认的新布局、筛选作用域、新版不支持对比及图 05 列范围。

## 真实接口联调记录（2026-11-17）

使用本地 `level2` DB 数据集、迭代 13、4 卡 default_group，在 framework 中关闭 Mock 验证了缩略图、矩阵、慢卡分析及当时的底部耗时建议（后按需求移除）。缩略图与矩阵算子筛选可独立刷新真实数据，缩略图尾对齐及右键跳转至 Timeline 对应算子也已验证。带宽统计及包分布的旧协议要求 `rankId` 为字符串，已修正数字转换并增加回归测试。当前样本返回 HCCS/SDMA 统计与 HCCS 包分布，其他传输类型按无数据处理。

当时四个新接口尚未实现，对应区块显示请求失败和重试，不影响旧图表。卡级新统计表暂无数据，因此抽屉的整条 UI 入口还需在新接口就绪后联调；本轮已直接核验其依赖的两个旧接口。此小规模数据集不代表万卡性能验收。

2026-11-18 补充：同一真实 DB 数据集已核验集群级四类诊断及全部旧版解决建议；小包分析表显示 SDMA、RDMA 两行，其他诊断按接口结果展示问题算子或无问题提示。组件测试覆盖公共筛选不重复请求集群诊断、切集群刷新、失败重试、空数据隐藏、中英文及旧请求晚到保护。

2026-11-18 需求调整：移除底部耗时、带宽统计建议及对应接口适配、组件、类型与专用文案；保留四类集群级诊断，旧版页面功能不变。

2026-11-20 补充：按后端新增四接口文档接入正式字段、字符串域标识和卡级服务端分页排序；开发 fixture 同步为正式 wire 结构。此轮为前端契约与交互验证，尚未验证实际后端返回。
