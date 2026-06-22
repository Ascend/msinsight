# 智能防抖函数（SmartDebounce）

前端公共库 `modules/lib` 提供了智能防抖函数，用于对高频异步请求进行防抖控制。该函数支持 trailing 和 leading 两种防抖模式，并提供三种使用方式以适配不同场景。

**源码位置：** `modules/lib/src/utils/createSmartDebounceRequestFunc.ts`

## 1. 两种防抖模式

| 模式 | `leading` 值 | 行为 | 最适合场景 |
| --- | --- | --- | --- |
| trailing（默认） | `false` | 等待期间持续替换参数，延迟结束后用**最终参数**发请求 | 搜索框输入、筛选条件变更等只关心最终状态的场景 |
| leading | `true` | 第一次调用**立即执行**请求，delay 窗口内所有调用方共享同一结果 | 按钮/交互响应等首次触发就要立即反馈的场景 |

### 1.1 trailing 模式状态流转

```text
idle ──调用──► pending ──timer到期──► inflight ──请求完成──► idle
                   │                      │
                   │ delay内再次调用       │ delay内再次调用
                   │ 替换参数，重置timer   │ 新state重新进入pending
                   ▼                      ▼
```

**典型场景：搜索框输入**

用户快速输入 "k" → "ke" → "key" → "keyword"，只有 "keyword" 会真正发出请求。

```typescript
const debouncedSearch = createSmartDebounceRequestFunc(
    async (keyword) => searchAPI(keyword),
    { delay: 300 }  // trailing 默认
);
```

### 1.2 leading 模式状态流转

```text
idle ──调用──► inflight（立即执行请求）
                    │
                    │ delay内再次调用 → 加入队列，不触发新请求
                    │
                    ├──请求在delay内完成──► delay到期 → 统一resolve队列 → idle
                    │
                    └──请求耗时>delay──► past_delay → 请求完成 → 立即resolve队列 → idle
```

**典型场景：交互按钮**

用户快速点击 3 次 → 第 1 次立即发请求，后 2 次共享结果，避免重复请求的同时保证首次点击即时响应。

```typescript
const debouncedQuery = createSmartDebounceRequestFunc(
    async (param) => queryAPI(param),
    { delay: 300, leading: true }
);
```

## 2. 三种使用方式

### 2.1 `createSmartDebounceRequestFunc` — 底层工具函数

最灵活、最底层的封装，直接包装任意异步函数。

```typescript
import { createSmartDebounceRequestFunc } from '@insight/lib/utils';

const debouncedQuery = createSmartDebounceRequestFunc(
    async (keyword: string) => {
        return fetchData('/api/search', { keyword });
    },
    {
        delay: 300,
        leading: false,
        keyFn: (keyword) => keyword[0],
        onBeforeRequest: (keyword) => setLoading(true),
        onAfterRequest: (result, keyword) => setLoading(false),
    }
);

const result = await debouncedQuery('hello');
debouncedQuery.cancel();
debouncedQuery.flush();
```

**最适合场景：**

- 非 React 环境（普通 TypeScript/JS 模块、工具类、服务层）
- 需要精细控制 `keyFn`、`onBeforeRequest`/`onAfterRequest` 等生命周期回调
- 需要手动管理防抖实例的生命周期（如单例服务、全局缓存）
- 需要调用 `cancel()`/`flush()`/`getPendingCount()` 等控制方法

### 2.2 `useSmartDebounceRequest` — React Hook 封装

React 组件专用，在 `createSmartDebounceRequestFunc` 基础上增加了 `useRef` 稳定实例和组件卸载自动 `cancel()`。

```typescript
import { useSmartDebounceRequest } from '@insight/lib/hooks';

function SearchComponent() {
    const debouncedSearch = useSmartDebounceRequest(
        async (keyword: string) => fetchData('/api/search', { keyword }),
        { delay: 300, leading: false }
    );

    const handleChange = (e) => {
        debouncedSearch(e.target.value).then(setResults);
    };

    return <input onChange={handleChange} />;
}
```

**最适合场景：**

- React 函数组件内部使用防抖请求
- 组件有卸载/挂载生命周期，需要自动清理
- 不想手动管理 `useEffect` 清理逻辑

### 2.3 `createDebounceRequest` — 请求层封装

基于 `createRequest` + `createSmartDebounceRequestFunc` 的组合封装，专门用于与后端通信的场景，已内置 `keyFn`（按 `command:params` 隔离不同接口）。

```typescript
import { createDebounceRequest } from '@insight/lib/utils';

const debouncedRequest = createDebounceRequest(connector, {
    delay: 300,
    leading: false,
});

const result = await debouncedRequest('communication/matrix/group', params);
```

**最适合场景：**

- 使用项目统一 `ClientConnector` 通信层的场景
- 需要防抖的 RPC/IPC 请求（如 Electron 主进程通信）
- 不需要自定义 `keyFn`，快速给已有请求接口加防抖

## 3. API 参考

### 3.1 函数签名

```typescript
function createSmartDebounceRequestFunc<T extends (...args: any[]) => Promise<any>>(
    requestFn: T,
    options?: {
        delay?: number;           // 延迟时间，默认 300ms
        keyFn?: (...args: Parameters<T>) => string;  // 按 key 隔离不同请求
        leading?: boolean;        // 是否使用 leading 模式，默认 false
        onBeforeRequest?: (...args: Parameters<T>) => void;
        onAfterRequest?: (result: Awaited<ReturnType<T>>, ...args: Parameters<T>) => void;
    }
): DebouncedRequestFunc<T>
```

### 3.2 返回的防抖函数接口

```typescript
interface DebouncedRequestFunc<T extends (...args: any[]) => Promise<any>> {
    (...args: Parameters<T>): Promise<Awaited<ReturnType<T>>>;
    cancel(key?: string): void;
    flush(key?: string): void;
    getPendingCount(): number;
    getPendingKeys(): string[];
}
```

### 3.3 控制方法

| 方法 | 说明 |
| --- | --- |
| `.cancel(key?)` | 取消指定 key 或所有等待中的请求 |
| `.flush(key?)` | 立即发送指定 key 或所有等待中的请求（跳过延迟） |
| `.getPendingCount()` | 获取当前等待中的请求数量 |
| `.getPendingKeys()` | 获取所有等待中的请求 key 列表 |

## 4. 注意事项

1. **内存泄漏**：若调用方创建了大量不同 key 的请求但从未 `await` 或调用 `cancel()`/`flush()`，内部状态会持续占用内存。长时间运行场景建议定期调用 `getPendingCount()` 监控
2. **React 组件**：优先使用 `useSmartDebounceRequest`，组件卸载时自动清理
3. **竞态安全**：内部所有异步回调都检查 state 是否仍然有效，不会出现旧 timer/请求误操作新状态的问题
