/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const path = require('node:path');

const metrics = ['statements', 'branches', 'functions', 'lines'];

function summarizeModules(coverage, modules, expectedFiles = []) {
    const missing = expectedFiles.filter(file => !Object.hasOwn(coverage, file));
    if (missing.length) throw new Error(`Missing source coverage (${missing.length} files): ${missing.join(', ')}`);
    const result = {};
    for (const module of modules) {
        const files = Object.entries(coverage).filter(([file]) => {
            if (file === 'total') return false;
            const relative = path.relative(module.rootDir, file);
            return relative.startsWith(`src${path.sep}`);
        });
        if (!files.length) throw new Error(`Missing coverage for ${module.name}; refusing an incomplete report.`);
        const summary = { files: files.length };
        for (const metric of metrics) {
            const total = files.reduce((sum, [, entry]) => sum + entry[metric].total, 0);
            const covered = files.reduce((sum, [, entry]) => sum + entry[metric].covered, 0);
            summary[metric] = { total, covered, pct: total ? Math.floor(covered / total * 10000) / 100 : 100 };
        }
        result[module.name] = summary;
    }
    return result;
}

module.exports = { summarizeModules };
