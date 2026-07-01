import { defineConfig, devices } from '@playwright/test';
import os from 'os';
import * as path from 'path';

/**
 * Read environment variables from file.
 * https://github.com/motdotla/dotenv
 */

/**
 * See https://playwright.dev/docs/test-configuration.
 */
const isCI = !!process.env.CI;
const getProfilerServerCmd = (port = 9000) => {
    const platform = os.platform();
    let serverBinDir = '../server/output/';
    let profilerServer = 'profiler_server';
    let logPath = path.resolve(__dirname, 'log/');
    if (platform === 'win32') {
        // windows平台当前固定使用mingw进行编译
        serverBinDir = path.join(__dirname, serverBinDir, 'win_mingw64/bin/');
        profilerServer += '.exe';
        logPath = 'C:\\msinsight-log';
    } else if (platform === 'darwin') {
        // mac平台使用apple clang进行编译
        serverBinDir += 'darwin/bin/';
    } else if (platform === 'linux') {
        // linux平台使用gcc编译，但arch不同输出目录不同
        const arch = os.arch() === 'x64' ? 'x86_64' : 'aarch64';
        serverBinDir += `linux-${arch}/bin/`;
    } else {
        throw new Error(`Unsupported platform: ${platform}`);
    }

    return `${serverBinDir}${profilerServer} --wsPort=${port} --wsHost=127.0.0.1 --logPath=${logPath}`;
};
export default defineConfig({
    testDir: './src/tests',
    /* Maximum time one test can run for. */
    timeout: 60 * 1000,
    expect: {
        /**
         * Maximum time expect() should wait for the condition to be met.
         * For example in `await expect(locator).toHaveText();`
         */
        timeout: 5000,
    },
    /* Fail the build on CI if you accidentally left test.only in the source code. */
    forbidOnly: !!process.env.CI,
    /* No retry */
    retries: 0,
    /* Opt out of parallel tests on CI. */
    workers: process.env.CI ? 1 : 1,
    /* Reporter to use. See https://playwright.dev/docs/test-reporters */
    reporter: [
        ['html', { open: 'never' }],
        [path.resolve(__dirname, 'src/utils/myReporter.ts')],
    ],
    /* Shared settings for all the projects below. See https://playwright.dev/docs/api/class-testoptions. */
    use: {
        /* Maximum time each action such as `click()` can take. Defaults to 0 (no limit). */
        actionTimeout: 0,
        /* Base URL to use in actions like `await page.goto('/')`. */
        baseURL: 'http://localhost:5174',
        /* Collect trace when retrying the failed test. See https://playwright.dev/docs/trace-viewer */
        trace: 'on-first-retry',
        /* Only on CI systems run the tests headless */
        headless: true,
        launchOptions: {
            args: [
                '--use-gl=swiftshader',
                '--disable-gpu-compositing',
                '--disable-webgl',
            ],
        },
    },

    /* Configure projects for major browsers */
    projects: [
        {
            use: {
                ...devices['Desktop Chrome'],
                viewport: { width: 1920, height: 1080 },
            },
        },
    ],
    /* Run your local dev server before starting the tests */
    webServer: [
        {
            command: getProfilerServerCmd(9000),
            reuseExistingServer: !isCI,
        },
        {
            /**
             * Use the dev server by default for faster feedback loop.
             * Use the preview server on CI for more realistic testing.
             * Playwright will re-use the local server if there is already a dev-server running.
             */
            command: 'npm run staging',
            cwd: path.resolve(__dirname, '../modules/framework'),
            url: 'http://localhost:5174',
            reuseExistingServer: !isCI,
        },
    ],
});
