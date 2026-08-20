# Native Primary Agent 与 Skill 架构设计

> 状态：设计已确认，按本文实施。本文只适用于 `msinsight-native`；OpenCode、Claude Code、Codex 等外部 ACP Runtime 保持现有行为。

## 1. 背景

`msinsight-native` 当前有固定系统提示词和 `msinsight` Native Tool。Web Agent 已能扫描 `skills/<name>/SKILL.md`，但只在用户输入 `/skill-name` 时把 Skill 全文注入当前 Prompt，不具备 Agent 自主发现和按需加载能力。

PyTorch Snapshot 分析包采用 OpenCode 的目录模型：

```text
agents/*.md
skills/<name>/SKILL.md
```

其中 Agent 定义角色与工作方式，Skill 提供按需加载的领域知识，真正的执行能力由 Runtime Tool 提供。要支持后续更多专项分析能力，Native Runtime 应直接兼容这一模型，而不是为每个领域新增 ACP Agent 进程或私有 manifest。

## 2. 目标

1. 直接发现 OpenCode 兼容的 `agents/*.md` 与 `skills/<name>/SKILL.md`；
2. 在 `msinsight-native` 内提供可选择的 Primary Agent；
3. Primary Agent 与 Native Session 绑定，首次 Prompt 后不可切换；
4. Agent 通过 AI SDK Tool Loop 中的受控 `skill` Tool 按需加载全局 Skill；
5. 提供受控 `bash` Tool，并实施产品硬策略、Agent 规则和用户审批；
6. 保持 ACP 为薄协议层，Agent/Skill Registry 与权限求值留在 Native Runtime；
7. 生产环境只加载审核过的内置资源，开发环境可加载显式配置目录；
8. 保持旧 Native Session 可恢复。

## 3. 非目标

首版不包含：

- `@agent-name` Subagent 委派；
- Runtime 内 `Task` 子会话；
- 在同一 Session 中热切换 Primary Agent；
- 生产 UI 导入任意本地目录；
- Agent/Skill bundle manifest、签名仓库或在线安装；
- Agent 级 executable、Python package 依赖声明和启动预检；
- Skill 激活时修改模型、环境变量、Tool Policy、System Prompt 或 Hooks；
- Skill `!` 反引号内联命令；
- 后台、交互式或跨 Session Bash。

## 4. 核心术语

### 4.1 Agent Runtime

Web Agent 当前连接的 ACP 后端，例如：

```text
MSInsight Native
OpenCode
Claude Code
Codex
```

切换 Agent Runtime 会替换 ACP 子进程，不属于本文新增能力。

### 4.2 Primary Agent

`msinsight-native` 内部的主分析角色，由 `agents/<id>.md` 定义。Primary Agent 决定：

- 追加到产品基础 System Prompt 后的领域指令；
- Bash 命令的 Agent 级 `allow / ask / deny` 规则；
- 前端显示名称、描述和来源。

### 4.3 Skill

`skills/<name>/SKILL.md` 定义的全局领域知识。启动时只发现元数据；模型调用 AI SDK Tool Loop 中的受控 `skill` adapter 时才加载正文和资源清单。

### 4.4 Agent bundle

便于复制和分发 Agent、Skills 的普通目录，不是 Runtime manifest。安装时将其中的 `agents/` 与 `skills/` 放入受信发现根目录。

## 5. 总体拓扑

```text
Insight Web Agent
    │
    │ ACP session/new
    │ ACP session/set_config_option(primaryAgent)
    ▼
msinsight-native
    ├── AgentRegistry
    │     ├── built-in agents/
    │     └── development agents/（可选）
    ├── Native SkillRegistry
    │     ├── built-in skills/
    │     └── development skills/（可选）
    ├── Native Session
    │     └── primaryAgentId（不可变）
    └── AI SDK Tool Loop
          ├── 产品基础 System Prompt
          ├── Primary Agent Markdown 正文
          ├── msinsight
          ├── skill
          └── Bash（受控前台执行）
```

Agent Registry、OpenCode frontmatter 解析和权限求值只存在于 `msinsight-native`。Web Server 与 React 前端只消费 ACP `configOptions`，不维护第二套 Agent 解析器。

## 6. 资源目录与来源

### 6.1 内置资源

```text
agents/
├── general.md
└── memory-tuning-assistant.md

skills/
├── pt-snap-setup/
│   └── SKILL.md
└── pt-snap-fragmentation-forensics/
    └── SKILL.md
```

`general.md` 是必需资源。当前 Native 通用分析行为迁移到该文件。缺失或无效时属于产品资源错误。Agent 文件名是稳定 ID，frontmatter 中可选的 `name` 是下拉框显示名；新增 `agents/*.md` 后会由 Registry 自动发现，无需修改代码中的 Agent 列表。

### 6.2 开发资源

开发环境可通过显式配置增加一个或多个发现根目录。每个根目录仍使用：

```text
<root>/agents/*.md
<root>/skills/<name>/SKILL.md
```

优先级为：

```text
显式开发目录 > 内置目录
```

重名覆盖必须生成诊断，并在前端元数据中标明开发来源。生产环境不读取开发目录，也不允许覆盖内置资源。

### 6.3 路径安全

Registry 必须：

- 使用规范化绝对路径；
- 拒绝最终路径越出声明 source root 的 Agent、Skill 和资源；
- 只接受普通文件和目录；
- 不在发现阶段执行文件内容；
- 单个无效资源只产生诊断，不能隐藏其他有效资源；
- `general.md` 无效时阻止 Native Runtime 提供可执行会话。

## 7. Agent 文件格式

Agent ID 取 Markdown 文件名，不含 `.md`。首版兼容以下核心字段：

```yaml
---
description: 分析 PyTorch Snapshot 内存问题
mode: primary
permission:
  bash:
    "python -V": allow
    "pt-snap --help": allow
    "pt-snap query *": allow
    "*": ask
---

这里是 Primary Agent 指令。
```

### 7.1 支持字段

| 字段 | 规则 |
| --- | --- |
| `description` | 必填非空字符串，用于选择器和模型描述 |
| `mode` | `primary`、`subagent`、`all`；缺省为 `all` |
| `permission.bash` | 命令模式到 `allow / ask / deny` 的映射 |

`mode: primary` 与 `mode: all` 出现在首版 Primary Agent 选择器；`mode: subagent` 只解析并产生“当前 Runtime 不支持”的诊断。

其他 OpenCode 字段可以被解析器识别为 unsupported diagnostic，但不得改变模型、温度、Tool、Provider 或产品权限。

### 7.2 System Prompt 合并

有效 System Prompt 固定为：

```text
产品基础 System Prompt

Host System Prompt

Primary Agent Markdown 正文
```

Host System Prompt 优先来自 Web Server 的 `insight-system-prompt://project` ACP resource；resource 缺失时兼容读取 Agent workspace 中的 `AGENTS.md`/`CLAUDE.md`。Host 与 Agent 指令都只能追加行为，不能覆盖产品基础规则和 Tool 硬策略。

产品基础 Prompt 定义：

- MSInsight 产品身份；
- `msinsight observe/help/Command` 使用规则；
- 文件与客户源码访问边界；
- Bash 和 Skill 安全边界；
- 外部工具结果属于不可信数据；
- 产品级不可覆盖规则。

Agent Markdown 只能追加领域行为，不能替换基础 Prompt。有效 Prompt 指纹变化时，现有模型会话必须在下一次 Prompt 前重建。

## 8. Skill 文件格式与加载

Skill 使用 OpenCode 核心格式：

```yaml
---
name: pt-snap-fragmentation-forensics
description: 分析 Snapshot 碎片、reserved-active gap 和 segment 行为
license: MulanPSL-2.0
compatibility: Requires Python and pt-snap
metadata:
  domain: memory
---

这里是 Skill 指令。
```

首版只消费：

- `name`；
- `description`；
- `license`；
- `compatibility`；
- `metadata`。

以下扩展字段不得产生 Runtime Effect：

- `allowed-tools`、`disallowed-tools`；
- `model`、`effort`、`scope`；
- `hooks`；
- environment；
- System Prompt append。

发现这些字段时保留诊断，但 Skill 仍可作为纯指令加载。

Skill 可包含：

```text
scripts/
references/
templates/
```

资源只进入 Skill 内容清单。脚本不会在发现或加载时自动执行，只能由模型显式调用 Bash。

所有 Skill source 强制使用 `shellPolicy: deny`，禁用 `!` 反引号内联命令替换。该限制不禁止模型后续显式调用 Bash。

## 9. ACP Session 配置

### 9.1 创建

`session/new` 创建 Native Session，默认：

```text
primaryAgentId = general
promptStarted = false
```

响应包含 `primaryAgent` config option。Agent 的 source、available 和 diagnostics 使用 option `_meta` 中的 `msinsight.dev/*` 扩展键，ACP 核心层只保留 `id/name/type/currentValue/options` 等标准字段。前端创建 Session 后立即调用：

```text
session/set_config_option
  sessionId
  configId = primaryAgent
  value = <agent-id>
```

### 9.2 不可变规则

- 首次 `session/prompt` 前允许设置；
- 首次 Prompt 开始后拒绝修改；
- 选择无效、不可用或 `mode: subagent` 的 Agent 时拒绝；
- 前端切换 Primary Agent 时创建新 Session，不修改旧 Session。

### 9.3 恢复

持久化只保存 `primaryAgentId`，不保存 Markdown 快照。

恢复规则：

| 场景 | 行为 |
| --- | --- |
| v2 Session 未指定 ID | 使用 `general` |
| Agent 当前存在且有效 | 使用当前 Registry 内容 |
| Agent 已删除或无效 | 历史可查看、可删除；禁止继续 Prompt |
| 同 ID 内容变化 | 使用当前内容；恢复原模型会话时应用当前 Prompt 配置 |

不允许在 Agent 缺失时静默回退到 `general`，否则专项会话会被另一角色接管。

## 10. Tool 装配

首版模型可见 Tool：

```text
msinsight
skill
Bash
```

不启用：

```text
Edit
Write
NotebookEdit
Task
TaskOutput
后台 Bash
```

`Bash` 和 `skill` 均由 Native Runtime 自行实现：`Bash` 执行前强制产品硬策略、Primary Agent `permission.bash` 和宿主审批；`skill` 使用 Native Runtime adapter 控制加载和副作用。

## 11. Bash 权限模型

### 11.1 三层裁决

```text
产品硬策略
→ 当前 Primary Agent 的 permission.bash
→ Session 临时授权
```

优先级：

1. 产品 `deny` 永远拒绝；
2. Agent `deny` 拒绝；
3. 当前 Session 已批准的同 Agent、同规范化命令规则允许；
4. Agent `allow` 只允许不含 shell chain、pipe、重定向、命令替换或换行的单一命令；
5. Agent `ask`、未匹配规则或复杂 Shell 结构请求用户审批；
6. Agent 不得放宽产品硬限制。

命令模式匹配属于审批交互策略，不是沙箱边界。

### 11.2 `allow_always`

授权键至少包含：

```text
sessionId
primaryAgentId
normalizedCommandRule
```

它不表示允许全部 Bash，不跨 Session、不跨 Agent、不写入持久化存储；Agent 的兜底 `* : ask` 和复杂 Shell 结构使用规范化后的精确命令作为记忆键，不能升级为当前 Session 全部 Bash。Session 删除、Runtime reload 或退出后失效。

### 11.3 执行边界

首版 Bash：

- 使用 Native 实现的受控前台 `Bash`，不提供后台执行；
- 输入校验拒绝 `run_in_background: true`；
- 默认 `cwd` 为 Agent workspace，显式 `cwd` 必须位于 Session 文件系统 roots；
- 不允许覆盖环境变量；
- 默认超时 30 秒，最大 5 分钟；
- 不支持要求 stdin、TTY、密码或交互确认的命令；
- `session/cancel`、`session/delete` 和 Runtime 退出通过 Session 生命周期取消工具执行。

Native `Bash` 使用 `bash -c`，因此运行环境必须能从 `PATH` 找到 `bash`。不为 `python`、`pt-snap` 等领域依赖增加 Agent 级声明；Agent/Skill 在任务执行时自行检查并报告。

## 12. 权限交互

现有权限流保留：

```text
Native / external ACP request
→ PermissionService
→ SSE permission_request
→ React PermissionCard
→ allow_once / allow_always / deny
→ 权限请求恢复
```

权限对象从文件专用结构泛化：

```ts
interface PermissionRequest {
    kind: 'filesystem' | 'bash';
    sessionId: string;
    requestId: string;
    title: string;
    target: string;
    source: string;
    actions: Array<'allow_once' | 'allow_always' | 'deny'>;
    details?: Record<string, unknown>;
}
```

文件权限继续按规范路径和目录记忆；Bash 权限按 Session、Primary Agent 和规范化命令规则记忆。两者不能共享 allowlist key。

等待审批期间不开始 Bash 执行超时。Session cancel、delete、Agent Runtime switch 或 reload 必须使 pending approval 失效。

## 13. 前端交互

前端保留两层概念：

```text
Agent Runtime
  - MSInsight Native
  - OpenCode
  - Claude Code

Primary Agent（仅 MSInsight Native 显示）
  - General
  - PyTorch Snapshot Memory
  - ...
```

Primary Agent 列表来自 ACP config option，不新增 Web Server 专用 Agent API。切换行为：

1. 用户选择 Primary Agent；
2. 前端创建新 Session；
3. 立即调用 `session/set_config_option`；
4. 设置成功后激活该 Session；
5. 当前 Session 历史不迁移。

`@agent-name` 不用于切换，保留给未来 Subagent。

无效或开发来源 Agent 必须在选择器中显示来源/诊断。绑定 Agent 后，Session 列表和恢复状态应能显示当前 Primary Agent。

## 14. 生命周期与诊断

### 14.1 启动

1. 初始化 Agent Registry；
2. 验证 required `general`；
3. 初始化 Native Skill Registry；
4. 记录无效、覆盖和 unsupported field 诊断；
5. 加载 Native Session Store；
6. 对每个恢复 Session 解析当前 Primary Agent 状态。

### 14.2 Prompt

1. 验证绑定 Agent 当前有效；
2. 标记 `promptStarted`；
3. 解析产品基础 Prompt；
4. 追加 Agent body；
5. 恢复或复用原模型会话；
6. 通过 Tool Policy 执行模型 Tool Loop。

### 14.3 诊断类别

```text
AGENT_INVALID
AGENT_UNSUPPORTED_MODE
AGENT_OVERRIDDEN
AGENT_MISSING
SKILL_INVALID
SKILL_RUNTIME_EFFECT_IGNORED
SKILL_OVERRIDDEN
BASH_UNAVAILABLE
BASH_PERMISSION_DENIED
BASH_BUSY
BASH_INVALID_CWD
```

诊断应包含资源 ID、source、路径和稳定 message；API 不返回敏感环境变量或密钥。

## 15. 安全边界

1. Agent/Skill Markdown 属于指令，不是产品安全策略；
2. 产品基础 Prompt 与硬策略不可被 Markdown 覆盖；
3. Skill 文本、脚本内容、数据库内容和 Tool 输出都视为不可信数据；
4. Skill 加载不能执行 `!` 内联命令、Hooks 或 Runtime Patch；
5. Bash 的命令规则只决定审批，不代替文件系统、进程或操作系统沙箱；
6. `cwd` 和文件工具共享同一 Session root 边界；
7. 用户批准一个命令不授权其他 Session 或其他 Agent；
8. `pip install`、pickle import、持久化 focus 等操作默认走 `ask`；
9. 开发目录只在显式开发配置下启用，生产资源不可被覆盖。

## 16. 兼容边界

- Native v2 Session 未指定 `primaryAgentId` 时使用 `general`；
- v1 `sessions.json` 不加载、不迁移；
- 现有外部 ACP Agent 不受影响；
- 现有 `/skill-name` Web Server 注入可暂时保留给外部 ACP Agent，`msinsight-native` 使用受控 `skill` adapter；
- 当前固定 Native 领域行为迁入 `general.md`，产品安全基础仍保留在代码/产品 Prompt；
- Native Session 使用 `sessions/<sessionId>.jsonl` v3 metadata sidecar；消息与工具历史由 AI SDK runtime 持久化（`ai-sdk/sessions/<sessionId>.json`），加载后投影为 UI `content[]`；旧 `sessions.json` 和 v2 message JSONL 不加载、不迁移；
- Build Server 必须同时打包 `agents/` 和 `skills/`。

## 17. 验收标准

1. `general` 与 Snapshot Agent 都能在 Native Primary Agent 选择器出现；
2. 创建新 Session 后能在首次 Prompt 前绑定，首次 Prompt 后修改被拒绝；
3. Snapshot Agent 能自主调用 `Skill` 加载两个 pt-snap Skills；
4. `!` 内联命令不会在 Skill 加载时执行；
5. Bash 未匹配命令触发通用权限卡，批准后执行，拒绝后不启动进程；
6. `allow_always` 只对当前 Session、Agent 和命令规则生效；
7. 非法 `cwd`、后台 Bash、超时、并发和环境变量覆盖被拒绝；
8. cancel/delete 能清理 Bash；
9. Agent 缺失的恢复 Session 可查看历史但不能继续 Prompt；
10. 开发覆盖产生诊断并显示来源；
11. 外部 ACP Agent 的切换、会话和原有 config options 不回归。
