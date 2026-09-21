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

import React, { useEffect, useMemo, useState } from 'react';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import { Tree, type TreeDataNode } from 'antd';
import type { EventDataNode } from 'antd/lib/tree';
import { getFiles } from '@/utils/Request';
import { dealResource, ResourceItem, updateTreeData } from '@/utils/Resource';

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
    onSearchReturnChange: (val: SearchResult) => void;
    onSelectedChange: (val: string) => void;
}
// 文件目录
const ResourceCatalog = observer(({ actionListener, onSearchReturnChange, onSelectedChange }: IResourceProps) => {
    const [treeData, setTreeData] = useState<TreeDataNode[]>([]);
    const [selectedPath, setSelectedPath] = useState<React.Key>('');
    const [expandedKeys, setExpandedKeys] = useState<React.Key[]>([]);
    const selectedKeys = useMemo(() => selectedPath === '' ? [] : [selectedPath], [selectedPath]);

    // 查询并跳转到对应文件
    // Recursive，递归函数
    const searchPath = async(searchText: string = '', curLevelTree: TreeDataNode[] = [], historyRouteKeys: React.Key[] = [], depth = 0): Promise<void> => {
        // 安全防护:最大查询深度
        const maxDepth = 100;
        if (depth > maxDepth) {
            onSearchReturnChange({ success: false, result: { message: SearchReturnMessage.EXCEEDING_MAX_DEPETH, options: { maxDepth } } });
            return;
        }
        // 第一次查询
        if (depth === 0) {
            const nextLevelTree = await onLoadData();
            searchPath(searchText, nextLevelTree, [], 1);
            return;
        }
        if (searchText === '') {
            return;
        }
        const routeKeys: React.Key[] = historyRouteKeys;
        for (let i = 0; i < curLevelTree.length; i++) {
            const node = curLevelTree[i];
            if (node.key === searchText) {
                // 展开层级目录
                setExpandedKeys(routeKeys);
                // 选中节点
                setSelectedPath(node.key);
                // 跳转到选中节点
                setTimeout(() => {
                    const dom = document.getElementById(searchText)?.closest('div');
                    dom?.scrollIntoView({ behavior: 'smooth', block: 'center', inline: 'start' });
                }, 100);
                onSearchReturnChange({ success: true });
                return;
            } else {
                // 是否上层目录
                const curPath = String(node.key);
                // 分隔符
                const separator = curPath.includes('/') ? '/' : '\\';
                const isParentDir = searchText.startsWith(curPath.endsWith(separator) ? curPath : `${curPath}${separator}`);
                if (isParentDir) {
                    routeKeys.push(node.key);
                    const nextLevelTree = await onLoadData(node);
                    searchPath(searchText, nextLevelTree, routeKeys, depth + 1);
                    return;
                }
            }
        }
        onSearchReturnChange({ success: false, result: { message: SearchReturnMessage.FILE_NOT_FOUND } });
    };

    // 加载子节点
    const onLoadData = async(node?: TreeDataNode): Promise<TreeDataNode[]> => {
        return new Promise((resolve) => {
            getFiles(String(node?.key ?? '')).then(data => {
                const newTreeData = dealResource(data as ResourceItem);
                // 根目录
                if (!node) {
                    setTreeData(newTreeData);
                } else {
                    setTreeData(origin => updateTreeData(origin, node?.key, newTreeData));
                }
                resolve(newTreeData);
            });
        });
    };

    // 点击节点文字
    const handleSelect = (keys: React.Key[], { node }: {node: EventDataNode<TreeDataNode>}): void => {
        // 点击都是选中
        setSelectedPath(node.key);
        // 更新输入框
        onSelectedChange(String(node.key));
        // 展开的折叠or折叠的展开
        const newExpandedKeys = node.expanded ? expandedKeys.filter(key => key !== node.key) : [...expandedKeys, node.key];
        setExpandedKeys(newExpandedKeys);
        if (!node.expanded) {
            onLoadData(node);
        }
    };

    // 点击展开图标
    const handleExpand = (keys: React.Key[], { node, expanded }: {expanded: boolean;node: TreeDataNode}): void => {
        setExpandedKeys(keys);
        // 选中
        setSelectedPath(node.key);
        if (expanded) {
            onLoadData(node);
        }
    };

    // 操作目录树
    useEffect(() => {
        const { type, value } = actionListener;
        if ([CatalogAction.SEARCH, CatalogAction.REFRESH].includes(type)) {
            searchPath(value);
        }
        // 若输入框文字变化，清除选中目录
        if (type === CatalogAction.INPUT_PATH_CHANGE && value !== selectedPath) {
            setSelectedPath('');
        }
    }, [actionListener]);

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
