/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const fs = require('node:fs');
const path = require('node:path');

module.exports = class RunSummary {
    onRunComplete(_contexts, results) {
        const directory = path.resolve(__dirname, '../test-results');
        fs.mkdirSync(directory, { recursive: true });
        fs.writeFileSync(path.join(directory, 'run-summary.json'), JSON.stringify({
            testFiles: results.testResults.map(result => result.testFilePath),
            passed: results.numPassedTests,
            failed: results.numFailedTests,
            failedSuites: results.numFailedTestSuites,
            skipped: results.numPendingTests,
        }, null, 2));
    }
};
