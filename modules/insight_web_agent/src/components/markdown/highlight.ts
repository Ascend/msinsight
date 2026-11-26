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
import hljs from 'highlight.js/lib/core';
import bash from 'highlight.js/lib/languages/bash';
import c from 'highlight.js/lib/languages/c';
import cpp from 'highlight.js/lib/languages/cpp';
import csharp from 'highlight.js/lib/languages/csharp';
import css from 'highlight.js/lib/languages/css';
import diff from 'highlight.js/lib/languages/diff';
import dockerfile from 'highlight.js/lib/languages/dockerfile';
import go from 'highlight.js/lib/languages/go';
import java from 'highlight.js/lib/languages/java';
import javascript from 'highlight.js/lib/languages/javascript';
import json from 'highlight.js/lib/languages/json';
import markdown from 'highlight.js/lib/languages/markdown';
import perl from 'highlight.js/lib/languages/perl';
import php from 'highlight.js/lib/languages/php';
import plaintext from 'highlight.js/lib/languages/plaintext';
import powershell from 'highlight.js/lib/languages/powershell';
import python from 'highlight.js/lib/languages/python';
import ruby from 'highlight.js/lib/languages/ruby';
import rust from 'highlight.js/lib/languages/rust';
import sql from 'highlight.js/lib/languages/sql';
import typescript from 'highlight.js/lib/languages/typescript';
import xml from 'highlight.js/lib/languages/xml';
import yaml from 'highlight.js/lib/languages/yaml';

const LANGUAGES = {
    bash,
    c,
    cpp,
    csharp,
    css,
    diff,
    dockerfile,
    go,
    java,
    javascript,
    json,
    markdown,
    perl,
    php,
    plaintext,
    powershell,
    python,
    ruby,
    rust,
    sql,
    typescript,
    xml,
    yaml,
};

Object.entries(LANGUAGES).forEach(([name, definition]) => {
    hljs.registerLanguage(name, definition);
});

const ALIASES: Record<string, string> = {
    sh: 'bash',
    shell: 'bash',
    zsh: 'bash',
    py: 'python',
    js: 'javascript',
    jsx: 'javascript',
    ts: 'typescript',
    tsx: 'typescript',
    yml: 'yaml',
    md: 'markdown',
    'c++': 'cpp',
    cc: 'cpp',
    h: 'cpp',
    hpp: 'cpp',
    cs: 'csharp',
    html: 'xml',
    svg: 'xml',
    text: 'plaintext',
    txt: 'plaintext',
    plain: 'plaintext',
};

const HTML_ESCAPES: Record<string, string> = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    '\'': '&#39;',
};

export const escapeHtml = (value: string): string => value.replace(/[&<>"']/g, character => HTML_ESCAPES[character]);

export const normalizeLanguage = (raw?: string): string | undefined => {
    if (!raw) return undefined;
    const key = raw.trim().toLowerCase();
    if (!key) return undefined;
    const resolved = ALIASES[key] ?? key;
    return Object.prototype.hasOwnProperty.call(LANGUAGES, resolved) ? resolved : undefined;
};

export const displayLanguage = (raw?: string): string => normalizeLanguage(raw) ?? 'text';

export const highlightCode = (code: string, raw?: string): string => {
    const language = normalizeLanguage(raw);
    if (!language) return escapeHtml(code);
    try {
        return hljs.highlight(code, { language, ignoreIllegals: true }).value;
    } catch {
        return escapeHtml(code);
    }
};
