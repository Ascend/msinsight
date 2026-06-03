/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

/**
 * 防抖请求函数接口
 * 包含扩展的控制方法
 */
export interface DebouncedRequestFunc<T extends (...args: any[]) => Promise<any>> {
    (...args: Parameters<T>): Promise<Awaited<ReturnType<T>>>;
    /** 取消指定key或所有等待中的请求 */
    cancel(key?: string): void;
    /** 立即发送指定key或所有等待中的请求 */
    flush(key?: string): void;
    /** 获取当前等待中的请求数量 */
    getPendingCount(): number;
    /** 获取所有等待中的请求key */
    getPendingKeys(): string[];
}

/**
 * 智能防抖高阶函数
 *
 * 两种模式：
 * - trailing（默认）：等待期间更新参数，延迟结束后发送最终参数的请求
 * - leading：第一次调用立即执行，delay 窗口内所有调用方统一等待，窗口结束时统一返回第一次的结果
 *
 * ⚠️ 内存泄漏警告：
 *   本函数内部使用 Map<string, KeyState> 维护每个 key 的状态。状态仅在请求完成/取消/flush 后清理。
 *   若调用方创建了带有不同 key 的大量请求但从未等待其完成（如未 await 且不调用 cancel/flush），
 *   对应的状态对象会持续占用内存。建议：
 *   - 确保所有调用方 await 返回的 Promise 或妥善处理 rejection
 *   - 组件卸载/页面关闭时调用 cancel() 清理所有待处理状态
 *   - 长时间运行的场景下定期调用 getPendingCount() 监控
 *
 * 状态流转：
 *
 * trailing 模式：
 * ```
 * idle ──debounced('a')──► pending
 *                              │
 *                              │ delay 内 debounced('b') → 替换参数，重置 timer
 *                              │
 *                              ▼
 *                         inflight ──请求完成──► idle
 *                              │
 *                              │ delay 内 debounced('b') → 新 state 重新进入 pending
 *                              │
 *                              ▼
 *                         pending ──timer到期──► inflight
 * ```
 *
 * leading 模式：
 * ```
 * idle ──debounced('a')──► inflight（立即执行请求）
 *                              │
 *                              │ delay 内 debounced('b/c') → 加入队列，不触发新请求
 *                              │
 *                              ├───请求在 delay 内完成────┐
 *                              │                          │
 *                              ▼                          │
 *                         delay到期 → 统一 resolve 队列   │
 *                              │                          │
 *                              ▼                          │
 *                             idle ◄──────────────────────┘
 *                              │
 *                              └───请求耗时 > delay────────┐
 *                                                          │
 *                              delay到期 → past_delay      │
 *                              │                           │
 *                              ▼                           │
 *                         请求完成 → 立即 resolve 队列    │
 *                              │                           │
 *                              ▼                           │
 *                             idle ◄───────────────────────┘
 * ```
 *
 * @param requestFn - 原始请求函数 (params) => Promise<any>
 * @param options - 配置选项
 * @returns 包装后的防抖函数
 */
export function createSmartDebounceRequestFunc<T extends (...args: any[]) => Promise<any>>(
    requestFn: T,
    options?: {
        delay?: number;
        keyFn?: (...args: Parameters<T>) => string;
        leading?: boolean;
        onBeforeRequest?: (...args: Parameters<T>) => void;
        onAfterRequest?: (result: Awaited<ReturnType<T>>, ...args: Parameters<T>) => void;
    }
): DebouncedRequestFunc<T> {
    type Resolve = (value: Awaited<ReturnType<T>>) => void;
    type Reject = (reason?: any) => void;
    type QueueItem = { resolve: Resolve; reject: Reject };

    /**
     * 内部状态对象
     *
     * 通用字段：
     *   - status   : 当前状态
     *   - timer    : setTimeout 句柄（trailing 的 pending 定时器 / leading 的 delay 定时器）
     *   - args     : 最后一次调用传入的参数
     *
     * trailing 模式专用：
     *   - trailingResolve / trailingReject : pending/inflight 时存储单次调用的 resolve/reject
     *
     * leading 模式专用：
     *   - leadingQueue     : delay 窗口内所有调用方的 Promise 回调队列
     *   - leadingResult    : 请求成功时缓存的结果
     *   - leadingError     : 请求失败时缓存的错误
     *   - leadingCompleted : 请求是否已完成
     */
    type KeyState = {
        status: 'idle' | 'pending' | 'inflight' | 'past_delay';
        timer?: NodeJS.Timeout;
        args?: Parameters<T>;

        // --- trailing 模式专用 ---
        trailingResolve?: Resolve;
        trailingReject?: Reject;

        // --- leading 模式专用 ---
        leadingQueue: QueueItem[];
        leadingResult?: Awaited<ReturnType<T>>;
        leadingError?: any;
        leadingCompleted: boolean;
    };

    const {
        delay = 300,
        keyFn,
        leading = false,
        onBeforeRequest,
        onAfterRequest
    } = options || {};

    const states = new Map<string, KeyState>();

    // ========== leading 模式辅助函数 ==========

    const resolveLeadingQueue = (state: KeyState) => {
        while (state.leadingQueue.length > 0) {
            const { resolve } = state.leadingQueue.shift()!;
            resolve(state.leadingResult as Awaited<ReturnType<T>>);
        }
    };

    const rejectLeadingQueue = (state: KeyState, reason?: any) => {
        while (state.leadingQueue.length > 0) {
            const { reject } = state.leadingQueue.shift()!;
            reject(reason);
        }
    };

    const cleanupLeadingState = (state: KeyState) => {
        if (state.leadingError) {
            rejectLeadingQueue(state, state.leadingError);
        } else {
            resolveLeadingQueue(state);
        }
    };

    /** leading 模式：delay 到期 */
    const onLeadingDelayExpired = (key: string, state: KeyState) => {
        const current = states.get(key);
        if (!current || current !== state) return;

        if (state.leadingCompleted) {
            // 请求已完成，统一 resolve/reject 队列中的调用方
            cleanupLeadingState(state);
            states.delete(key);
        } else {
            // 请求未完成，标记为 past_delay，等请求完成后立即 resolve
            state.status = 'past_delay';
        }
    };

    /** leading 模式：请求完成 */
    const onLeadingRequestDone = (key: string, state: KeyState) => {
        // 防御竞态：检查 state 是否仍有效
        const current = states.get(key);
        if (!current || current !== state) return;

        if (state.status === 'past_delay') {
            // delay 已结束但请求刚完成，立即 resolve/reject 队列
            cleanupLeadingState(state);
            states.delete(key);
        }
        // 如果状态是 inflight，什么都不做，等 delay timer 到期统一 resolve
    };

    /** leading 模式：执行请求 */
    const executeLeading = async (key: string, args: Parameters<T>, state: KeyState) => {
        try {
            onBeforeRequest?.(...args);
            const result = await requestFn(...args);
            onAfterRequest?.(result, ...args);

            // 防御竞态：请求完成后检查 state 是否仍有效（可能被 cancel 或 flush 清理过）
            const current = states.get(key);
            if (current === state) {
                state.leadingResult = result;
                state.leadingCompleted = true;
                onLeadingRequestDone(key, state);
            }
        } catch (error) {
            const current = states.get(key);
            if (current === state) {
                state.leadingError = error;
                state.leadingCompleted = true;
                onLeadingRequestDone(key, state);
            }
        }
    };

    // ========== trailing 模式辅助函数 ==========

    /** trailing 模式：执行请求 */
    const executeRequest = async (key: string, args: Parameters<T>, state: KeyState) => {
        try {
            onBeforeRequest?.(...args);
            const result = await requestFn(...args);
            onAfterRequest?.(result, ...args);
            // 防御竞态：请求完成后检查 state 是否仍有效（可能被 cancel 或新调用替换过）
            if (states.get(key) === state && state.status === 'inflight') {
                state.trailingResolve?.(result);
                states.delete(key);
            }
        } catch (error) {
            if (states.get(key) === state && state.status === 'inflight') {
                state.trailingReject?.(error);
                states.delete(key);
            } else {
                // 状态已被改变（如 cancel），仍需 reject 避免 Promise 永远 pending
                state.trailingReject?.(error);
            }
        }
    };

    /** trailing 模式：启动定时器 */
    const startTimer = (key: string, state: KeyState) => {
        state.timer = setTimeout(() => {
            // 防御竞态：
            // 1. cancel 后 states 中已不存在该 key
            // 2. cancel + 新调用后 states 中已是新的 state 对象
            // 3. 旧 timer 回调应忽略，避免误操作新请求
            const current = states.get(key);
            if (!current || current !== state || current.status !== 'pending') return;
            current.status = 'inflight';
            current.timer = undefined;
            executeRequest(key, current.args!, current);
        }, delay);
    };

    // ========== 核心包装函数 ==========

    const debouncedFn = (...args: Parameters<T>): Promise<Awaited<ReturnType<T>>> => {
        const key = keyFn ? keyFn(...args) : 'default';

        return new Promise((resolve, reject) => {
            let state = states.get(key);
            if (!state) {
                state = {
                    status: 'idle',
                    leadingQueue: [],
                    leadingCompleted: false,
                };
                states.set(key, state);
            }

            if (leading) {
                if (state.status === 'idle') {
                    // 第一次调用：立即执行请求，启动 delay 窗口
                    state.status = 'inflight';
                    state.args = args;
                    state.leadingQueue = [{ resolve, reject }];
                    state.leadingCompleted = false;
                    state.leadingResult = undefined;
                    state.leadingError = undefined;
                    state.timer = setTimeout(() => onLeadingDelayExpired(key, state!), delay);
                    executeLeading(key, args, state);
                } else if (state.status === 'inflight') {
                    // delay 窗口内：加入队列，统一等待
                    state.leadingQueue.push({ resolve, reject });
                } else if (state.status === 'past_delay') {
                    // delay 已过但请求未完成，新调用开始新的周期
                    const newState: KeyState = {
                        status: 'inflight',
                        args,
                        leadingQueue: [{ resolve, reject }],
                        leadingCompleted: false,
                    };
                    states.set(key, newState);
                    newState.timer = setTimeout(() => onLeadingDelayExpired(key, newState), delay);
                    executeLeading(key, args, newState);
                }
            } else {
                if (state.status === 'idle' || state.status === 'pending') {
                    // idle / pending：替换为最新参数，重置定时器
                    if (state.timer) clearTimeout(state.timer);
                    state.status = 'pending';
                    state.args = args;
                    state.trailingResolve = resolve;
                    state.trailingReject = reject;
                    startTimer(key, state);
                } else if (state.status === 'inflight') {
                    // inflight：丢弃旧结果，创建新 state 重新进入 pending
                    const newState: KeyState = {
                        status: 'pending',
                        args,
                        leadingQueue: [],
                        leadingCompleted: false,
                        trailingResolve: resolve,
                        trailingReject: reject,
                    };
                    states.set(key, newState);
                    startTimer(key, newState);
                }
            }
        });
    };

    debouncedFn.cancel = (key?: string) => {
        const doCancel = (k: string) => {
            const state = states.get(k);
            if (!state || state.status === 'idle') return;
            if (state.timer) clearTimeout(state.timer);

            if (leading) {
                // leading 模式下 reject 队列中所有等待的调用方
                rejectLeadingQueue(state, new Error('Cancelled'));
            } else {
                // trailing 模式下 pending 不直接 reject，避免调用方未 await 时触发 unhandled rejection
                // Promise 成为孤儿，由 GC 回收
            }
            states.delete(k);
        };
        if (key) {
            doCancel(key);
        } else {
            Array.from(states.keys()).forEach(doCancel);
        }
    };

    debouncedFn.flush = (key?: string) => {
        const doFlush = (k: string) => {
            const state = states.get(k);
            if (!state) return;

            if (leading) {
                if (state.status === 'inflight') {
                    if (state.timer) clearTimeout(state.timer);
                    onLeadingDelayExpired(k, state);
                }
            } else {
                if (state.status === 'pending') {
                    if (state.timer) clearTimeout(state.timer);
                    state.status = 'inflight';
                    state.timer = undefined;
                    executeRequest(k, state.args!, state);
                }
            }
        };
        if (key) {
            doFlush(key);
        } else {
            Array.from(states.keys()).forEach(doFlush);
        }
    };

    debouncedFn.getPendingCount = () => {
        return Array.from(states.values()).filter(s => s.status !== 'idle').length;
    };

    debouncedFn.getPendingKeys = () => {
        return Array.from(states.entries())
            .filter(([, s]) => s.status !== 'idle')
            .map(([k]) => k);
    };

    return debouncedFn;
}
