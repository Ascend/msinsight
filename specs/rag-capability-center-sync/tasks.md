# RAG Capability Center 同步任务

## 1. 应用原子 RAG 补丁

- [x] 在 `msinsight2.0-dev` 工作区确认 tracked 文件干净并记录现有未跟踪产物。
- [x] 使用 `git cherry-pick -n b58a1246c` 应用补丁，不创建提交。
- [x] 排除 `docs/superpowers` 下 5 个旧 RAG 设计/计划文档。
- [x] 确认未删除或改写现有 `dist-server`、`main.lib` 和 `__pycache__`。

## 2. 合并依赖和构建交付链路

- [x] 合并 `modules/insight_web_agent/package.json`，同时保留 Capability Center 与 RAG 依赖、脚本。
- [x] 重新生成或正确合并 `modules/pnpm-lock.yaml`，保证 lockfile 与 package manifests 一致。
- [x] 合并 `build-server.mjs`，同时打包 capability center 配置/代码和 RAG runtime/CLI/smoke 入口。
- [x] 合并根构建、modules 构建、server 构建、SQLite、CMake 和 NSIS 安装器改动。
- [x] 保留 `.gitignore` 和 `.gitattributes` 对 RAG 二进制及生成物的规则。

## 3. 接入 RAG 核心

- [x] 同步 `rag-runtime`、RAG service、retriever、embedding、knowledge package、wire contract 和 CLI 文件。
- [x] 合并 RAG 配置解析及固定产品路径，保持 Windows x64 和 fail-open 语义。
- [x] 在 Host 启动时创建唯一 `RagService` 实例，并确保其生命周期独立于 Agent/Session 切换。

## 4. 注册 Capability

- [x] 定义 `rag_retrieve` 的名称、描述、输入 schema 和强制 output schema。
- [x] 新增 RAG capability 工厂，实现输入校验、错误映射和脱敏输出投影。
- [x] 在检索开始前和结果返回前检查 Capability 调用取消信号。
- [x] 将 `RagService` 注入 `createCapabilityCenter` 并注册 `rag_retrieve`。
- [x] 确认 `state.availableCapabilities` 包含 RAG 定义且 Agent reset 不误删全局能力目录。
- [x] 将 `rag_retrieve` 加入 Native 内建 definitions，设置 `requiresApproval: false`。
- [x] 保持 HTTP MCP Adapter 和 Native Internal API 复用统一 Registry，不新增旁路执行。

## 5. 删除自动 Prompt 检索

- [x] 从 `chatService` 参数和 Prompt 准备流程移除 `ragService.retrieve`。
- [x] 保留目标分支 `errorResult/errorCause`、Prompt 状态、取消和 Agent 切换语义。
- [x] 从 `contextAssembler` 移除 RAG 参数和 RAG hidden-context 清洗逻辑。
- [x] 更新 System Prompt，引导 Agent 按需调用 `rag_retrieve` 并引用 `sourceLabel`。

## 6. 合并和补充测试

- [x] 同步原 RAG 单元、集成、打包和安装器测试。
- [x] 增加 RAG capability 输入、输出、不可用、无命中、繁忙和脱敏测试。
- [x] 增加 HTTP MCP `tools/list/tools/call` RAG 测试。
- [x] 增加 Native definition 和内部 API 调用 RAG 测试。
- [x] 更新 Chat/Context 测试，断言 Prompt 不自动检索且 hidden context 不含 RAG provider。
- [x] 解决源分支旧测试与目标分支新错误响应/Capability Center 行为之间的冲突。

## 7. 验证

- [x] 运行 Capability Center、HTTP MCP、Native Tool 和 Prompt 回归测试。
- [x] 运行 RAG service、retriever、package、wire 和 runtime path 测试。
- [x] 运行 RAG packaged entry、packaged smoke、coverage checker 和 required smoke。
- [x] 运行 Python 安装器测试。
- [x] 运行 `git diff --check` 并扫描冲突标记。
- [x] 复核最终 diff 仅包含同步和 capability 集成所需改动，且 5 个旧设计文档未加入。
