# Smart Debounce Function (SmartDebounce)

The frontend common library `modules/lib` provides a smart debounce function for debounce control of high-frequency asynchronous requests. The function supports both trailing and leading debounce modes and provides three usage patterns for different scenarios.

**Source location:** `modules/lib/src/utils/createSmartDebounceRequestFunc.ts`

## 1. Two Debounce Modes

| Mode | `leading` value | Behavior | Best suited for |
| --- | --- | --- | --- |
| trailing (default) | `false` | Parameters are continuously replaced while waiting, and the request is sent with the **final parameters** after the delay ends | Scenarios that only care about the final state, such as search box input and filter condition changes |
| leading | `true` | The first call **executes the request immediately**, and all callers within the delay window share the same result | Scenarios that require immediate feedback on the first trigger, such as buttons and interactive responses |

### 1.1 State Transitions in trailing Mode

```text
idle ──call──► pending ──timer expires──► inflight ──request completes──► idle
                   │                      │
                   │ call again within    │ call again within delay:
                   │ delay: replace the   │ the new state re-enters pending
                   ▼ params and reset the ▼ timer
```

**Typical scenario: search box input**

When the user types quickly, "k" → "ke" → "key" → "keyword", only "keyword" actually triggers a request.

```typescript
const debouncedSearch = createSmartDebounceRequestFunc(
    async (keyword) => searchAPI(keyword),
    { delay: 300 }  // trailing by default
);
```

### 1.2 State Transitions in leading Mode

```text
idle ──call──► inflight (the request is executed immediately)
                    │
                    │ call again within delay → added to the queue, no new request
                    │
                    ├──request completes within delay──► delay expires → resolve the whole queue → idle
                    │
                    └──request takes longer than delay──► past_delay → request completes → resolve the queue immediately → idle
```

**Typical scenario: interactive button**

When the user clicks 3 times quickly, the 1st click sends the request immediately and the following 2 share the result, which avoids duplicate requests while guaranteeing an immediate response to the first click.

```typescript
const debouncedQuery = createSmartDebounceRequestFunc(
    async (param) => queryAPI(param),
    { delay: 300, leading: true }
);
```

## 2. Three Usage Patterns

### 2.1 `createSmartDebounceRequestFunc` — Underlying Utility Function

The most flexible and lowest-level wrapper. It directly wraps any asynchronous function.

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

**Best suited for:**

- Non-React environments (ordinary TypeScript/JS modules, utility classes, and service layers)
- Fine control over `keyFn`, `onBeforeRequest`/`onAfterRequest`, and other lifecycle callbacks
- Manual management of the debounce instance lifecycle (such as singleton services and global caches)
- Calling control methods such as `cancel()`, `flush()`, and `getPendingCount()`

### 2.2 `useSmartDebounceRequest` — React Hook Wrapper

Dedicated to React components. On top of `createSmartDebounceRequestFunc`, it adds a stable instance created with `useRef` and automatic `cancel()` on component unmount.

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

**Best suited for:**

- Debounced requests inside React function components
- Components with unmount/mount lifecycles that need automatic cleanup
- Cases where you do not want to manage the `useEffect` cleanup logic manually

### 2.3 `createDebounceRequest` — Request Layer Wrapper

A combined wrapper based on `createRequest` + `createSmartDebounceRequestFunc`, dedicated to scenarios that communicate with the backend. `keyFn` is built in (isolating different interfaces by `command:params`).

```typescript
import { createDebounceRequest } from '@insight/lib/utils';

const debouncedRequest = createDebounceRequest(connector, {
    delay: 300,
    leading: false,
});

const result = await debouncedRequest('communication/matrix/group', params);
```

**Best suited for:**

- Scenarios that use the project's unified `ClientConnector` communication layer
- RPC/IPC requests that need debouncing (such as Electron main process communication)
- Adding debouncing to an existing request interface quickly without a custom `keyFn`

## 3. API Reference

### 3.1 Function Signature

```typescript
function createSmartDebounceRequestFunc<T extends (...args: any[]) => Promise<any>>(
    requestFn: T,
    options?: {
        delay?: number;           // Delay time. The default value is 300ms.
        keyFn?: (...args: Parameters<T>) => string;  // Isolate different requests by key.
        leading?: boolean;        // Whether to use leading mode. The default value is false.
        onBeforeRequest?: (...args: Parameters<T>) => void;
        onAfterRequest?: (result: Awaited<ReturnType<T>>, ...args: Parameters<T>) => void;
    }
): DebouncedRequestFunc<T>
```

### 3.2 Returned Debounce Function Interface

```typescript
interface DebouncedRequestFunc<T extends (...args: any[]) => Promise<any>> {
    (...args: Parameters<T>): Promise<Awaited<ReturnType<T>>>;
    cancel(key?: string): void;
    flush(key?: string): void;
    getPendingCount(): number;
    getPendingKeys(): string[];
}
```

### 3.3 Control Methods

| Method | Description |
| --- | --- |
| `.cancel(key?)` | Cancels the pending requests of the specified key, or all pending requests |
| `.flush(key?)` | Sends the pending requests of the specified key, or all pending requests, immediately (skipping the delay) |
| `.getPendingCount()` | Obtains the number of currently pending requests |
| `.getPendingKeys()` | Obtains the list of keys of all currently pending requests |

## 4. Notes

1. **Memory leak**: if a caller creates a large number of requests with different keys but never `await`s them or calls `cancel()`/`flush()`, the internal state keeps occupying memory. For long-running scenarios, call `getPendingCount()` periodically to monitor it
2. **React components**: `useSmartDebounceRequest` is preferred, as it cleans up automatically when the component unmounts
3. **Race safety**: all internal asynchronous callbacks check whether the state is still valid, so an old timer or request never operates on a new state by mistake
