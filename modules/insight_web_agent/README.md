# Insight Bot 欢迎页与入口

工具栏中的 Insight Bot 入口使用 Logo 展示名称，悬停时显示提示并旋转一圈。提示框指向图标中心，在窗口右侧向左展开。

欢迎页使用动态点阵背景。点阵采用 demo 的 560 × 602px 画布，宽高分别不超过视口的 92%，以 Logo 和欢迎语整体居中。首次显示及新建对话时等待 200ms 后播放一次 1.5 秒的扩散波纹。鼠标附近的点会变亮、放大，移出后恢复。系统开启“减少动态效果”时展示静态点阵。

会话生成回复期间可以新建对话，原会话继续在后台执行。回复和排队消息保留在原会话中，可通过历史会话切回查看。首次发送尚未返回会话 ID 时也可以新建对话；取消操作会等待真实 ID 返回后发送到原会话。

## 实现与维护

- `src/components/WelcomePanel.tsx` 管理欢迎页布局及新建对话的重播标识。
- `src/components/WelcomeDots.tsx` 封装 Canvas、主题更新和组件卸载清理；`hitAreaRef` 指定响应鼠标的欢迎区，`replayKey` 变化时重播。
- `src/components/welcomeDotsEffect.ts` 管理点阵绘制、波纹、悬停、尺寸和可见性监听。点半径为 2px，中心间距为 10px，DPR 上限为 2；相关视觉参数集中在文件顶部。
- 动画直接更新 Canvas，静止后停止请求动画帧；隐藏或卸载时取消延迟任务和动画帧。主题切换只重绘，不重新生成点阵纹理。
- `src/hooks/useChatState.tsx` 为首次发送分配独立临时会话记录，收到真实 ID 后迁移对应队列、配置和提示；仅在该临时会话仍被选中时更新当前会话 ID。

## 验证

在 `modules/insight_web_agent` 目录运行：

```sh
CI=true node ../node_modules/react-scripts/bin/react-scripts.js test --watchAll=false --runInBand --runTestsByPath src/test/components/welcomeDotsEffect.test.ts src/test/components/WelcomeDots.test.tsx src/test/components/WelcomePanel.test.tsx src/test/components/ChatPanel.test.tsx
CI=true node ../node_modules/react-scripts/bin/react-scripts.js test --watchAll=false --runInBand --runTestsByPath src/test/hooks/useChatState.sessions.test.tsx
```

手动检查深浅主题下的首次显示、新建对话、抽屉重新打开、鼠标经过与移出，以及窄窗口中的背景对齐和引导按钮点击。开启系统“减少动态效果”后确认波纹与悬停动画停止。
