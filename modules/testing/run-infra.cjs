/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const files = fs.readdirSync(__dirname, { withFileTypes: true })
    .filter(entry => entry.isFile() && entry.name.endsWith('.test.cjs'))
    .map(entry => path.join(__dirname, entry.name)).sort();

if (files.length === 0) {
    console.error('No infrastructure tests found.');
    process.exitCode = 1;
} else {
    // Explicit file arguments also work on Node 18 and shells without glob expansion.
    const result = spawnSync(process.execPath, ['--test', ...files], {
        cwd: path.resolve(__dirname, '..'),
        stdio: 'inherit',
        shell: false,
    });
    if (result.error) console.error(result.error.message);
    process.exitCode = result.status ?? 1;
}
