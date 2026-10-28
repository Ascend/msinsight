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

type ConfigModule = typeof import('./defaultBrowseDirectory');
let config: ConfigModule;

const href = 'https://example.test/base/resources/profiler/frontend/index.html?port=9000&profilerServerId=instance-1';
const endpoint = 'https://example.test/base/mindstudio_insight_jupyterlab/file_selector_config';
const warning = 'Prefetch Insight file selector configuration failed:';
const response = (body: unknown = { defaultBrowseDir: '/data/profiling' }, status = 200): Response => ({
    ok: status >= 200 && status < 300,
    status,
    json: jest.fn().mockResolvedValue(body),
} as unknown as Response);
const createFetcher = (): jest.MockedFunction<typeof fetch> => jest.fn();

beforeEach(() => {
    // Separate module instances model separate iframes without a production cache-reset API.
    jest.isolateModules(() => { config = require('./defaultBrowseDirectory'); });
    jest.useFakeTimers();
    jest.spyOn(console, 'warn').mockImplementation(jest.fn());
});

afterEach(() => {
    jest.restoreAllMocks();
    jest.clearAllTimers();
    jest.useRealTimers();
});

describe('Insight file selector configuration URL', () => {
    it.each([
        ['http://localhost:8888', ''],
        ['https://example.test', '/base'],
        ['https://example.test', '/space%20name/nested'],
    ])('preserves the origin and base_url: %s%s', (origin, base) => {
        const url = `${origin}${base}/resources/profiler/frontend/index.html?profilerServerId=1&port=9000&jupyterlabProxy=true&token=secret#view`;
        expect(config.getInsightFileSelectorConfigUrl(url)).toBe(`${origin}${base}/mindstudio_insight_jupyterlab/file_selector_config`);
    });

    it.each([
        'wry://localhost/resources/profiler/frontend/index.html?port=9000',
        'http://wry.localhost/resources/profiler/frontend/index.html?port=9000',
        'file:///opt/insight/resources/profiler/frontend/index.html?profilerServerId=1',
        'http://localhost:9000/resources/profiler/frontend/index.html?port=9000',
        'http://localhost:5174/index.html?port=9000',
        'https://example.test/proxy/9000/index.html?proxy=true',
        'vscode-webview://example/index.html?port=9000',
        'https://example.vscode-cdn.net/extension/index.html?port=9000',
        'https://example.test/resources/profiler/frontend/index.html?profilerServerId=',
        'https://example.test/other/index.html?profilerServerId=1',
    ])('does not fetch or schedule a timeout for a non-Jupyter entry: %s', async url => {
        const fetcher = createFetcher();
        const timeout = jest.spyOn(window, 'setTimeout');
        expect(config.getInsightFileSelectorConfigUrl(url)).toBeUndefined();
        config.prefetchInsightFileSelectorDefaultDirectory(url, fetcher);
        await expect(config.loadInsightFileSelectorDefaultDirectory(url, fetcher)).resolves.toBe('');
        expect(fetcher).not.toHaveBeenCalled();
        expect(timeout).not.toHaveBeenCalled();
        expect(jest.getTimerCount()).toBe(0);
        expect(console.warn).not.toHaveBeenCalled();
    });
});

describe('Insight file selector configuration requests', () => {
    it('uses the authenticated configuration endpoint and clears the timeout on success', async () => {
        const fetcher = createFetcher().mockResolvedValue(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(1);
        expect(fetcher).toHaveBeenCalledWith(endpoint, {
            credentials: 'same-origin', cache: 'no-store', redirect: 'error', signal: expect.any(AbortSignal),
        });
        expect(jest.getTimerCount()).toBe(0);
        jest.advanceTimersByTime(5000);
        expect(fetcher.mock.calls[0][1]?.signal?.aborted).toBe(false);
    });

    it('uses window.location and window.fetch when arguments are omitted', async () => {
        const originalUrl = window.location.href;
        const fetchDescriptor = Object.getOwnPropertyDescriptor(window, 'fetch');
        const fetcher = createFetcher().mockResolvedValue(response());
        try {
            window.history.replaceState({}, '', '/prefix/resources/profiler/frontend/index.html?profilerServerId=1');
            Object.defineProperty(window, 'fetch', { configurable: true, writable: true, value: fetcher });
            config.prefetchInsightFileSelectorDefaultDirectory();
            await expect(config.loadInsightFileSelectorDefaultDirectory()).resolves.toBe('/data/profiling');
            expect(fetcher).toHaveBeenCalledTimes(1);
            expect(fetcher.mock.calls[0][0]).toBe(`${window.location.origin}/prefix/mindstudio_insight_jupyterlab/file_selector_config`);
        } finally {
            window.history.replaceState({}, '', originalUrl);
            if (fetchDescriptor) {
                Object.defineProperty(window, 'fetch', fetchDescriptor);
            } else {
                Reflect.deleteProperty(window, 'fetch');
            }
        }
    });

    it.each([400, 401, 403, 422, 500])('rejects HTTP %s and permits a later retry', async status => {
        const fetcher = createFetcher().mockResolvedValueOnce(response(null, status)).mockResolvedValueOnce(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher))
            .rejects.toThrow('Cannot read the Insight file selector default directory configuration');
        expect(jest.getTimerCount()).toBe(0);
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(2);
    });

    it.each([null, [], {}, 123, 'text', { defaultBrowseDir: 123 }])('rejects a malformed payload: %j', async body => {
        const fetcher = createFetcher().mockResolvedValueOnce(response(body)).mockResolvedValueOnce(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher))
            .rejects.toThrow('Invalid Insight file selector configuration');
        expect(jest.getTimerCount()).toBe(0);
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(2);
    });

    it('clears the failed request when JSON parsing rejects', async () => {
        const error = new SyntaxError('Invalid JSON');
        const invalidResponse = response();
        jest.mocked(invalidResponse.json).mockRejectedValue(error);
        const fetcher = createFetcher().mockResolvedValueOnce(invalidResponse).mockResolvedValueOnce(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).rejects.toBe(error);
        expect(jest.getTimerCount()).toBe(0);
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(2);
    });

    it('aborts only at the timeout and allows another request afterwards', async () => {
        const fetcher = createFetcher().mockImplementation((_url, init) => new Promise((_resolve, reject) => {
            init?.signal?.addEventListener('abort', () => reject(new DOMException('Aborted', 'AbortError')));
        }));
        const failure = expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).rejects.toMatchObject({ name: 'AbortError' });
        const signal = fetcher.mock.calls[0][1]?.signal;
        jest.advanceTimersByTime(4999);
        expect(signal?.aborted).toBe(false);
        jest.advanceTimersByTime(1);
        await failure;
        expect(signal?.aborted).toBe(true);
        expect(jest.getTimerCount()).toBe(0);
        fetcher.mockResolvedValueOnce(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(2);
        expect(jest.getTimerCount()).toBe(0);
    });
});

describe('Insight file selector prefetch and iframe cache', () => {
    it('shares a pending prefetch and retains its successful result', async () => {
        let resolveRequest!: (result: Response) => void;
        const fetcher = createFetcher().mockReturnValue(new Promise(resolve => { resolveRequest = resolve; }));
        expect(config.prefetchInsightFileSelectorDefaultDirectory(href, fetcher)).toBeUndefined();
        config.prefetchInsightFileSelectorDefaultDirectory(href, fetcher);
        const reads = [
            config.loadInsightFileSelectorDefaultDirectory(href, fetcher),
            config.loadInsightFileSelectorDefaultDirectory(href, fetcher),
        ];
        expect(fetcher).toHaveBeenCalledTimes(1);
        expect(jest.getTimerCount()).toBe(1);
        resolveRequest(response());
        await expect(Promise.all(reads)).resolves.toEqual(['/data/profiling', '/data/profiling']);
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(1);
        expect(jest.getTimerCount()).toBe(0);
        expect(console.warn).not.toHaveBeenCalled();
    });

    it.each([200, 404])('caches an empty configuration or legacy HTTP %s fallback', async status => {
        const result = response({ defaultBrowseDir: '' }, status);
        const fetcher = createFetcher().mockResolvedValue(result);
        config.prefetchInsightFileSelectorDefaultDirectory(href, fetcher);
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('');
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('');
        expect(fetcher).toHaveBeenCalledTimes(1);
        expect(result.json).toHaveBeenCalledTimes(status === 404 ? 0 : 1);
        expect(jest.getTimerCount()).toBe(0);
        expect(console.warn).not.toHaveBeenCalled();
    });

    it('logs prefetch failure, rejects waiting reads and does not automatically retry', async () => {
        let rejectRequest!: (reason: Error) => void;
        const error = new Error('Network unavailable');
        const fetcher = createFetcher().mockReturnValueOnce(new Promise((_resolve, reject) => { rejectRequest = reject; }));
        config.prefetchInsightFileSelectorDefaultDirectory(href, fetcher);
        const failure = expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).rejects.toBe(error);
        rejectRequest(error);
        await failure;
        expect(console.warn).toHaveBeenCalledTimes(1);
        expect(console.warn).toHaveBeenCalledWith(warning, error);
        expect(jest.getTimerCount()).toBe(0);
        jest.advanceTimersByTime(10000);
        expect(fetcher).toHaveBeenCalledTimes(1);
        fetcher.mockResolvedValueOnce(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        expect(fetcher).toHaveBeenCalledTimes(2);
        expect(console.warn).toHaveBeenCalledTimes(1);
    });

    it('keeps configuration caches isolated between module instances', async () => {
        const fetcher = createFetcher()
            .mockResolvedValueOnce(response({ defaultBrowseDir: '/first' }))
            .mockResolvedValueOnce(response({ defaultBrowseDir: '/second' }));
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/first');
        let other!: ConfigModule;
        jest.isolateModules(() => { other = require('./defaultBrowseDirectory'); });
        await expect(other.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/second');
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/first');
        expect(fetcher).toHaveBeenCalledTimes(2);
    });

    it('does not apply cached Jupyter configuration to a non-Jupyter URL', async () => {
        const fetcher = createFetcher().mockResolvedValue(response());
        await expect(config.loadInsightFileSelectorDefaultDirectory(href, fetcher)).resolves.toBe('/data/profiling');
        await expect(config.loadInsightFileSelectorDefaultDirectory('wry://localhost/index.html', fetcher)).resolves.toBe('');
        expect(fetcher).toHaveBeenCalledTimes(1);
    });
});
