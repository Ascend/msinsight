/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
const fs = require('fs');
const path = require('path');

const buildDir = path.join(process.cwd(), 'build');

if (!fs.existsSync(buildDir)) {
    console.error('build directory not found at', buildDir);
    process.exit(1);
}

const htmlFiles = fs.readdirSync(buildDir).filter((f) => f.endsWith('.html'));

if (htmlFiles.length === 0) {
    console.error('No HTML files found in build directory');
    process.exit(1);
}

const filesToRemove = [];

htmlFiles.forEach((htmlFile) => {
    const htmlPath = path.join(buildDir, htmlFile);
    let html = fs.readFileSync(htmlPath, 'utf8');

    // Inline CSS
    html = html.replace(/<link[^>]+\bhref="([^"]+\.css)"[^>]*>/g, (match, href) => {
        const cssPath = path.join(buildDir, href);
        if (fs.existsSync(cssPath)) {
            filesToRemove.push(cssPath);
            const css = fs.readFileSync(cssPath, 'utf8');
            return '<style>' + css + '</style>';
        }
        return match;
    });

    // Inline JS - replace script tag with inline content
    html = html.replace(/<script[^>]+\bsrc="([^"]+\.js)"[^>]*><\/script>/g, (match, href) => {
        const jsPath = path.join(buildDir, href);
        if (fs.existsSync(jsPath)) {
            filesToRemove.push(jsPath);
            const js = fs.readFileSync(jsPath, 'utf8');
            return '<script>' + js + '</script>';
        }
        return match;
    });

    // Move inline <script> tags from <head> to end of <body>
    const headBodyMatch = html.match(/^([\s\S]*?<\/head>)([\s\S]*<\/body>[\s\S]*)$/i);
    if (headBodyMatch) {
        let head = headBodyMatch[1];
        let body = headBodyMatch[2];
        const scriptRegex = /<script>[\s\S]*?<\/script>/g;
        const scripts = head.match(scriptRegex) || [];
        if (scripts.length > 0) {
            head = head.replace(scriptRegex, '');
            const bodyCloseIdx = body.lastIndexOf('</body>');
            if (bodyCloseIdx !== -1) {
                body = body.slice(0, bodyCloseIdx) + scripts.join('') + body.slice(bodyCloseIdx);
            }
            html = head + body;
        }
    }

    fs.writeFileSync(htmlPath, html);
    console.log('[inline] ' + htmlFile + ' - assets merged');
});

// Only remove files that were actually inlined
filesToRemove.forEach((f) => {
    if (fs.existsSync(f)) fs.unlinkSync(f);
});

// Remove orphan LICENSE files for inlined JS
const licenseFiles = fs.readdirSync(buildDir).filter((f) => f.endsWith('.js.LICENSE.txt'));
licenseFiles.forEach((f) => fs.unlinkSync(path.join(buildDir, f)));

// Clean up empty static directories
const staticDir = path.join(buildDir, 'static');
if (fs.existsSync(staticDir)) {
    ['js', 'css', 'media'].forEach((sub) => {
        const subDir = path.join(staticDir, sub);
        if (fs.existsSync(subDir) && fs.readdirSync(subDir).length === 0) {
            fs.rmdirSync(subDir);
        }
    });
    if (fs.readdirSync(staticDir).length === 0) {
        fs.rmdirSync(staticDir);
        console.log('[inline] static directory removed (empty)');
    } else {
        const jsCount = fs.existsSync(path.join(staticDir, 'js')) ? fs.readdirSync(path.join(staticDir, 'js')).length : 0;
        const cssCount = fs.existsSync(path.join(staticDir, 'css')) ? fs.readdirSync(path.join(staticDir, 'css')).length : 0;
        console.log('[inline] remaining async chunks: js=' + jsCount + ' css=' + cssCount);
    }
}

console.log('[inline] completed');
