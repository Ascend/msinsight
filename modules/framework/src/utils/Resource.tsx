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
import React from 'react';
import { FileIcon, FolderIcon } from '@insight/lib/icon';
import type { TreeDataNode } from 'antd';
import { message as Message } from 'antd';
import { checkProjectValid, getFiles } from '@/utils/Request';
import { DataSource, Project } from '@/centralServer/websocket/defs';
import { ProjectError } from '@/utils/enum';
import { LocalStorageKey, localStorageService } from '@insight/lib';
import { store } from '@/store';
import { runInAction } from 'mobx';
import { hasFileOverlap } from '@/utils/Project';

export interface ResourceItem {
    path: string;
    name: string;
    childrenFolders: ResourceItem[];
    childrenFiles: ResourceItem[];
    exist: boolean;
};

export interface ProjectCheckErrorDetail {
    layer: number;
    error: ProjectError;
    path: string;
    message: string;
}

export interface ProjectCheckResult {
    result: ProjectError;
    errorDetail: ProjectCheckErrorDetail[];
}

// 接口返回数据转为Tree组件数据结构
export const dealResource = (resource: ResourceItem): TreeDataNode[] => {
    const folders = (resource.childrenFolders ?? []).map(item => ({
        isLeaf: false,
        icon: <FolderIcon/>,
        title: <span id={item.path}>{item.name}</span>,
        key: item.path,
    }));
    const files = (resource.childrenFiles ?? [])?.map(item => ({
        isLeaf: true,
        icon: <FileIcon/>,
        title: <span id={item.path}>{item.name}</span>,
        key: item.path,
    }));
    return [...folders, ...files];
};

const maxDepth = 100;
// 更新Tree组件子节点
// Recursive，递归函数
export const updateTreeData = (list: TreeDataNode[], key: React.Key, children: TreeDataNode[], depth: number = 0): TreeDataNode[] => {
    if (depth > maxDepth) {
        Message.error(`The depth of the file directory exceeds ${maxDepth}`);
        return [];
    }
    return list.map(node => {
        if (node.key === key) {
            return {
                ...node,
                children,
            };
        }
        if (node.children) {
            return {
                ...node,
                children: updateTreeData(node.children, key, children, depth + 1),
            };
        }
        return node;
    });
};

// 文件/文件夹是否存在
export const fileExist = async (path: string): Promise<boolean> => {
    if (path == null || path === undefined || path === '') {
        return false;
    }
    const result = await getFiles(path) as ResourceItem;
    return result.exist;
};

// 文件/文件夹安全校验
export const checkPathValid = async(project: Project):
Promise<ProjectCheckResult> => {
    // 检查文件是否存在
    const existed = await fileExist(project.projectPath[0] ?? '');
    if (!existed) {
        return { result: ProjectError.FILE_NOT_EXIST, errorDetail: [] };
    }
    // 是否在左侧已导入的数据中
    if (checkExistedDataSource(project)) {
        return { result: ProjectError.IMPORTED, errorDetail: [] };
    }
    // 校验文件安全，如文件冲突、超大文件、软链接等
    const res = await checkProjectValid({ projectName: project.projectName ?? '', dataPath: project.projectPath ?? [] });
    const result = res as {result: ProjectError; errorDetail?: ProjectCheckErrorDetail[]};
    return { result: result.result, errorDetail: result.errorDetail ?? [] };
};

// 文件是否已导入
export const checkExistedDataSource = (data: Project): boolean => {
    const session = store.sessionStore.activeSession;
    const allProject: DataSource[] = session.dataSources ?? [];
    const project = allProject.find((item) => item.projectName === data.projectName);
    if (project === undefined) {
        return false;
    }
    return hasFileOverlap(data, project);
};

// 上次导入文件路径
export const getLastFilePath = (): string => {
    return localStorageService.getItem(LocalStorageKey.LAST_FILE_PATH) ?? '';
};

// 修整输入的文件路径
// 1、去掉首尾空字符串
// 2、除了C:\这种根目录，除去其它路径尾部多余的斜杠\ /，如D:\data\ -> D:\data
export const getTrimedPath = (inputPath: string): string => {
    const trimedPath = inputPath.trim();
    const rootReg = /^[\s\S]+:\\+$/;
    if (rootReg.test(trimedPath)) {
        // 清理路径尾部多余的\
        return trimedPath.replace(/:\\+$/, ':\\');
    }
    return trimedPath.replace(/[^\\/][\\/]+$/, (match: string) => match.substring(0, 1));
};

// 查询目录树时，自动修整查询路径
// 1、修整
// 2、云环境默认路径
export const getSearchDir = (inputPath = ''): string => {
    const path = getTrimedPath(inputPath);
    if (path !== '') {
        return path;
    }
    // 云指定路径
    const isCloudEnv = window.location.pathname.includes('/proxy/');
    const defaultCloudDir = '/home/ma-user/work';
    return isCloudEnv ? defaultCloudDir : path;
};

// 工程名是否存在
export const isProjectNameExisted = (projectName: string): boolean => {
    const session = store.sessionStore.activeSession;
    return session.dataSources.some((item) => item.projectName === projectName);
};

// 修改数据源的工程名
export const updateDataSourceName = (oldProjectName: string, newProjectName: string): void => {
    if (!isProjectNameExisted(oldProjectName)) {
        return;
    }
    const session = store.sessionStore.activeSession;
    const { dataSources, activeDataSource } = session;

    runInAction(() => {
        session.dataSources = dataSources.map(dataSource =>
            dataSource.projectName === oldProjectName ? { ...dataSource, projectName: newProjectName } : dataSource);

        if (activeDataSource.projectName === oldProjectName) {
            session.activeDataSource = { ...activeDataSource, projectName: newProjectName };
        }
    });
};
