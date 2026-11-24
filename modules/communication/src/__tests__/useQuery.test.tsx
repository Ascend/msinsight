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
import { act, renderHook } from '@testing-library/react';
import * as testing from '@testing-library/react';
import { useQuery } from '../state/useQuery';
const { waitFor } = testing;
it('discards old success and failure even when the transport ignores cancellation', async () => {
    const pending: Array<{ resolve: (value: string) => void; reject: (error: Error) => void; signal: AbortSignal }> = [];
    const { result, rerender } = renderHook(({ key }) => useQuery(key, true, signal => new Promise<string>((resolve, reject) => pending.push({ resolve, reject, signal }))), { initialProps: { key: 'first' } });
    await waitFor(() => expect(pending).toHaveLength(1)); rerender({ key: 'second' });
    await waitFor(() => expect(pending).toHaveLength(2)); expect(pending[0].signal.aborted).toBe(true);
    await act(async () => pending[1].resolve('current')); expect(result.current.data).toBe('current');
    await act(async () => pending[0].reject(new Error('stale'))); expect(result.current.data).toBe('current'); expect(result.current.error).toBeUndefined();
});
it('hides old data when disabled or its scope changes', async () => {
    const load = jest.fn().mockResolvedValue('old');
    const { result, rerender } = renderHook(({ enabled }) => useQuery('key', enabled, load), { initialProps: { enabled: true } });
    await waitFor(() => expect(result.current.data).toBe('old')); rerender({ enabled: false });
    expect(result.current.data).toBeUndefined(); expect(result.current.loading).toBe(false);
});
