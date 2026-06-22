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
import { useRef, useEffect } from 'react';
import {
    createSmartDebounceRequestFunc,
    DebouncedRequestFunc,
} from '../utils/createSmartDebounceRequestFunc';

/**
 * React Hook 版本的智能防抖请求
 * 组件卸载时自动调用 cancel，避免内存泄漏和过时的状态更新
 *
 * @example
 * const queryStages = useSmartDebounceRequest(
 *     async (param) => window.requestData('communication/matrix/group', param),
 *     { delay: 300, leading: false }
 * );
 *
 * // 组件卸载时自动 cancel
 */
export function useSmartDebounceRequest<T extends (...args: any[]) => Promise<any>>(
    requestFn: T,
    options?: {
        delay?: number;
        leading?: boolean;
        keyFn?: (...args: Parameters<T>) => string;
        onBeforeRequest?: (...args: Parameters<T>) => void;
        onAfterRequest?: (result: any, ...args: Parameters<T>) => void;
    }
): DebouncedRequestFunc<T> {
    const requestFnRef = useRef(requestFn);
    requestFnRef.current = requestFn;

    const onBeforeRef = useRef(options?.onBeforeRequest);
    onBeforeRef.current = options?.onBeforeRequest;

    const onAfterRef = useRef(options?.onAfterRequest);
    onAfterRef.current = options?.onAfterRequest;

    const debouncedRef = useRef<DebouncedRequestFunc<T>>();

    if (!debouncedRef.current) {
        debouncedRef.current = createSmartDebounceRequestFunc(
            ((...args: Parameters<T>) => requestFnRef.current(...args)) as T,
            {
                delay: options?.delay,
                leading: options?.leading,
                keyFn: options?.keyFn,
                onBeforeRequest: (...args) => onBeforeRef.current?.(...args),
                onAfterRequest: (result, ...args) => onAfterRef.current?.(result, ...args),
            }
        );
    }

    useEffect(() => {
        return () => {
            debouncedRef.current?.cancel();
        };
    }, []);

    return debouncedRef.current;
}
