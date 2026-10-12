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

// 去除路径尾部分隔符；保留盘符根如 C:\
export const trimFocusPath = (path: string): string => {
    if (!path) {
        return '';
    }
    const trimmed = path.trim();
    if (/^[A-Za-z]:\\?$/i.test(trimmed) || trimmed === '/') {
        return trimmed.replace(/:\\+$/, ':\\');
    }
    return trimmed.replace(/[\\/]+$/, '');
};

// 文件焦点的排序目录是其父目录；盘符根没有父目录
export const parentDirectory = (path: string): string => {
    const focus = trimFocusPath(path);
    if (!focus || focus === '/' || /^[A-Za-z]:\\$/i.test(focus)) {
        return focus;
    }
    const slash = Math.max(focus.lastIndexOf('/'), focus.lastIndexOf('\\'));
    if (slash <= 0) {
        return focus.startsWith('/') ? '/' : '';
    }
    let parent = focus.slice(0, slash);
    if (/^[A-Za-z]:$/i.test(parent)) {
        parent = `${parent}\\`;
    }
    return parent;
};

// 文件焦点按父目录排序，目录焦点按自身及其子孙排序
export const resolveSortFocus = (focusPath: string, focusIsFile: boolean): string => {
    const focus = trimFocusPath(focusPath);
    if (!focus || !focusIsFile) {
        return focus;
    }
    return parentDirectory(focus);
};

// 仅对焦点目录及其子孙应用用户排序；祖先目录固定 name/asc，避免父级同级跳动
export const shouldApplyUserSort = (dirPath: string, focusPath: string, focusIsFile = false): boolean => {
    const focus = resolveSortFocus(focusPath, focusIsFile);
    if (!focus) {
        return true;
    }
    const path = trimFocusPath(dirPath);
    if (path === focus) {
        return true;
    }
    const separator = focus.includes('/') && !focus.includes('\\') ? '/' : '\\';
    const prefix = focus.endsWith(separator) ? focus : `${focus}${separator}`;
    return path.startsWith(prefix);
};

export type CatalogActionKind = 'search' | 'refresh' | 'input path change' | 'no action';

// 显式搜索/刷新看 nonce，不看路径内容；排序变化单独比较
export const catalogReloadKind = (params: {
    prevNonce: number;
    nonce: number;
    type: CatalogActionKind | string;
    prevSortSig: string;
    sortSig: string;
}): 'search' | 'sort' | 'none' => {
    const explicit = params.nonce !== params.prevNonce &&
        (params.type === 'search' || params.type === 'refresh');
    if (explicit) {
        return 'search';
    }
    if (params.sortSig !== params.prevSortSig) {
        return 'sort';
    }
    return 'none';
};
