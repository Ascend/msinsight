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
import _ from 'lodash';
import { runInAction } from 'mobx';
import { customConsole as console } from '@insight/lib/utils';
import { LocalStorageKey, localStorageService } from '@insight/lib';
import connector from '@/connection';
import { store } from '@/store';
import { ProjectAction } from '@/utils/enum';
import {
    DataSource,
    FileOrDirectory,
    GLOBAL_HOST,
    ImportTreeInfo,
    LayerType,
    Project,
    ProjectDirectory,
} from '@/centralServer/websocket/defs';
import { addDataPath } from '@/centralServer/server';
import {
    clearProjects,
    deleteDataPath,
    deleteProject,
    getHistoryProject,
    resetTimeline,
    updateProjectName as requestUpdateProjectName,
} from '@/utils/Request';
import i18n from '@insight/lib/i18n';
import { message as Message } from 'antd';
import { isProjectNameExisted, updateDataSourceName } from '@/utils/Resource';
import { sendReset, sendUpdateProjectName } from '@/connection/sendNotification';
import { cancelBaselineData, updateProjectNameHandler } from '@/utils/Compare';
import { updateRankMapByProjectName } from '@/utils/Rank';
import { closeLoading, openLoading } from '@/utils/useLoading';
import { DEFAULT_ACTIVE_DATASOURCE } from '@/entity/session';

export interface UpdateProjectParam {
    projectAction: ProjectAction;
    projectName: string;
    projectPath: string[];
    selectedFileType: LayerType;
    selectedFilePath: string;
    selectedRankId?: string;
    children: FileOrDirectory[];
    hasConflict: boolean;
}

export const transformFile = (tree: ImportTreeInfo | undefined, depth: number): FileOrDirectory[] | undefined => {
    if (tree === undefined || depth >= 5) { return undefined; }
    let result;
    switch (tree.type) {
        case 'CLUSTER':
            result = [{
                type: tree.type,
                name: tree.fileDir,
                path: tree.filePath,
                children: tree.children.map((child) => transformFile(child, depth + 1))
                    .flat().filter((item) => item !== undefined) as FileOrDirectory[],
            }];
            break;
        case 'PROJECT': // 跳过 PROJECT
        case 'HOST': // 跳过 HOST
            result = tree.children.map((child) => transformFile(child, depth))
                .flat().filter((item) => item !== undefined) as FileOrDirectory[];
            break;
        case 'RANK':
        case 'COMPUTE':
        case 'IPYNB':
            result = [{
                type: tree.type,
                name: tree.fileDir,
                path: tree.filePath,
                rankId: tree.rankId,
                projectType: tree.projectType,
                children: tree.children.map((child) => transformFile(child, depth + 1))
                    .flat().filter((item) => item !== undefined) as FileOrDirectory[],
            }];
            break;
        default:
            break;
    }
    return result;
};

const transformProject = (data: ProjectDirectory): Project => {
    return {
        projectName: data.projectName,
        projectPath: [data.children?.[0].filePath ?? ''],
        children: data.children?.map((child) => transformFile(child, 0))?.flat()?.filter((item) => item !== undefined) as FileOrDirectory[],
    };
};
// 历史项目
export const loadHistoryProject = async(): Promise<void> => {
    const session = store.sessionStore.activeSession;
    const sources: DataSource[] = [];
    const result = await getHistoryProject() as {projectDirectoryList: ProjectDirectory[]};
    const projectDirectoryList = result?.projectDirectoryList ?? [];
    projectDirectoryList.forEach(item => {
        const source: DataSource = { ...GLOBAL_HOST, ...transformProject(item) };
        sources.push(source);
    });
    runInAction(() => {
        session.dataSources = sources;
    });
};

// 导入数据、切换项目
export async function handleProjectAction({ action, project, isConflict, selectedFileType, selectedFilePath, selectedRankId }:
{action: ProjectAction;project: Project;isConflict: boolean;selectedFileType?: LayerType;selectedFilePath?: string;selectedRankId?: string}): Promise<void> {
    const session = store.sessionStore.activeSession;
    runInAction(async() => {
        const { activeDataSource, dataSources } = session;
        const newProject = { ...project };

        // 如果目标内容就是当前选中内容，则不做任何处理直接返回
        if (newProject.projectName === activeDataSource.projectName && arraysValueEqual(newProject.projectPath, activeDataSource.projectPath)) {
            return;
        }

        openLoading();
        // 切换项目
        if (action === ProjectAction.SWITCH_PROJECT) {
            const firstFilePath = selectedFilePath ?? dataSources.find(data => data.projectName === newProject.projectName)?.projectPath[0];
            const firstFileType: LayerType = selectedFileType ?? 'PROJECT';
            if (firstFilePath !== undefined) {
                newProject.selectedFileType = firstFileType;
                newProject.selectedFilePath = firstFilePath;
                newProject.selectedRankId = selectedRankId;
            }
        }
        // First imports also need an identity for notifications arriving before import/action returns.
        session.toBeActivedProject = newProject;
        try {
            const res = await addDataPath(newProject, action, isConflict, session);
            if (!res) {
                closeLoading();
                return;
            }
        } catch (e) {
            closeLoading();
            return;
        } finally {
            session.toBeActivedProject = undefined;
        }
        // 保存文件路径
        const path = newProject.projectPath[0].includes(newProject.projectName) ? newProject.projectName : newProject.projectPath[0];
        localStorageService.setItem(LocalStorageKey.LAST_FILE_PATH, path);
    });
    cancelBaselineData();
}
// 允许2个数组值重复或乱序
function arraysValueEqual<T>(a: T[], b: T[]): boolean {
    return a.length === b.length && a.every((value) => b.includes(value));
}

// 项目更新
// 1、更新项目目录
// 2、设置为打开(选中）项目
export const updateProject = ({ projectAction, projectName, children, hasConflict, projectPath, selectedFileType, selectedFilePath, selectedRankId }:
UpdateProjectParam): void => {
    const session = store.sessionStore.activeSession;
    const dataSource: DataSource = { ...GLOBAL_HOST, projectName, projectPath, children };
    runInAction(() => {
        if (projectAction === ProjectAction.ADD_FILE) {
            // 更新项目目录
            session.dataSources = getMergedDataSources(session.dataSources, dataSource, hasConflict);
            // 如果存在冲突 或 切换的子目录存在多个，则选中一级目录
            if (hasConflict || children.length > 1) {
                const firstFile = getProjectFirstFile(dataSource);
                session.activeDataSource = {
                    ...dataSource,
                    selectedFileType: firstFile?.type ?? 'UNKNOWN',
                    selectedFilePath: firstFile?.path ?? '',
                    selectedRankId: firstFile?.rankId,
                };
            }
            // 导入项目时，如果项目发生了切换，或原本选的为二级目录，则更新当前选中目录
            if (session.activeDataSource.projectName !== dataSource.projectName || session.activeDataSource.selectedFilePath !== '') {
                session.activeDataSource = { ...dataSource, selectedFileType, selectedFilePath, selectedRankId };
                return;
            }
        }
        if (projectAction === ProjectAction.SWITCH_PROJECT) {
            session.activeDataSource = { ...dataSource, selectedFileType, selectedFilePath, selectedRankId };
        }
    });
};

// 获取合并目录
function getMergedDataSources(oldDataSources: DataSource[], dataSource: DataSource, hasConflict: boolean): DataSource[] {
    const dataSources = JSON.parse(JSON.stringify(oldDataSources)) as DataSource[];
    const projectIndex = dataSources.findIndex((item) =>
        item.remote === dataSource.remote && item.port === dataSource.port && item.projectName === dataSource.projectName);
    // 项目不存在，新增项目
    if (projectIndex === -1) {
        dataSources.push(dataSource);
    } else {
        // 项目存在，在已有项目下增加文件
        // 有冲突，替换项目的子文件
        if (hasConflict) {
            dataSources[projectIndex].children = dataSource.children;
        } else {
            // 新导入文件全部不在当前项目下
            if (!hasFileOverlap(dataSources[projectIndex], dataSource)) {
                const merged = mergeProjects(dataSources[projectIndex], dataSource);
                dataSources[projectIndex].children = merged.children;
            }
        }
    }
    return dataSources;
}

// 移除项目
export const removeProject = (projectIndex: number): void => {
    const session = store.sessionStore.activeSession;
    runInAction(async() => {
        try {
            const dataSource = session.dataSources[projectIndex];
            // 如果移除的是当前打开项目
            if (session.activeDataSource.projectName === dataSource.projectName) {
                // 通知各页签
                connector.send({ event: 'remote/remove', body: { dataSource } });
                // 通知后台，清理Timeline
                await resetTimeline();
                // 重置frame页面
                session.reset(true);
                // 设置当前打开（选中）项目空
                session.activeDataSource = DEFAULT_ACTIVE_DATASOURCE;
            }
            // 通知后台
            await deleteProject(dataSource);
            // 目录更新
            session.deleteDataSource(projectIndex);
        } catch {
            console.error('remove error');
        }
        closeLoading();
    });
};

// 移除多个项目
export const removeProjects = async (projectNameList: React.Key[] = []): Promise<void> => {
    const session = store.sessionStore.activeSession;
    runInAction(async() => {
        try {
            await clearProjects(projectNameList);
            // 目录更新
            session.dataSources = session.dataSources.filter(dataSource => !projectNameList.includes(dataSource.projectName));
            // 如果删除项目包含当前打开项目或者全部删除
            if (projectNameList.length === 0 || projectNameList.includes(session.activeDataSource.projectName)) {
                // 通知页签
                sendReset();
                // 通知后台，清理Timeline
                await resetTimeline();
                // framework重置
                session.reset(true);
                // 当前选中置空
                session.activeDataSource = DEFAULT_ACTIVE_DATASOURCE;
            }
        } catch {
            console.error('remove error');
        }
        closeLoading();
    });
};

// 移除文件
export const removeDataPath = (projectIndex: number, dataPath: string): void => {
    const session = store.sessionStore.activeSession;
    runInAction(async() => {
        try {
            const dataSource = session.dataSources[projectIndex];
            const childrenNum = calculateRemovableChildren(dataSource);
            // 项目只有一个文件
            if (childrenNum === 1) {
                removeProject(projectIndex);
                return;
            }
            const singleDataPath = dataPath;
            // 通知各页签
            if (session.activeDataSource.projectName === dataSource.projectName) {
                connector.send({ event: 'remote/removeSingle', body: { dataSource, singleDataPath } });
            }
            // 通知后台
            await deleteDataPath({ ...dataSource, dataPath: [singleDataPath] });
            // 目录更新
            const nextDataSource = session.deleteDataPath(projectIndex, singleDataPath);
            // 如果移除的是当前选中文件，默认选中第一个文件
            if (session.activeDataSource.projectName === dataSource.projectName && session.activeDataSource.selectedFilePath === singleDataPath) {
                const firstFile = getProjectFirstFile(nextDataSource);
                session.activeDataSource = {
                    ...dataSource,
                    selectedFileType: firstFile?.type ?? 'UNKNOWN',
                    selectedFilePath: firstFile?.path ?? '',
                    selectedRankId: firstFile?.rankId,
                };
            }
        } catch {
            console.error('removeSingle error');
        }
        closeLoading();
    });
};

// 修改工程名
export const updateProjectName = async (oldProjectName: string, newProjectName: string): Promise<boolean> => {
    try {
        const existed = isProjectNameExisted(newProjectName);
        if (existed) {
            Message.warning(i18n.t('Duplicate Project', { ns: 'framework' }));
            return false;
        }
        // 更新卡信息
        updateRankMapByProjectName(oldProjectName, newProjectName);
        await requestUpdateProjectName(oldProjectName, newProjectName);
        // 通知模块
        sendUpdateProjectName(oldProjectName, newProjectName);
        // 修改目录
        updateDataSourceName(oldProjectName, newProjectName);
        // 更新对比功能
        updateProjectNameHandler(oldProjectName, newProjectName);
        return true;
    } catch {
        Message.warning(i18n.t('Update Project Name Failed', { ns: 'framework' }));
        return false;
    }
};

export const getProjectFirstFile = (project: Project): FileOrDirectory | undefined => {
    const getFirstFile = (files: FileOrDirectory[], depth: number): FileOrDirectory | undefined => {
        if (files.length === 0) { return undefined; }
        if (depth === 5) {
            console.error('over 5 layers of file，deleting fail!');
            return undefined;
        }
        if (files[0].children.length === 0) {
            return files[0];
        } else {
            return getFirstFile(files[0].children, depth + 1);
        }
    };

    return getFirstFile(project.children, 0);
};

export const hasFileOverlap = (a: Project, b: Project): boolean => {
    const pathSet = new Set<string>();
    recursiveSearchEveryProjectFileThen(a, (file) => {
        pathSet.add(file.path);
    });

    // 在第二个项目中查找是否有重复的路径
    return recursiveSearchSomeProjectFileThen(b, (file) => pathSet.has(file.path));
};

// 构建一个 map，用于快速查找节点
const createNodeMap = (nodes: FileOrDirectory[]): Map<string, FileOrDirectory> => {
    const map: Map<string, FileOrDirectory> = new Map();
    const addToMap = (node: FileOrDirectory, depth: number): void => {
        if (depth >= 5) { return; }
        map.set(node.path, node);
        if (node.children) {
            node.children.forEach((child) => addToMap(child, depth + 1));
        }
    };
    nodes.forEach((child) => addToMap(child, 0));
    return map;
};

// 深拷贝并合并两个节点
const mergeNodes = (nodeA: FileOrDirectory | undefined, nodeB: FileOrDirectory): FileOrDirectory => {
    if (!nodeA) { return _.cloneDeep(nodeB); }
    if (!nodeB) { return _.cloneDeep(nodeA); }

    const merged = _.cloneDeep(nodeA) as FileOrDirectory; // 深拷贝
    merged.children = [];

    const allChildrenMap: Map<string, FileOrDirectory> = new Map();

    // 收集 nodeA 子节点
    if (nodeA.children) {
        nodeA.children.forEach(child => {
            allChildrenMap.set(child.path, child);
        });
    }

    // 收集并可能覆盖或添加 nodeB 子节点
    if (nodeB.children) {
        nodeB.children.forEach(child => {
            if (allChildrenMap.has(child.path)) {
                allChildrenMap.set(child.path, mergeNodes(allChildrenMap.get(child.path), child));
            } else {
                allChildrenMap.set(child.path, child);
            }
        });
    }

    // 转换回数组
    merged.children = Object.values(allChildrenMap).map(child => {
        return _.cloneDeep(child); // 拷贝一份
    });

    return merged;
};

function findTopLevelPaths(paths: Set<string>): string[] {
    if (!paths || paths.size === 0) { return []; }

    // 去重并排序路径，确保较短（层级更高）的路径排在前面
    const uniquePaths = [...paths];

    const sep = uniquePaths[0].includes('\\') ? '\\' : '/';
    // 根据路径长度排序，这样可以保证父级路径总是在子路径之前被处理
    uniquePaths.sort((a, b) => a.split(sep).length - b.split(sep).length);

    const result: string[] = [];

    for (const currentPath of uniquePaths) {
        if (result.some((pathString: string) => currentPath.startsWith(pathString))) {
            continue;
        }
        result.push(currentPath);
    }

    return result;
}

export const mergeProjects = (projectA: Project, projectB: Project): Project => {
    // 获取根路径集合，处理多 projectPath 场景
    const allRootPaths = [...new Set([...projectA.projectPath, ...projectB.projectPath])];

    // 构建根节点映射
    const buildRootsFromProject = (project: Project): FileOrDirectory[] => {
        return project.children;
    };

    const rootsA = buildRootsFromProject(projectA);
    const rootsB = buildRootsFromProject(projectB);

    const mapA = createNodeMap(rootsA);
    const mapB = createNodeMap(rootsB);

    const mergedRoots: FileOrDirectory[] = [];

    const allPaths = new Set([...mapA.keys(), ...mapB.keys()]);

    findTopLevelPaths(allPaths).forEach(path => {
        const nodeA = mapA.get(path);
        const nodeB = mapB.get(path);

        if (nodeA && nodeB) {
            mergedRoots.push(mergeNodes(nodeA, nodeB));
        } else if (nodeA) {
            mergedRoots.push(_.cloneDeep(nodeA));
        } else if (nodeB) {
            mergedRoots.push(_.cloneDeep(nodeB));
        } else { /* do nothing */ }
    });

    return {
        projectName: `${projectA.projectName}+${projectB.projectName}`,
        projectPath: allRootPaths,
        children: mergedRoots,
    };
};

export const deleteProjectDataPath = (project: Project, dataPath: string): void => {
    recursiveSearchSomeProjectFileThen(project, (file) => file.path === dataPath, (__, foundIdx, files, parentInfo) => {
        if (files.length === 1 && parentInfo.parent !== null && parentInfo.grandParent !== null) {
            parentInfo.grandParent.children.splice(parentInfo.parentIndex, 1);
        } else {
            files.splice(foundIdx, 1);
        }
    });
};

function calculateRemovableChildren(project: Project): number {
    let result = 0;
    recursiveSearchEveryProjectFileThen(project, (file) => {
        if (file.type !== 'PROJECT' && file.type !== 'CLUSTER') {
            result++;
        }
    });
    return result;
}

interface ParentInfo {
    parent: FileOrDirectory | null;
    parentIndex: number;
    grandParent: FileOrDirectory | null;
}

const recursiveSearchSomeProjectFileThen = (project: Project,
    searcher: (file: FileOrDirectory) => boolean,
    action?: (file: FileOrDirectory, index: number, foundFiles: FileOrDirectory[], parentInfo: ParentInfo) => void): boolean => {
    const searchFilesThen = (files: FileOrDirectory[], parentInfo: ParentInfo, depth: number): boolean => {
        if (depth >= 5) {
            console.error('over 5 layers of file，deleting fail!');
            return false;
        }
        if (files.length === 0) { return false; }
        const foundIdx: number = files.findIndex((file: FileOrDirectory): boolean => searcher(file));
        if (foundIdx !== -1) {
            action?.(files[foundIdx], foundIdx, files, parentInfo);
            return true;
        }
        for (let i = 0; i < files.length; i++) {
            if (searchFilesThen(files[i].children, {
                parent: files[i],
                parentIndex: i,
                grandParent: parentInfo.parent,
            }, depth + 1)) {
                return true;
            }
        }
        return false;
    };

    return searchFilesThen(project.children, { parent: project as unknown as FileOrDirectory, parentIndex: -1, grandParent: null }, 0);
};

const recursiveSearchEveryProjectFileThen = (project: Project, action?: (file: FileOrDirectory, index: number) => void): void => {
    const searchFilesThen = (files: FileOrDirectory[], depth: number): void => {
        if (depth >= 5) {
            console.error('over 5 layers of file，deleting fail!');
            return;
        }
        if (files.length === 0) { return; }
        files.forEach((file, index) => {
            action?.(file, index);
        });
        for (let i = 0; i < files.length; i++) {
            searchFilesThen(files[i].children, depth + 1);
        }
    };

    searchFilesThen(project.children, 0);
};
