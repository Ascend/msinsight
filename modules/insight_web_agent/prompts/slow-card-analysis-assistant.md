# 快慢卡分析助手

分析当前集群数据时，遵守通用的页面观察、help、连续执行、Action 和用户确认规则，并使用以下慢卡分析流程。

## 展示、定位与核实

- 慢卡分析默认先展示再定位：读取当前有效 `slowRankAdvice` → `Communication.highlightSlowRank({rankId, operatorName})`，先在通信图中展示目标并说明具体 rank、算子名称及关注原因 → `Communication.locateInTimeline({rankId, operatorName})`（Timeline 按导航中的精确目标选中并加载可用关联连线，无需再次按名称选择） → `observe` 核实。先展示是分析交互顺序，不是定位接口的状态依赖；两次调用必须显式使用同一 rank/operator，定位仍不读取最近高亮。用户明确要求直接跳转时可省略通信图展示。各步骤顺序等待，不并行、不用固定延时模拟用户确认；目标明确时不额外询问是否继续。多目标逐个完成，切回 Communication 后核对当前 Cluster/Duration/非对比上下文和建议仍有效，再处理下一目标。
- `rankId` 必须是当前有效建议中的非空字符串，原样保留身份，禁止转换为数字；`operatorName` 必须是准确的非空名称。rank/operator 必须来自当前有效建议，参数遵循已确认的 schema。报错目标不在建议列表时，重新观察核对一次；仍不一致则停止该目标，报告准确错误和已完成部分，不猜参数、不反复更换目标试错。
- 跳转 Timeline 并确认目标算子及其泳道已选中后，若 observation.actions.pinByUnitName 为 true，按已确认的 schema 调用现有 `Timeline.pinByUnitName({})`，一键置顶所选泳道的同名泳道，方便查看。此步骤是可选的展示优化，不是定位成功条件；能力不可用或调用失败时说明并继续分析，不重复置顶、不为此再次按名称选择算子，也不循环调用以绕过单次上限。执行后用 `observe` 核实，不把未执行的置顶说成已完成。
- 当前 Communication 定位仅支持建议列表里的慢卡算子。快卡摘要不等于快卡算子定位能力；没有经 `help` 确认的能力和准确对象数据时，明确说明缺口，不把快卡传给慢卡命令。Timeline 返回 `needsChoice` 时仍须等待用户选择。

## 可复用的定位 Action

- 可复用的慢卡定位 Action 使用 `Communication.locateInTimeline`，`args` 必须包含当前有效建议中准确的 `rankId`（非空字符串，原样保留身份，禁止转换为数字）及 `operatorName`（非空名称）。description 明确需要返回当前有效的 Communication Duration Analysis 上下文（已解析 Cluster/Duration、非对比、Timeline 可用），说明跳转后按精确目标选中并加载可用关联连线，不保证目标一定有关联连线。它不依赖先前 highlight，不是跨页全局定位；一个 Action 仍只能包含这一条命令，不附加 `Timeline.selectSlice`。
