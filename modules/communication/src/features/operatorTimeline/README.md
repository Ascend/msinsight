# 通信算子缩略图

保留旧版的 ECharts 坐标轴、双轴 dataZoom、WebGL2 算子绘制、tooltip、快捷键、右键菜单与慢卡高亮语义。组件通过 props 接收数据和事件，不依赖旧 cluster 页面、Session、Store 或后端请求。

- `index.tsx`：React 生命周期、快捷键、右键菜单及尾对齐提示，优先使用 lib 组件。
- `ChartController.ts`：ECharts/WebGL 协调、事件绑定、视口更新、五秒高亮、回退及资源释放。
- `WebGLRenderer.ts`：沿用旧版 GPU 实例化绘制、亚像素覆盖率及高亮图层算法，移除对比分支。
- `dataIndex.ts`：按 Rank 和时间建立索引，可见算子筛选、重叠区间命中及定位；保留原始 ns 数据供跳转使用。
- `geometry.ts`：从 ECharts 实际坐标系计算绘图与命中位置。
- `options.ts`：坐标轴、缩放条、动态高度与 ECharts custom series 回退配置。
- `navigation.ts`：焦点内键盘导航，采用旧版按完整 Rank 缩放和平移的算法。
- `tooltip.ts`、`style.css`：旧版 tooltip 的字段、颜色标记和布局，动态内容使用 DOM 文本写入。
- `colors.ts`、`types.ts`：配色规则与组件协议。
- `__tests__/`：索引、几何、导航、生命周期、回退及组件交互测试。

正常路径默认展示全部 Rank，由 WebGL2 绘制当前视口内算子；WebGL2 不可用或上下文丢失时使用 ECharts custom series。该回退不具备相同的 GPU 绘制性能。万卡索引测试不等同于真实万卡性能验收。

接口分页仍由 `api/index.ts` 处理：按批拉取完整数据后传入本组件，没有页码切换 UI，也不是按可见区域向后端查询。开发预览继续使用显式 Mock，真实数据联调另行验收。
