# 前端测试指南

首次运行前，请在 `modules/` 目录单独执行 `pnpm install --frozen-lockfile` 安装依赖。
测试命令不会安装依赖、启动后端或更新快照。

## 常用命令

在 `modules/` 目录执行以下命令：

| 命令 | 用途 |
| --- | --- |
| `pnpm test` | 单次运行整个工作区的 UT，默认使用两个 worker 进程 |
| `pnpm test --module memory` | 运行指定模块的测试，包含其 Node 环境测试 |
| `pnpm test --module timeline --module lib` | 运行多个指定模块的测试 |
| `pnpm test --module timeline --testPathPattern=unitFunc` | 按路径筛选一组测试 |
| `pnpm test:watch --module memory` | 启动交互式监听模式 |
| `pnpm test:ci` | 运行全部 UT，生成覆盖率、JUnit XML 和运行摘要 |
| `pnpm test:infra` | 检查模块发现、测试项目划分及覆盖率聚合逻辑 |
| `pnpm test:browser` | 在 Chromium 中运行现有的 24 个 tooltip 布局回归场景 |

在仓库根目录执行 `npm test` 或 `npm run test:ci`，也会转发到同一测试运行器。
各模块目录内的 `pnpm test` 同样使用该运行器。支持 `--runInBand`、`--maxWorkers=4`
等 Jest 参数，但这两个参数不能同时使用。

浏览器测试需要安装 Playwright 的 Chromium，也可以通过 `BROWSER_CHANNEL=msedge`
选择已安装的浏览器。浏览器测试独立于 UT，使用真实浏览器执行，不通过 jsdom 模拟。
产品端到端（E2E）测试仍位于 `e2e/` 目录。

## 配置与职责

`workspace.cjs` 根据 `pnpm-workspace.yaml` 发现模块，不扫描任意目录，也不另行维护一份模块清单。
新增测试会被自动发现。目前四个尚无 UT 基线的模块在该文件中显式列出，并在每次全量运行时提示。
新增模块未提供测试，或已有测试的模块丢失全部测试时，运行器会报错。
如果显式选择的模块全部没有测试，也会返回失败，避免误报成功。

`jest.config.cjs` 统一负责别名解析、测试环境、代码转换和报告生成。
`@insight/lib` 的别名来自该包实际的导出配置；`@/` 在各测试项目内解析。
各模块不应再引入独立的 Jest 配置。Jest 27 已声明为直接依赖。
为保持与生产代码一致的 JSX 转换行为，当前暂时保留 CRA 的 Babel 和静态资源转换器；本次不迁移到 Vite。

## 测试分层与边界

- `*.unit.test.ts`：使用 Node 环境，测试纯逻辑、通过依赖注入提供的服务和状态仓库（store）。
  此类测试不加载旧的全局 Session 初始化逻辑或 DOM 上下文提供器（provider）。
- 其他 `*.test.ts(x)` / `*.spec.ts(x)`：使用 jsdom 和 React Testing Library。
  测试应就近放在对应功能目录，现有目录结构无需整体搬迁。
- 浏览器测试：验证真实布局、Canvas/WebGL、worker、OPFS 和 iframe 集成。
  使用模拟实现的 UT 通过，并不能证明这些浏览器 API 在真实环境中可用。

功能测试优先使用真实 MobX 状态和用户交互。在请求、传输、worker 或存储边界提供模拟实现，
避免替换状态管理框架本身。纯计算应具有明确的输入和输出；依赖浏览器的对象应在应用边界创建，
并提供资源释放方法。

可参考以下示例：

- `memory/src/components/__tests__/StaticLineChart.test.tsx`：使用真实可观察 Session 和选择器，
  仅在请求边界提供模拟实现，并控制异步响应时机。
- `numa/src/utils/__tests__/overview/NumaStore.unit.test.ts`：注入数据加载器，验证请求顺序、
  去重、加载与错误状态以及重试行为。
- `timeline/src/utils/__test__/string.unit.test.ts`：不依赖 DOM 的工具函数测试。
- `timeline/src/components/ArgsData.test.tsx`：独立导入叶子组件，避免加载所在页面及其 Canvas 依赖。

`renderWithProviders` 为每次渲染创建独立的 i18n 实例，并使用真实主题 provider。
业务翻译和测试数据（fixture）由对应功能维护。`deferred` 用于控制 Promise 完成或失败的时机，
无需通过实际等待来协调异步操作。只有针对稳定边界的辅助逻辑才应提取为公共工具，且公共工具不得导入业务模块。

## 测试隔离

在 `beforeEach` 中创建可变测试数据和函数监视器（spy），不要在模块作用域共享这些对象。
Jest 会在每个测试前清空调用记录、重置 mock 实现并还原 spy，因此需要在 `beforeEach` 中重新设置 mock 实现。
每个测试应单独启用模拟定时器，并在结束后还原；DOM 测试的公共初始化逻辑也会恢复真实定时器。
测试结束时，应释放订阅、MobX reaction 和 worker，并还原被替换的全局属性。

为兼容现有 DOM 测试，暂时保留 timeline/cluster 的全局 Session 初始化逻辑。
新增测试应显式构造所需数据。未处理的 Promise 拒绝交由 Jest 检测；初始化脚本不得安装仅记录日志的监听器来处理这类错误。
本次迁移不全局屏蔽现有控制台警告，也不将其统一升级为测试失败。

## 测试报告与覆盖率

- `test-results/junit.xml`：测试结果，包含测试套件加载失败的情况。
- `test-results/run-summary.json`：实际执行的测试路径和结果数量统计。
- `coverage/coverage-final.json`、`coverage-summary.json`、`lcov.info` 和 `lcov-report/`：
  供工具读取的覆盖率数据及 HTML 报告。
- `coverage/module-summary.json`：按源码所属模块汇总的覆盖率，包含是否全工作区运行、测试是否通过的明确标记。

覆盖率统计包含未执行的 TS/TSX 源码，包括 `timeline/src/insight` 及尚无 UT 的模块。
仅排除测试文件、类型声明、测试数据与初始化脚本，以及专用的翻译和样式目录。
共享源码归入其所属模块，避免因多个模块引用而重复统计。
Node 和 DOM 测试项目的覆盖率数据先由 Jest 合并，再按模块聚合。

为兼容现有 CI 报告消费者，在仓库根目录执行 `npm run test:ci` 后，再执行 `python3 build/test_unite.py`。
该脚本将 JSON 转换为 `llt_cov/total_cov.csv` 和 `junit/junit.json`，不再解析 HTML；
对于仅运行部分模块或测试失败的结果，脚本会拒绝导出。
`build/llt_test.sh` 依次执行锁定依赖安装、测试基础设施检查、全量覆盖率测试及报告导出。
同一份代码检出目录内，一次只运行一个生成报告的命令，避免报告相互覆盖。

本阶段先建立真实、完整的覆盖率基线，暂不设定统一的全局百分比门槛。
后续将评估分模块和增量覆盖率门槛，继续迁移测试数据与初始化方式，并增加独立的 WebGL/worker 浏览器测试项目。
在受影响测试筛选机制能够正确处理共享库和配置变更前，PR 验证继续执行全量 UT。
