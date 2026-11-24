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
import { useEffect, useRef, useState } from 'react';
export interface Query<T> { data?: T; error?: Error; loading: boolean; retry: () => void }
export function useQuery<T>(key: string, enabled: boolean, load: (signal: AbortSignal) => Promise<T>): Query<T> {
    const loader = useRef(load);
    loader.current = load;
    const [attempt, setAttempt] = useState(0);
    const [result, setResult] = useState<{ key: string; data?: T; error?: Error; loading: boolean }>({ key: '', loading: false });
    useEffect(() => {
        if (!enabled) { setResult({ key: '', loading: false }); return; }
        const controller = new AbortController();
        const currentLoad = loader.current;
        setResult({ key, loading: true });
        void Promise.resolve().then(async () => currentLoad(controller.signal)).then(data => {
            if (!controller.signal.aborted) { setResult({ key, data, loading: false }); }
        }, (error: unknown) => {
            if (!controller.signal.aborted) { setResult({ key, error: error instanceof Error ? error : new Error(String(error)), loading: false }); }
        });
        return () => controller.abort();
    }, [key, enabled, attempt]);
    const current = enabled && result.key === key ? result : { loading: enabled };
    return { ...current, retry: () => setAttempt(value => value + 1) };
}
