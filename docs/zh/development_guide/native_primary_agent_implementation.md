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

`general` 是 Native 内部默认助手的保留 ID。新会话默认绑定 `general`；当前不通过 ACP 公开 Primary Agent 列表或配置选项，前端不展示 Native 助手选择器，模型选择不受影响。定义仅存放在 Native 自身资源目录，打包到 `native-agent/agents/`，不复制到 `agent-workspace`。下文的专项助手格式和绑定机制作为内部扩展保留，不表示前端已开放选择。

Native 的输入框选择器通过 ACP `mode` 配置公开 **Permissions**，仅有 `Ask for Approval`（默认）与 `Full Access`。它不是 Primary Agent 切换：两者均保持 `general`。Full Access 跳过 Shell 和配置能力的交互审批，模型也不再重复请求执行授权；明确 deny、输入校验、取消、并发和超时限制仍生效。现有 Bash 不提供工作区路径沙箱，权限选择不新增或放宽路径隔离。权限仅绑定当前会话并持久化，旧会话默认 Ask；执行中不能切换，保存失败回滚，切换不清空历史，也不制造永久工具授权。外部 Agent 的模式行为不变。

## 2. 新增内置助手

在以下目录新增 Markdown 文件：

```text
modules/insight_web_agent/server/native-agent/agents/<assistant-id>.md
```

例如：

```text
modules/insight_web_agent/server/native-agent/agents/memory-leak-analyzer.md
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
- 当需要分析 allocator gap 时，加载 `pt-snap-memory-fragmentation`。
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

## 9. 欢迎页内存分析入口与对话交互

欢迎页卡片填入对应提示词，并将焦点移到输入框；用户确认后再发送。内存分析与慢卡分析分别设置 `memory-tuning-assistant`、`slow-card-analysis-assistant` 预设，服务端读取对应内置 Markdown，仅随选中后的这一条消息通过 `insight-assistant-preset://project` 发送。发送或入队即消费选择，后续追问不再附带预设；再次主动选择卡片可重新发送一次。Host 不再每轮传输基础 `system.md`，基础规则仍在启动时写入工作区 `AGENTS.md`/`CLAUDE.md`，由 Agent 加载。Native 将预设作为该轮用户上下文写入运行时历史，不追加到持久 system 指令；后续依靠会话历史延续。外部 ACP Agent 需支持工作区规则与嵌入文本资源加载。聊天界面只展示用户输入。

该入口使用提示词预设，不注册新的 Native 分析助手。修改输入内容会保留预设；清空输入、切换卡片或会话后清除。消息进入队列时保存自己的预设，避免影响后续普通消息。未知预设返回 HTTP 400，内存分析模板缺失或为空时返回 HTTP 503。新增预设时需要同步扩展前后端白名单，不能由客户端指定任意文件路径。

对话操作与状态展示：

- 输入 `/` 打开命令菜单，用上下方向键切换选项，按 Enter 或 Tab 填入选中的命令。
- 输入框默认显示 3 行，随输入和窗口宽度变化自动增高，最多显示 8 行，超出后在框内滚动；清空或发送后恢复基础高度。
- 输入框底部的 Agent 与模型选择器显示对应图标；模型弹层支持搜索，清空搜索后定位当前模型，并支持方向键选择。
- Agent 执行中，空输入框显示停止按钮；输入新内容或添加图片后显示发送按钮，发送后进入待执行队列。
- 向上滚动会暂停自动跟随；距底部超过 80px 才显示查看最新按钮，向下滚回底部附近或点击按钮恢复跟随。执行中按钮显示三个动态圆点。
- 分析状态持续到本轮结束事件，再显示完成、失败或已停止。输出过程说明不会提前标记完成；后续仍有思考或工具调用的说明文字按原始顺序归入分析过程，末尾正文继续流式展示。
- 总标题在进行中显示“已处理 + 用时”，从本轮开始持续计时，包含思考、工具调用和结果输出；正常结束后显示“用时 + 时间”，以本轮结束时间冻结。失败或停止时也冻结用时并保留对应状态。独立思考段仍使用各段耗时；缺少结束时间的旧消息回退到已有计时数据。
- 处理期间调用链末尾圆点显示蓝色呼吸动效；输入框上方和吸顶消息下方使用主题背景渐变。吸顶渐变仅在消息实际吸顶时显示，不额外占据消息间距。
- 设置页的 Native Agent 卡片带有“系统”角标，英文界面显示“System”。
