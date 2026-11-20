# Summary / Communication 公共接入协议

本文约定两条公共 PR 合入后的完整接入行为。公共请求与集群选择同步由当前 PR 提供，新旧版本切换和页面容器由独立的页面切换 PR 提供；两者均合入后再使用下述新版开关。

framework 管理两个集群页签的版本偏好、数据格式判断、集群选择和待执行定位。Summary 和 Communication 各自实现页面、Store、图表和业务接口，复用 lib 的公共请求、组件、主题与国际化。页面之间通过宿主协调上下文。

## 分批合入与启用

`REACT_APP_ENABLE_MODERN_CLUSTER` 是 **framework 的构建开关**，只有字符串 `true` 开启 Web 新版接入，默认关闭。VSCode 构建暂时保留旧版及原有 HTML 注入加载方式，无论开关值如何均隐藏版本切换按钮。关闭时两个页签沿用原有页面入口，工具栏隐藏版本切换按钮，DB/TEXT 的旧版集群对比继续可用；已有 localStorage 新版偏好不能绕过开关，也不会被关闭状态覆盖。

公共接入可以先单独合入，不需要安装或打包任何新版页面。两位开发者随后分别提交自己的页面和构建分发配置。在两个新版页面均已接入并完成联合验收后，由发布构建显式启用开关。开启后 DB 数据默认新版（尊重已保存的旧版偏好），TEXT 和无法确认格式的数据继续使用旧版。一次版本切换同时重建两个页签，不保留旧 iframe 的局部筛选与图表状态。

在 `modules` 目录运行 framework 联调：

```sh
REACT_APP_ENABLE_MODERN_CLUSTER=true pnpm --filter @insight/framework start
```

Summary 开发者同时设置 `REACT_APP_SUMMARY_MODERN_SRC` 为自己已启动的本地服务地址。端口由 Summary 项目确定；开发地址只允许 `http://localhost:<port>/...` 或 `http://127.0.0.1:<port>/...`。修改环境变量后需重启开发服务或重新构建。

| 页面 | 开发入口 | 生产入口 |
| --- | --- | --- |
| Summary | `REACT_APP_SUMMARY_MODERN_SRC` | 同一变量指定 `./plugins/` 下的实际入口，不允许目录穿越或外部地址 |
| Communication | `http://localhost:3012/` | `./plugins/Communication/index.html` |

页签名称和 iframe 的 `name` 固定为 `Summary`、`Communication`，不随语言或版本变化。Summary 需要在自身 PR 中完成 workspace 注册、开发启动、构建及插件产物分发。开启联调开关不会自动启动页面服务或生成插件文件；缺少入口或加载失败会显示不可用状态，可统一切回旧版。

## 消息封装与就绪顺序

消息沿用现有 JSON 字符串格式：`{ event, from, body }`。`from` 为当前 iframe 的 `window.name`；请求另有 `module`、`args` 和数字 `id`。使用 lib Connector 时由其填写发送方。自行实现消息桥接时，需检查消息来自 `window.parent` 及预期 origin，并使用宿主 origin 发送。

页面应先注册上下文、主题、语言、重置等监听，再发送统一就绪消息。每次 iframe 重新挂载都要执行一次：

```ts
connector.send({ event: 'clusterPage/ready', body: { version: 'modern' } });
```

宿主核验该消息的来源窗口和 origin 后，下发 `frame/loaded`、`clusterBaselineToggle`、`setTheme` 和 `switchLanguage`，再投递仍匹配当前工程/集群的待执行定位。加载阶段可能提前收到初始化消息，处理过程应可重复执行，不能依赖消息只到达一次。`communication/ready` 作为 Communication 的兼容事件保留；新增 Summary 使用 `clusterPage/ready`。

就绪表示页面的监听已注册并可接收上下文，不需要等待图表接口返回。当前容器的 15 秒等待只用于判断页面是否就绪；业务接口没有前端超时。

## 宿主下发的上下文

| 事件 | 页面需要处理的内容 |
| --- | --- |
| `frame/loaded`、`switchDirectory` | `body.selectedProjectName`、`body.selectedFileType`、`body.selectedFilePath`、`body.pageInfo.cluster` 与 `body.pageInfo.timeline` |
| `updateClusterPageInfo` | `body` 即集群上下文，包括 `clusterList` 和 `selectedClusterPath` |
| `updateSession` | 按现有会话协议读取 `body.clusterPageInfo`；按提供的字段更新，不用缺失字段覆盖已有状态 |
| `setTheme` | `body.isDark` |
| `switchLanguage` | `body.lang` |
| `clusterBaselineToggle` | `body.status`；新版页面不支持集群对比 |
| `remote/remove`、`remote/reset`、`module.reset` | 清理当前页面上下文、查询结果和在途等待，避免显示上一工程的数据 |

`clusterList` 的条目包含 `name`、`path`、`parsed`、`durationParsed`，可按页面需要区分集群解析和通信耗时解析状态。页面切换工程或集群时应使旧查询失效。解析完成后宿主会再次广播集群上下文。

## 集群选择与页面定位

用户在任一版本的集群下拉框选择数据后发送：

```ts
connector.send({
    event: 'clusterPage/selectCluster',
    body: { project: selectedProjectName, clusterPath: selectedClusterPath },
});
```

宿主校验工程名称和集群列表成员关系，保存选择并广播 `updateClusterPageInfo`。页面收到广播时更新本地状态，不要再次发送选择消息。新旧页面重建后从宿主恢复当前集群。

向其他页面定位沿用 `switchModule`：`body.switchTo` 指定页签（如 `timeline` 或 `communication`），`body.toModuleEvent` 指定目标事件，`body.params` 提供定位参数。宿主当前仅为新版 Communication 的 `locateCommunication`、`viewCommunicationDurationAnalysis` 缓冲就绪前的最新请求，并校验工程/集群范围；Summary 新增定位事件应在自身 SR 中明确并补充测试。

## 公共请求与页面清理

普通页面可复用 `ClientConnector` 与 `createRequest`。每个页面只创建一个 Connector，注册/卸载组件监听时使用对应的监听句柄。后端路由 `module` 根据接口所属模块确定，不能用页签名称代替后端模块名。

`createRequest` 的第四个参数支持 `{ signal, silent }`。作用域变化或卸载时取消 AbortController；这只结束前端等待，不停止后端计算，`AbortError` 不应显示为接口失败。默认不设置业务请求超时。

自行管理消息桥接的页面可使用 `RequestChannel`：将宿主请求回包交给 `receive`，在工程/集群重置及卸载时调用 `cancelAll`，通过 `createRequest(channel)` 复用错误处理。请求 ID 和晚到回包处理由公共层负责。宿主丢弃已被替换 iframe 的旧回包，避免污染新页面。

## 联合验收

- 开关缺省/关闭：DB 和 TEXT 均沿用旧入口，无切换按钮，无新版产物仍可使用。
- 开关开启：DB 两页共同切换，TEXT 无切换按钮；旧版集群对比继续可用，新版对比入口提示回旧版。
- 两页分别验证真实工程初始化、主题/语言、集群选择、解析更新、工程关闭与重置。
- 快速切换版本、慢接口回包、页面重试和 Timeline 定位不污染新上下文；卡级 Timeline 对比保持可用。
- 两个项目各自负责产物分发和真实数据验收。实际 VSCode 宿主、四个新通信接口及万卡性能需另行验证，Web 构建和单元测试不替代这些验收。
