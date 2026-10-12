# 安装包 Skill 的 Runtime 原生发现集成

> 状态：设计已确认，按本文实施。

## 1. 架构决策

`insight_web_agent` 只负责把安装包 Skill 交付到共享 workspace，不解析 `SKILL.md`，不选择 Skill，也不向 Prompt 注入正文。Skill 的发现、描述匹配、调用和按需加载均由 `msinsight-native`、OpenCode、Claude Code 和 Codex 自己完成。

所有正式 Runtime 共用规范化后的 `<ACP_CWD>`：

```text
<resourceDir>/skills
        |
        | Host 启动时复制
        v
<ACP_CWD>/.agents/skills
        |
        +-- OpenCode / Codex / msinsight-native 原生发现
        |
        +-- <ACP_CWD>/.claude/skills 目录链接
                |
                +-- Claude Code 原生发现
```

切换 Runtime 不改变 ACP 子进程、ACP Session、Host 文件处理器和默认文件 allowlist 使用的 `cwd`。旧的按 Agent 隔离的 workspace 子目录不迁移、不删除，升级后不再使用。

## 2. workspace 初始化

Host 在创建首个 ACP adapter 前完成以下操作：

```text
创建并规范化 <ACP_CWD>
-> 创建 .agents/skills
-> 创建或校验 .claude/skills 目录链接
-> 复制安装包 Skill
-> 写入 AGENTS.md / CLAUDE.md
-> 创建 ACP adapter
```

`.claude/skills` 始终指向 `.agents/skills`：Unix 使用目录符号链接，Windows 使用 junction。已有普通目录或指向其他位置的链接会导致 Host 启动失败。

Skill 初始化只在 Host 启动时执行一次。Runtime 切换复用已经准备好的共享 workspace；自动发现探测仍使用 `<ACP_CWD>/.discovery/<runtime>`，不写入正式 workspace。

## 3. Skill 同步语义

同步器扫描安装包 `skills/` 下的一级目录，并逐个复制到 `<ACP_CWD>/.agents/skills/`：

- 同名目标先删除再复制，由当前安装包覆盖；
- 其他 workspace Skill 保留；
- 安装包中已移除的旧副本不自动删除；
- Host 不检查 `SKILL.md`、Skill 名称、frontmatter 或正文；
- 复制使用 Node.js `fs.cp` 默认行为。

源目录只能来自代码解析的安装包资源，目标只能位于共享 workspace。源与 workspace 重叠、目录结构冲突或复制失败都会终止 Host 启动，并阻止首个 ACP 子进程创建。

同步不提供 staging、backup、回滚或中断恢复。同名 Skill 复制失败时可能缺失或只完成部分复制，下次 Host 启动会重新覆盖。

## 4. Runtime 与 Web 行为

`msinsight-native` 启动时优先使用 `cwd/.agents/skills/`；该目录不存在时回退安装包 `skills/`。现有 `SkillRegistry` 负责发现元数据，`skill` Tool 负责惰性加载正文。

Web 的 `availableSkills` 只接受当前 Runtime 在 ACP `initialize` 元数据中主动报告的 `name` 和 `description`。Runtime 未报告时不显示 Skill 补全，Host 不扫描安装包补齐列表。

所有用户 Prompt 均原样下发，包括 `/skill-name arguments` 和其他 Slash Command。Host 不识别 Runtime 类型，不转换调用语法，也不读取或注入 Skill 正文。

未知 Runtime 仍可启动，但 Host 不保证它能发现 `.agents/skills/`，也不会回退到 Prompt 注入。已经启动的 Runtime 不承诺热加载 Skill 变化；多个 Host 进程不得并发初始化同一个 `<ACP_CWD>`。

## 5. 代码索引

- `modules/insight_web_agent/server/index.mjs`：共享 workspace 初始化和 ACP adapter 启动时序；
- `modules/insight_web_agent/server/services/workspaceSkillSync.mjs`：目录创建、Skill 复制和 Claude 目录链接；
- `modules/insight_web_agent/server/services/chatService.mjs`：消费 Runtime 自报 Skill 元数据并原样下发 Prompt；
- `modules/insight_web_agent/server/native-agent/index.mjs`：选择 workspace 或安装包 Skill 根；
- `modules/insight_web_agent/server/native-agent/skills/skillRegistry.mjs`：Native Skill 发现与惰性加载；
- `modules/insight_web_agent/scripts/build-server.mjs`：复制安装包 Skill 资源。
