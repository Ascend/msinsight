# RAG Capability Center 验收清单

## 范围

- [x] `b58a1246c` 中所有非 `docs/superpowers` RAG 交付文件已同步。
- [x] 5 个旧 RAG 设计/计划文档未同步。
- [x] 目标工作区原有未跟踪构建产物未被修改或删除。
- [x] 未创建 Git commit。

## Capability 契约

- [x] Tool 名称固定为 `rag_retrieve`。
- [x] 输入仅允许非空 `query`，最大 8000 字符。
- [x] Tool 声明与实际输出一致的强制 output schema。
- [x] 输出状态仅为 `ok`、`no_match` 或 `unavailable`。
- [x] `ok` 至少有一个 source；`no_match` 和 `unavailable` 的 sources 为空。
- [x] 命中结果包含可引用的 `sourceLabel` 和知识正文。
- [x] 输出不包含 score、内部 ID、路径、模型、Package SHA、队列或异常栈。
- [x] `RAG_BUSY` 标记为 retryable。

## 注册与调用

- [x] Capability Registry 列出 `rag_retrieve`。
- [x] HTTP MCP Agent 可发现和调用 `rag_retrieve`。
- [x] Native Agent 可发现和调用 `rag_retrieve`。
- [x] Native RAG 调用不触发 Host 二次审批。
- [x] 两种 Adapter 最终调用同一个 Capability Center 和 `RagService` 实例。

## Prompt 行为

- [x] 普通 Prompt 不自动调用 RAG。
- [x] hidden context 不包含 RAG provider。
- [x] System Prompt 指导 Agent 按需检索、引用来源并防止知识文本注入。
- [x] `no_match/unavailable` 时 Agent 被明确要求不杜撰文档结论。
- [x] structured/page context、技能、图片、取消和错误响应无回归。

## 生命周期与降级

- [x] RAG 在 Host 启动时加载一次。
- [x] Agent 或 Session 切换不重复加载 RAG。
- [x] RAG disabled/unsupported/load-failed 不阻断普通聊天。
- [x] Host shutdown 和现有 MCP/ACP 清理行为无回归（Windows 不支持向子进程投递 SIGTERM，该平台场景由 listen-failure 和 Adapter disconnect 测试覆盖）。

## 构建与测试

- [x] package manifest 与 pnpm lockfile 一致。
- [x] RAG runtime、CLI、知识包和必需 smoke 文件进入 dist-server。
- [x] Capability Center 和 RAG 单元测试通过。
- [x] HTTP MCP 和 Native Adapter 集成测试通过。
- [x] RAG packaged 与 smoke 测试通过。
- [x] Python 安装器测试通过。
- [x] `git diff --check` 通过且不存在冲突标记。
