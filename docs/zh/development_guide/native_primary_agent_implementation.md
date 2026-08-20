# 为 Native Agent 添加分析助手

## 1. 概念与命名

`msinsight-native` 是固定的 **Agent Runtime 名称**，负责 ACP 会话、AI SDK Runtime、Tool、权限和资源加载。本文添加的是 Runtime 内可选的 **Native 分析助手**，不是新增一个 ACP Runtime。

三类名称应分开：

| 名称 | 来源 | 用途 | 示例 |
| --- | --- | --- | --- |
| Runtime 名称 | 产品固定 | Agent Runtime 选择器、进程身份 | `msinsight-native` |
| 助手 ID | Markdown 文件名 | Session 绑定、配置和持久化 | `memory-leak-analyzer` |
| 助手显示名称 | frontmatter `name` | 前端下拉框 | `内存泄漏分析助手` |

新增助手时不要使用 `msinsight-native` 作为文件名、助手 ID 或显示名称。当前解析器虽然会接受这个合法的 kebab-case ID，但项目命名规范禁止助手与 Runtime 重名，否则日志、配置项和 UI 语义会混淆。

推荐：

- 文件名使用小写 kebab-case，例如 `memory-leak-analyzer.md`；
- `name` 使用面向用户的职责名称，例如 `内存泄漏分析助手`；
- `description` 用一句话说明适用场景；
- 不使用 `agent`、`assistant` 之外的产品 Runtime 名称充当 ID；
- 不覆盖已有 ID，除非明确进行开发环境调试。

`general` 是内置默认助手的保留 ID。新增专项助手应使用其他 ID。

## 2. 新增内置助手

在以下目录新增 Markdown 文件：

```text
modules/insight_web_agent/agents/<assistant-id>.md
```

例如：

```text
modules/insight_web_agent/agents/memory-leak-analyzer.md
```

参考模板：

```markdown
---
name: 内存泄漏分析助手
description: 使用 MindStudio Insight 页面能力定位未释放内存块和泄漏模式。
mode: primary
permission:
  bash:
    "python -V": allow
    "*": ask
---

你是 MindStudio Insight 的内存泄漏分析助手。

## 工作方式

- 优先使用 `msinsight` 的 `observe` 获取当前页面状态。
- 先调用 `msinsight` 的 `help`，再查询目标 Command 的完整参数结构。
- 优先使用页面表格的过滤、排序和读取能力，不绕过页面直接解析原始数据。
- 区分采集前已存在的内存块与采集期间申请但未释放的内存块。
- 证据足够时先给出主要结论；用户未要求完整清单时，不遍历全部记录。
- 回答中列出关键 ID、大小、状态、申请事件和判断依据。
```

新增文件后无需修改助手列表代码。Native Agent 启动时会自动扫描 `agents/*.md`，符合条件的助手会出现在 `primaryAgent` 配置项中。

构建时，`scripts/build-server.mjs` 会把整个 `agents/` 目录复制到 `dist-server/agents/`。

## 3. Markdown 格式

### 3.1 必需结构

文件必须包含 YAML frontmatter 和非空正文：

```markdown
---
description: 助手描述
mode: primary
---

助手指令正文。
```

支持的 frontmatter 字段：

| 字段 | 是否必需 | 说明 |
| --- | --- | --- |
| `name` | 否 | 前端显示名称；为空时根据助手 ID 生成 |
| `description` | 是 | 非空字符串，用于助手选择器 |
| `mode` | 否 | `primary`、`all` 或 `subagent`；默认 `all` |
| `permission.bash` | 否 | Bash 命令模式到 `allow / ask / deny` 的映射 |

其他字段不会改变 Runtime 行为，只会产生 unsupported diagnostic。

### 3.2 助手 ID

助手 ID 由文件名产生，不含 `.md`。允许格式：

```text
^[a-z0-9]+(?:-[a-z0-9]+)*$
```

有效示例：

```text
memory-leak-analyzer
snapshot-reviewer
communication-diagnoser
```

无效示例：

```text
MemoryLeak
memory_leak
内存助手
msinsight-native
```

前三个不符合 ID 格式；最后一个虽符合格式，但与 Runtime 固定名冲突，不应使用。

### 3.3 mode

- `primary`：可作为 Native Session 的主分析助手；
- `all`：当前也可作为主分析助手；
- `subagent`：会被解析，但当前 Runtime 不支持委派，不会作为可选 Primary Agent。

新增可选助手通常使用：

```yaml
mode: primary
```

## 4. 编写助手指令

助手正文会追加到以下有效 System Prompt 之后：

```text
产品基础规则
+ Host System Prompt
+ 当前 Native 助手正文
```

正文应描述领域工作方法，不应重复或试图覆盖产品硬规则。

建议写入：

- 助手职责和适用场景；
- 应优先使用的页面能力；
- 领域分析步骤；
- 证据充分时的停止条件；
- 输出结构和必需证据；
- 哪些操作需要用户确认。

不要写入：

- API Key、Token 或本地凭据；
- 绕过文件系统和 Bash 权限的指令；
- 强制修改模型、Provider、环境变量或产品 System Prompt 的要求；
- 自动执行安装、删除、上传等副作用操作的要求；
- 与实际 `msinsight help` 返回结构不一致的命令示例。

命令输入结构应以运行时 `msinsight help` 为准，不要在助手正文中复制可能过期的完整 schema。

## 5. Bash 权限

示例：

```yaml
permission:
  bash:
    "python -V": allow
    "python -m pip show *": allow
    "python -m pip install *": ask
    "*": ask
```

规则行为：

- `allow`：匹配的简单命令可直接执行；
- `ask`：通过权限卡请求用户确认；
- `deny`：拒绝执行。

产品硬策略优先于助手规则。即使助手声明 `allow`，危险命令、后台执行、环境变量覆盖、越界 cwd 和复杂 Shell 结构仍可能被拒绝或要求确认。

规则匹配采用“更具体模式优先”；建议始终保留：

```yaml
"*": ask
```

不要配置大范围 `"*": allow`。

## 6. 关联 Skill

助手可以在正文中说明何时应加载某个 Skill，但不要把 Skill 全文复制进助手文件。

Skill 放在：

```text
modules/insight_web_agent/skills/<skill-name>/SKILL.md
```

助手正文可写：

```markdown
- 当需要安装或验证 pt-snap 时，加载 `pt-snap-setup`。
- 当需要分析 allocator gap 时，加载 `pt-snap-fragmentation-forensics`。
```

模型通过 Native `skill` Tool 按需加载正文。Skill 中要求用户确认或停止的步骤必须保留，助手不能用替代分析绕过。

## 7. 常见问题

### 助手没有出现在选择器中

检查：

- `mode` 是否为 `primary` 或 `all`；
- frontmatter 和正文是否完整；
- 文件名是否符合 ID 正则；
- 启动日志中是否存在 `AGENT_INVALID`；
- 文件是否位于实际资源根的 `agents/` 目录；
- 构建产物是否已经复制该文件。

## 8. 提交检查清单

- [ ] 助手 ID 与 `msinsight-native`、`general` 和已有助手不同；
- [ ] `name` 是清晰的职责名称；
- [ ] `description`、`mode` 和正文有效；
- [ ] 页面工具使用流程与当前 Command 契约一致；
- [ ] 定义了证据充分时的停止条件，避免无边界遍历；
- [ ] Bash 规则最小授权且保留 `"*": ask`；
- [ ] 需要的 Skill 已存在且只按需加载；
- [ ] Registry、构建产物和页面选择器均已验证；
- [ ] 未包含凭据、环境特定路径或未授权副作用操作。
