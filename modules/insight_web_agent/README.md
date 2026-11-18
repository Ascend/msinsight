# Insight Bot 欢迎页与入口

工具栏中的 Insight Bot 入口使用 Logo 展示名称，悬停时显示提示并旋转一圈。提示框指向图标中心，在窗口右侧向左展开。

## 接口错误与语言

Agent 设置、会话、消息、权限和操作接口的错误提示跟随当前界面语言显示；切换中英文时，设置面板中已显示的后台错误也会更新，无需重复请求。

前端请求通过 `Accept-Language` 传递当前语言，Node 服务逐请求解析，未指定或不支持的语言回退到英文。中英文文案统一维护在 `src/i18n/apiErrors.json`；新增接口错误时应提供稳定的 `code`，配置校验明细还应携带字段和细分错误码。动态参数通过 `messageParams` 传递，不根据英文文本匹配翻译。

HTTP 状态码、错误码、`saved` 等业务字段保持不变。前端不拼接原始 `details`，避免直接显示未翻译的诊断信息；诊断数据仍保留在响应中供排查。未知错误使用当前语言的通用提示。

欢迎页使用动态点阵背景。点阵采用 demo 的 560 × 602px 画布，宽高分别不超过视口的 92%，以 Logo 和欢迎语整体居中。首次显示及新建对话时等待 200ms 后播放一次 1.5 秒的扩散波纹。鼠标附近的点会变亮、放大，移出后恢复。系统开启“减少动态效果”时展示静态点阵。

会话生成回复期间可以新建对话，原会话继续在后台执行。回复和排队消息保留在原会话中，可通过历史会话切回查看。首次发送尚未返回会话 ID 时也可以新建对话；取消操作会等待真实 ID 返回后发送到原会话。

## 实现与维护

- `src/components/WelcomePanel.tsx` 管理欢迎页布局及新建对话的重播标识。
- `src/components/WelcomeDots.tsx` 封装 Canvas、主题更新和组件卸载清理；`hitAreaRef` 指定响应鼠标的欢迎区，`replayKey` 变化时重播。
- `src/components/welcomeDotsEffect.ts` 管理点阵绘制、波纹、悬停、尺寸和可见性监听。点半径为 2px，中心间距为 10px，DPR 上限为 2；相关视觉参数集中在文件顶部。
- 动画直接更新 Canvas，静止后停止请求动画帧；隐藏或卸载时取消延迟任务和动画帧。主题切换只重绘，不重新生成点阵纹理。
- `src/hooks/useChatState.tsx` 为首次发送分配独立临时会话记录，收到真实 ID 后迁移对应队列、配置和提示；仅在该临时会话仍被选中时更新当前会话 ID。

## 高级会话导出 / Advanced session export

工具栏统一的导出图标打开二级菜单：“导出当前会话”保留单会话快捷操作，“导出历史会话”打开筛选与预览。历史筛选分为三个区域：已接入 ACP Agent 多选（默认全选，清空后不能查询）、更新时间范围、会话标题。时间支持近一天/近一周/近一个月（分别为滚动 1/7/30 × 24 小时），也可自由指定起止时间；自定义控件显示浏览器本地时区，提交为带时区的 ISO 时间。所有时间条件使用 `updatedAt`，包含起止两个端点。标题搜索不区分大小写，支持普通子串和有序非连续字符（`ds` 匹配 `deepseek`），空白分隔的多个词以 AND 组合；不搜索正文或隐藏上下文。预览遍历后端全部分页，可导出全部匹配或跨页勾选部分记录。

导出不会切换或中断当前聊天。正在生成的会话、能力不支持、读取失败、未知日期及容量限制会明确列出，可导出部分成功结果。修改筛选或快照过期后须重新查询。一个批量 JSON 文件包含 Agent 归属、会话 ID、归一化消息、逐会话 Markdown 和诊断；桌面保存至下载目录，浏览器/Jupyter 发起一次下载。原有单会话快捷导出不变。接口和数据格式见 [InsightAgentBridge](../../docs/zh/development_guide/design/InsightAgentBridge.md#462-高级批量导出)。

历史列表中可点击重命名图标编辑标题，Enter/保存提交，Escape/取消放弃。标题为 1–200 个字符，在当前 Host 的 `ACP_ROOT/session-titles.json` 按 `(agentId, sessionId)` 持久化；刷新、重启和 Agent 切换后仍有效，标题筛选及导出沿用新标题。保存失败不更新界面，自动生成的标题不会覆盖人工标题。改名是本应用的展示元数据，不假定第三方 ACP 支持改名，也不修改外部 Agent 原始会话或会话更新时间；不切换/重载对话。

**English:** The export icon opens a menu for the current conversation or historical conversations. History filters combine available ACP owners (all selected by default), an inclusive **updatedAt** range (rolling 1/7/30 days or custom local-zone endpoints serialized to ISO), and title-only, case-insensitive ordered-character matching (`ds` → `deepseek`; whitespace-separated tokens are ANDed). Body text is not searched. Preview scans every backend cursor page; export all snapshot matches or select across display pages without switching chat, Agent or drafts. Unsupported/busy histories and other omissions remain explicit. One uniquely named JSON retains ownership, normalized UI history and Markdown. History rows also offer persistent app-owned titles, scoped by owner/session under `ACP_ROOT/session-titles.json`, used in searches and exports; external ACP history is not rewritten. Requery after filter changes or snapshot expiry. This is not a lossless raw model-context backup.

## 验证

在 `modules/insight_web_agent` 目录运行：

```sh
CI=true node ../node_modules/react-scripts/bin/react-scripts.js test --watchAll=false --runInBand --runTestsByPath src/test/components/welcomeDotsEffect.test.ts src/test/components/WelcomeDots.test.tsx src/test/components/WelcomePanel.test.tsx src/test/components/ChatPanel.test.tsx
CI=true node ../node_modules/react-scripts/bin/react-scripts.js test --watchAll=false --runInBand --runTestsByPath src/test/hooks/useChatState.sessions.test.tsx
node --test server/test/services/sessionBatchExport.test.mjs server/test/services/sessionExportSource.test.mjs server/test/services/sessionRename.test.mjs server/test/native-agent/nativeSessionExport.test.mjs server/test/controllers/sessionBatchExportController.test.mjs
CI=true node ../node_modules/react-scripts/bin/react-scripts.js test --watchAll=false --runInBand --no-cache --runTestsByPath src/test/components/SessionBatchExportDialog.test.tsx src/test/components/SessionSidebar.test.tsx src/test/components/SessionHistoryPopover.test.tsx src/test/api.sessionExport.test.ts src/test/api.sessionRename.test.ts src/test/sessionExportDownload.test.ts
node ../node_modules/typescript/bin/tsc --noEmit -p tsconfig.json --jsx react-jsx
```

独立类型检查显式指定与 CRA 一致的 automatic JSX runtime：当前基础 `tsconfig.json` 的 `jsx: react` 与 `jsxImportSource` 组合会触发 TS5089；上述命令不改写源配置。构建或复用共享依赖时，缓存与构建输出应放在当前工作树，不能写入共享 `node_modules`。

手动检查深浅主题下的首次显示、新建对话、抽屉重新打开、鼠标经过与移出，以及窄窗口中的背景对齐和引导按钮点击。开启系统“减少动态效果”后确认波纹与悬停动画停止。

## Agent 可视化报告

用户可以要求 Agent「根据以上数据生成交互图表/HTML 报告」。`publish_report` 能力通过 Native Tool 和能力中心 MCP 提供，接收 `title`、可选的 `description` 和完整 `html`，返回 `artifactId`、`version` 等元数据。聊天显示报告卡片，生成完成后直接在主区域打开为与 Timeline 同级的标签页；点击历史报告卡片也直接打开主区域，不经过右侧预览。Agent 对话面板保持打开和原有宽度，可继续对话。

同一报告重复打开会复用已有标签，不同报告各自拥有标签。主区域支持版本切换；关闭当前报告标签返回之前的分析模块，关闭后台标签不影响当前页面，也不会删除报告。打开失败时在对话区域显示重试提示。独立运行聊天页面时，直接显示全宽报告，关闭报告后回到对话。标签页只保存当前界面状态，刷新应用后可从历史会话卡片重新打开。

主应用只接收经过校验的报告 ID、版本和标题，并自行构造可信报告查看器地址，不接收 Agent 指定的 URL 或 HTML。查看器使用独立的 `view=report` 页面，不启动聊天会话或订阅 Agent 执行事件。报告 iframe 不注册为业务模块，不接收工程数据广播；报告内容仍运行在隔离的内层 iframe 中。

新建报告时将 `artifactId`、`baseVersion` 设为 `null`。修改同一报告时，提交完整新 HTML，并传入先前的 `artifactId`、`baseVersion`。版本冲突会返回错误，不覆盖其他修改。报告保存在运行目录的 `.msinsight_artifacts/<artifactId>/` 中，每个版本为独立快照，历史消息保留引用。删除聊天不会删除报告文件；当前版本不提供自动清理和导出。

预览由主页面通过带鉴权的 `/api/artifacts/<artifactId>?version=<version>` 获取 JSON，再使用 `srcDoc` 加载到 `sandbox="allow-scripts"` 的 iframe。不会直接把 HTML 作为同源页面提供，也不向 iframe 传递 token。CSP 限制外部脚本、图片、样式、网络接口、子框架和表单；主应用消息入口校验来源窗口。报告使用内联 CSS/JS、SVG/Canvas 和数据快照，第一版不提供 CDN 图表库或业务 API 桥接。单个 HTML 上限 512 KiB，标题上限 120 字符，描述上限 500 字符。

可用示例提示词：

> 使用示例数据生成一份流水线运行周报，包含执行次数、成功率、每日趋势图和失败明细；提供工作日/周末筛选，并明确标注示例数据。请调用 publish_report 展示页面。

相关验证：

```sh
node --test server/test/services/artifactService.test.mjs server/test/native-agent/capabilityTools.test.mjs
CI=true node ../node_modules/react-scripts/bin/react-scripts.js test --watchAll=false --runInBand --runTestsByPath src/test/components/ReportWorkspace.test.tsx
```
