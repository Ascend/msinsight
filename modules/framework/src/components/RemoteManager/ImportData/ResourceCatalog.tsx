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

import React, { useEffect, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import { Tree, type TreeDataNode } from 'antd';
import type { EventDataNode } from 'antd/lib/tree';
import { getFiles, type FileSortBy, type FileSortOrder, type GetFilesOptions } from '@/utils/Request';
import { dealResource, ResourceItem, updateTreeData } from '@/utils/Resource';
import { catalogReloadKind, parentDirectory, shouldApplyUserSort } from './importCatalog';

const ResourceCataologContainer = styled.div`
    margin-top: 24px;
    flex: 0 1 500px;
    min-height: 100px;
    overflow-y: auto;
    background: ${(props): string => props.theme.bgColorDark};
    padding: 10px 0;
    .ant-tree{
        width: max-content;
        min-width: 100%;
    }
    .ant-tree-list {
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorPrimary};
    }

    // 折叠展开图标
    .ant-tree-switcher {
        color: ${(props): string => props.theme.textColorPlaceholder};
    }

    // 目录项
    .ant-tree.ant-tree-block-node .ant-tree-list-holder-inner .ant-tree-node-content-wrapper {
        flex: 1 1 auto;
        display: flex;
        align-items: center;
    }
    .ant-tree .ant-tree-node-content-wrapper:hover {
        background: none;
    }
    .ant-tree-treenode:hover {
        background: ${(props): string => props.theme.bgColorLight};
    }
    .ant-tree .ant-tree-node-content-wrapper.ant-tree-node-selected {
        background: none;
    }
    .ant-tree-treenode.ant-tree-treenode-selected {
        background: ${(props): string => props.theme.bgColorLight};
    }
    .ant-tree-title {
        white-space: nowrap;
    }
`;
export enum CatalogAction {
    SEARCH = 'search',
    REFRESH = 'refresh',
    INPUT_PATH_CHANGE = 'input path change',
    NO_ACTION = 'no action',
}
export interface CatalogActionListener {
    type: CatalogAction;
    value?: string;
    /** 每次显式搜索/刷新递增，避免同路径被内容签名跳过 */
    nonce?: number;
}

export enum SearchReturnMessage {
    EXCEEDING_MAX_DEPETH = 'ExceedingMaxDepth',
    FILE_NOT_FOUND = 'FileNotFundDescribe',
}

export interface SearchResult {
    success: boolean;
    result?: {
        message: SearchReturnMessage;
        options?: Record<string, string | number>;
    };
}
interface IResourceProps {
    actionListener: CatalogActionListener;
    sortBy: FileSortBy;
    order: FileSortOrder;
    onSearchReturnChange: (val: SearchResult) => void;
    onSelectedChange: (val: string) => void;
}
const STABLE_SORT: GetFilesOptions = { sortBy: 'name', order: 'asc' };
// 文件目录
const ResourceCatalog = observer(({ actionListener, sortBy, order, onSearchReturnChange, onSelectedChange }: IResourceProps) => {
    const [treeData, setTreeData] = useState<TreeDataNode[]>([]);
    const [selectedPath, setSelectedPath] = useState<React.Key>('');
    const [expandedKeys, setExpandedKeys] = useState<React.Key[]>([]);
    const selectedKeys = useMemo(() => selectedPath === '' ? [] : [selectedPath], [selectedPath]);
    const sortOptions = useMemo(() => ({ sortBy, order }), [sortBy, order]);
    const sortOptionsRef = useRef(sortOptions);
    sortOptionsRef.current = sortOptions;
    const requestEpoch = useRef(0);
    // 上次成功选中的路径。排序重载用它，不用输入框里尚未确认的内容
    const resolvedTargetRef = useRef('');
    // 当前应用用户排序的目录（文件焦点时为其父目录）
    const sortFocusRef = useRef('');
    const confirmedSearchRef = useRef('');

    const resolveSortOptions = (dirPath: string, focusPath: string): GetFilesOptions => {
        return shouldApplyUserSort(dirPath, focusPath) ? sortOptionsRef.current : STABLE_SORT;
    };

    const rememberFocus = (path: string, isFile: boolean): string => {
        resolvedTargetRef.current = path;
        const focus = isFile ? parentDirectory(path) : path;
        sortFocusRef.current = focus;
        return focus;
    };

    // 加载子节点；epoch 不匹配说明排序或搜索已切换，丢弃过期响应
    // stable=true 时强制 name/asc，仅用于首次展开尚未加载的祖先
    const onLoadData = async(node: TreeDataNode | undefined, focusPath: string, epoch: number, stable = false): Promise<TreeDataNode[]> => {
        const dirPath = String(node?.key ?? '');
        const data = await getFiles(dirPath, stable ? STABLE_SORT : resolveSortOptions(dirPath, focusPath)) as ResourceItem;
        if (epoch !== requestEpoch.current) {
            return [];
        }
        const newTreeData = dealResource(data);
        if (!node) {
            setTreeData(newTreeData);
        } else {
            setTreeData(origin => updateTreeData(origin, node.key, newTreeData));
        }
        return newTreeData;
    };

    const reportSearch = (result: SearchResult, quiet: boolean): void => {
        if (!result.success && quiet) {
            return;
        }
        onSearchReturnChange(result);
    };

    // 查询并跳转到对应文件
    // Recursive，递归函数
    const searchPath = async(
        searchText: string = '',
        curLevelTree: TreeDataNode[] = [],
        historyRouteKeys: React.Key[] = [],
        depth = 0,
        epoch = 0,
        quiet = false,
    ): Promise<void> => {
        if (depth === 0) {
            epoch = ++requestEpoch.current;
        }
        if (epoch !== requestEpoch.current) {
            return;
        }
        const maxDepth = 100;
        if (depth > maxDepth) {
            reportSearch({ success: false, result: { message: SearchReturnMessage.EXCEEDING_MAX_DEPETH, options: { maxDepth } } }, quiet);
            return;
        }
        if (depth === 0) {
            const nextLevelTree = await onLoadData(undefined, searchText, epoch, true);
            if (epoch !== requestEpoch.current) {
                return;
            }
            await searchPath(searchText, nextLevelTree, [], 1, epoch, quiet);
            return;
        }
        if (searchText === '') {
            setExpandedKeys([]);
            setSelectedPath('');
            resolvedTargetRef.current = '';
            sortFocusRef.current = '';
            reportSearch({ success: true }, quiet);
            return;
        }
        const routeKeys: React.Key[] = historyRouteKeys;
        for (let i = 0; i < curLevelTree.length; i++) {
            const node = curLevelTree[i];
            if (node.key === searchText) {
                const isFile = Boolean(node.isLeaf);
                const sortFocus = rememberFocus(String(node.key), isFile);
                if (isFile) {
                    if (sortFocus) {
                        await onLoadData({ key: sortFocus } as TreeDataNode, sortFocus, epoch);
                    }
                } else {
                    await onLoadData(node, sortFocus, epoch);
                }
                if (epoch !== requestEpoch.current) {
                    return;
                }
                setExpandedKeys([...routeKeys, node.key]);
                setSelectedPath(node.key);
                setTimeout(() => {
                    const dom = document.getElementById(searchText)?.closest('div');
                    dom?.scrollIntoView({ behavior: 'smooth', block: 'center', inline: 'start' });
                }, 100);
                reportSearch({ success: true }, quiet);
                return;
            }
            const curPath = String(node.key);
            const separator = curPath.includes('/') ? '/' : '\\';
            const isParentDir = searchText.startsWith(curPath.endsWith(separator) ? curPath : `${curPath}${separator}`);
            if (isParentDir) {
                routeKeys.push(node.key);
                // 祖先已展开则复用现有顺序，避免把父层重新排成 name/asc
                const existing = node.children;
                const nextLevelTree = existing?.length
                    ? existing
                    : await onLoadData(node, searchText, epoch, true);
                if (epoch !== requestEpoch.current) {
                    return;
                }
                await searchPath(searchText, nextLevelTree, routeKeys, depth + 1, epoch, quiet);
                return;
            }
        }
        reportSearch({ success: false, result: { message: SearchReturnMessage.FILE_NOT_FOUND } }, quiet);
    };

    // 仅重排当前焦点目录，不动祖先层
    const reloadFocusSort = async(): Promise<void> => {
        const focus = sortFocusRef.current || confirmedSearchRef.current;
        const epoch = ++requestEpoch.current;
        if (!focus) {
            await onLoadData(undefined, '', epoch);
            return;
        }
        await onLoadData({ key: focus } as TreeDataNode, focus, epoch);
    };

    // 点击节点文字
    const handleSelect = (keys: React.Key[], { node }: {node: EventDataNode<TreeDataNode>}): void => {
        setSelectedPath(node.key);
        const isFile = Boolean(node.isLeaf);
        const sortFocus = rememberFocus(String(node.key), isFile);
        onSelectedChange(String(node.key));
        const newExpandedKeys = node.expanded ? expandedKeys.filter(key => key !== node.key) : [...expandedKeys, node.key];
        setExpandedKeys(newExpandedKeys);
        const epoch = requestEpoch.current;
        if (isFile) {
            if (sortFocus) {
                void onLoadData({ key: sortFocus } as TreeDataNode, sortFocus, epoch);
            }
            return;
        }
        if (!node.expanded) {
            void onLoadData(node, sortFocus, epoch);
        }
    };

    // 点击展开图标
    const handleExpand = (keys: React.Key[], { node, expanded }: {expanded: boolean;node: TreeDataNode}): void => {
        setExpandedKeys(keys);
        setSelectedPath(node.key);
        const sortFocus = rememberFocus(String(node.key), false);
        if (expanded) {
            void onLoadData(node, sortFocus, requestEpoch.current);
        }
    };

    const prevNonceRef = useRef(actionListener.nonce ?? 0);
    const prevSortSig = useRef(`${sortBy}:${order}`);

    // 操作目录树
    useEffect(() => {
        const { type, value } = actionListener;
        const nonce = actionListener.nonce ?? 0;
        const sortSig = `${sortBy}:${order}`;
        const kind = catalogReloadKind({
            prevNonce: prevNonceRef.current,
            nonce,
            type,
            prevSortSig: prevSortSig.current,
            sortSig,
        });
        prevNonceRef.current = nonce;
        prevSortSig.current = sortSig;

        if (kind === 'search') {
            confirmedSearchRef.current = value ?? '';
            void searchPath(value ?? '');
            return;
        }
        if (kind === 'sort') {
            void reloadFocusSort();
            return;
        }
        if (type === CatalogAction.INPUT_PATH_CHANGE && value !== selectedPath) {
            setSelectedPath('');
        }
    }, [actionListener, sortOptions]);

    // 选中目录变化
    useEffect(() => {
        onSelectedChange(String(selectedPath));
    }, [selectedPath]);

    return <ResourceCataologContainer>
        <Tree
            blockNode={true}
            showIcon={true}
            treeData={treeData}
            selectedKeys={selectedKeys}
            expandedKeys={expandedKeys}
            onSelect={handleSelect}
            onExpand={handleExpand}
        />
    </ResourceCataologContainer>;
});

export default ResourceCatalog;
