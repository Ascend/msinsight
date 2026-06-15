# Contributing to Mindstudio Insight

感谢您考虑为Mindstudio Insight贡献力量! 我们欢迎任何形式的贡献——无论是修复错误，功能增强，文档改进，或是任何反馈建议。无论您是经验丰富的开发者，还是第一次参与开源项目，您的帮助都非常宝贵。

您的支持可以有很多种形式：

- 报告问题或意外行为。
- 建议或实现新功能。
- 改进或扩展文档。
- 审阅Pull Request并协助其他贡献者
- 分享推荐：在博客文章、社交媒体中介绍MindStudio Insight，或为仓库项目点个🌟。

我们期待您的参与！

# 寻找可以贡献的Issue

正在寻找新issue的切入点？ 可以查看以下议题：

- [good-first-issue](https://gitcode.com/Ascend/msinsight/issues?state=all&scope=all&page=1&categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:22797,%22name%22:%22good-first-issue%22%257D%255D,%22label%22:%22good-first-issue%22%257D%255D)
- [help-wanted](https://gitcode.com/Ascend/msinsight/pulls?categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:22796,%22name%22:%22help-wanted%22%257D%255D,%22label%22:%22help-wanted%22%257D%255D&state=opened&scope=all&page=1)
- 除了上述两个新手友好issue外，我们也提供了其他的[issue模板](https://gitcode.com/Ascend/.gitcode/tree/master/.gitcode/ISSUE_TEMPLATE)来作为参考。
- 此外，您也可以通过 [RFC](https://gitcode.com/Ascend/msinsight/issues?state=all&scope=all&page=1&categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:25328,%22name%22:%22rfc%22%257D%255D,%22label%22:%22rfc%22%257D%255D) 和 [Roadmap](https://gitcode.com/Ascend/msinsight/issues?state=all&scope=all&page=1&categorysearch=%255B%257B%22field%22:%22labels%22,%22value%22:%255B%257B%22id%22:22807,%22name%22:%22roadmap%22%257D%255D,%22label%22:%22roadmap%22%257D%255D)来了解开发计划与规划。

# Pull Requests 与 Code Reviews

感谢您提交 PR！为优化审查流程，请遵循以下指南：

遵循我们的 Pull Request [模板与规范](.gitcode/PULL_REQUEST_TEMPLATE.md)

遵守 pre-commit（预提交）[代码规范检查](docs/zh/development_guide/develop_guide.md)，在提交PR之前请确保所有检查通过

对涉及用户端功能的改动，请同步更新对应的用户和开发者文档

在 CI 工作流中 添加或更新测试；若无需测试，请说明原因

在上述准备工作完成后提交代码，请输入 compile 命令触发机器人编译流水线

流水线编译通过后请联系[仓库管理和维护成员](https://gitcode.com/Ascend/msinsight/member)进行检视与合入

# License

请参阅 [LICENSE](./License) 文件以获取完整详细信息。

# 构建与测试

在提交PR之前，建议您先在本地搭建开发环境，构建`insight`并运行相关测试。

**开发者测试相关要求**：
合入后端代码时，有开发者测试的相关要求。

1. 后端DT使用测试框架GoogleTest，DT代码位置是server/src/test，Linux系统上，在build目录下执行命令`bash cpp_coverage.sh`即可生成覆盖率。后端覆盖率的要求是行覆盖率达到80%，分支覆盖率达到60%。后端合入新特性代码时，要求同时补充DT。详细步骤可以参考[开发指南](./docs/zh/development_guide/develop_guide.md)的3.3.3节。

**预冒烟测试相关要求**：
合入前端代码或后端代码时，有预冒烟测试的相关要求。

1. 预冒烟测试是端到端的测试，用于验证软件的主要功能是否正常运行，涉及前端和后端。预冒烟测试使用测试框架Playwright。详细步骤可以参考[开发指南](https://gitcode.com/Ascend/msinsight/blob/master/docs/zh/development_guide/develop_guide.md)的3.5节。

## PR标题与分类

只有特定类型的PR才会被审核。请在PR标题前添加合适的前缀，以明确PR类型。请使用以下分类之一:

- `[Platform]`: 底座平台相关的新功能、优化或bug修复。
- `[Common]`: 公共模块相关的新功能、优化或bug修复。
- `[Timeline]`: 系统调优-集群相关的新功能、优化或bug修复。
- `[Memory]`: 系统调优-内存相关的新功能、优化或bug修复。
- `[Operator]`: 系统调优-算子相关的新功能、优化或bug修复。
- `[MemScope]`: 系统调优-内存详情相关的新功能、优化或bug修复。
- `[Cluster]`: 系统调优-集群详情相关的新功能、优化或bug修复。
- `[RL]`: 系统调优-强化学习相关的新功能、优化或bug修复。
- `[Advisor]`: 系统调优-专家建议相关的新功能、优化或bug修复。
- `[Source]`: 算子调优相关的新功能、优化或bug修复。
- `[Servitization]`: 服务化调优相关的新功能、优化或bug修复。

## Commit Requirement

为保持commit记录清晰，请确保每个PR仅包含一个commit。
如果您的PR当前包含多个commits，请在提交前使用以下任一方法（包括但不限于）将其合并为单个commit。(尽管GitCode在合并PR时提供了`Squash 合并`的选项, 提前将PR整理为单个简洁的commit仍然被视为最佳实践，并且深受committer们的欢迎。)

### 方式一：交互式变基（推荐）

- 查看需要合并的最近几个commit（例如最近3个）：

``` bash
git log --oneline -n 3
```

- 启动交互式rebase (将`N`替换为需要合并的commit数量):

``` bash
git rebase -i HEAD~N
```

- 在打开的编辑器中:
    - 保留第一个commit的`pick`。
    - 将其余commit前的`pick`修改为`squash`(或简写为`s`) 。
- 保存并关闭。随后会打开新窗口，供您编写合并后的简洁、有意义的commit信息。
- 强制推送更新后的分支 (仅限于您自己的特性分支):

``` bash
git push --force-with-lease origin your-branch-name
```

### 方式二：reset + 新建commit

```bash
# 获取最新的待合入的目标分支（如main）
git fetch origin main

# Soft-reset到主干分支--此操作会保存所有修改并回归到暂存区。
git reset --soft origin/main

# 将所有更改提交为一个新的commit
git commit -m "feat: concise description of your change"

# 强制推送以更新PR分支
git push --force-with-lease origin your-branch-name

```

> 提示: 如果您不确定应基于哪个目标分支，请查看仓库的默认分支或咨询Maintainer.
<br/>
> 警告：切勿对共享或受保护的分支执行强制推送。

# 感谢

我们感谢您对 MindStudio Insight 的贡献。您的每一份努力，都让这个项目变得更强大、更易用。祝您创造愉快，编程开心！
