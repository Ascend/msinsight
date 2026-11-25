/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import fs from 'fs';
import path from 'path';

const modulesRoot = path.resolve(__dirname, '../../..');
const files = (directory: string): string[] => fs.readdirSync(directory, { withFileTypes: true }).flatMap(entry => {
    const file = path.join(directory, entry.name);
    return entry.isDirectory() ? files(file) : /\.tsx?$/.test(file) && !file.includes('.test.') ? [file] : [];
});
const dependencies = (file: string): string[] => [...fs.readFileSync(file, 'utf8').matchAll(/(?:from\s*|import\s*|import\(\s*)['"]([^'"]+)['"]/g)].map(([, specifier]) =>
    specifier.startsWith('.') ? path.resolve(path.dirname(file), specifier) : specifier,
);

describe('independent application boundaries', () => {
    it.each(['memscope', 'memsnapshot'])('%s imports only its own source and existing infrastructure', application => {
        const forbidden = ['leaks', 'memscope', 'memsnapshot', 'memory-common'].filter(name => name !== application).map(name => path.join(modulesRoot, name, 'src'));
        const violations = files(path.join(modulesRoot, application, 'src')).flatMap(file => dependencies(file)
            .filter(dependency => forbidden.some(prefix => dependency.startsWith(prefix)) || /^@insight\/(leaks|memscope|memsnapshot|memory-common)(\/|$)/.test(dependency))
            .map(dependency => `${file} -> ${dependency}`));
        expect(violations).toEqual([]);
    });
    it('has no separate memory-common package or configuration dependency', () => {
        expect(fs.existsSync(path.join(modulesRoot, 'memory-common'))).toBe(false);
        for (const application of ['memscope', 'memsnapshot']) {
            for (const file of ['package.json', 'tsconfig.json', 'craco.config.js']) {
                expect(fs.readFileSync(path.join(modulesRoot, application, file), 'utf8')).not.toContain('memory-common');
            }
        }
        expect(fs.readFileSync(path.join(modulesRoot, 'pnpm-workspace.yaml'), 'utf8')).not.toContain('memory-common');
        expect(fs.readFileSync(path.join(modulesRoot, 'pnpm-lock.yaml'), 'utf8')).not.toContain('memory-common');
    });
});

it('has no legacy frontend module or artifact distribution target', () => {
    expect(fs.existsSync(path.join(modulesRoot, 'leaks'))).toBe(false);
    expect(fs.readFileSync(path.join(modulesRoot, 'pnpm-workspace.yaml'), 'utf8')).not.toContain('  - \'leaks\'');
    expect(fs.readFileSync(path.join(modulesRoot, 'pnpm-lock.yaml'), 'utf8')).not.toContain('  leaks:');
    expect(fs.readFileSync(path.join(modulesRoot, 'sync.mjs'), 'utf8')).not.toContain('folder: \'leaks\'');
});
