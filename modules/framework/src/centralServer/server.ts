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
import {
    DataRequest,
    ModuleName,
    ConnectHost,
    GLOBAL_HOST,
    Project,
    FileOrDirectory,
    ImportResultBody,
    DataSource,
    LayerType, ImportTreeInfo, type Response,
} from './websocket/defs';
import { Connection, ErrorMsg } from './websocket/connection';
import connector from '@/connection';
import {
    importProject,
    ImportProjectParams,
} from '@/utils/Request';
import { ProjectAction, SessionAction, ProjectType } from '@/utils/enum';
import { updateRankMap } from '@/utils/Rank';
import { transformFile, updateProject } from '@/utils/Project';
import { closeLoading } from '@/utils/useLoading';
import { updateDataScene } from '@/components/TabPane/Index';
import { Session } from '@/entity/session';
import { errorCenter, RequestOptions, WsError } from '@insight/lib';
import { collectDataSourcePaths, isFileInDataSource } from '../utils/filePath';
import { getConnectHost, isVscodeEnv, setConnectRemoteFn } from '@/vscode-adapter';

export const CONNECTION_MAP: Map<string, Connection> = new Map();

// Only the most recently started request may reset the current page on a missing response body.
const latestImportRequestBySession = new WeakMap<Session, object>();

const getConnectionMapKey = (host: ConnectHost): string => {
    return `${host?.remote}:${host?.port}`;
};

// 获取实际使用的 host
const getActualHost = (): ConnectHost => {
    if (isVscodeEnv()) {
        return getConnectHost();
    }
    return GLOBAL_HOST;
};

export const connectRemote = async function (host: ConnectHost): Promise<boolean> {
    const connection = new Connection(host);
    try {
        await connection.connect();
    } catch (e) {
        return false;
    }
    CONNECTION_MAP.set(getConnectionMapKey(host), connection);
    return true;
};

// 注册 connectRemote 到 DataViewerController，打破循环依赖
if (isVscodeEnv()) {
    setConnectRemoteFn(connectRemote);
};

export const isExistedRemote = function(host: ConnectHost): boolean {
    return CONNECTION_MAP.has(getConnectionMapKey(host));
};

export const addDataPath = async function(project: Project, action: ProjectAction, isConflict: boolean, session: Session): Promise<boolean> {
    const host = getActualHost();
    if (!isExistedRemote(host)) {
        const connected = await connectRemote(host);
        if (!connected) {
            return false;
        }
    }
    const connection = CONNECTION_MAP.get(getConnectionMapKey(host));
    if (connection) {
        const importRequest = {};
        latestImportRequestBySession.set(session, importRequest);
        connector.send({
            event: 'remote/savePageSetting',
            body: {},
            target: 'plugin',
        });
        const params: ImportProjectParams = {
            projectName: project.projectName,
            path: project.projectPath,
            selectedFileType: project.selectedFileType,
            selectedFilePath: project.selectedFilePath,
            selectedRankId: project.selectedRankId,
            projectAction: action,
            isConflict,
        };
        // Directory contents are unknown until import/action returns. Register possible snapshot
        // ownership before awaiting it, but do not reset snapshot state for ordinary data files.
        // This frontend generation is not a backend task ID for retries of the same file.
        const sourcePaths = collectDataSourcePaths(project);
        const mayImportSnapshot = sourcePaths.some(path => !/\.(json|db|bin|csv|ipynb)$/i.test(path)) ||
            project.children?.some(child => isMemSnapshotFile(child.path));
        const parseAttempt = mayImportSnapshot ? ++session.memSnapshotParseAttempt : undefined;
        if (parseAttempt !== undefined) {
            session.memSnapshotParseFailure = null;
            session.memSnapshotSourcePaths = sourcePaths;
            session.memSnapshotImportPending = true;
            connector.send({
                event: 'updateSession',
                body: {
                    memSnapshotParseAttempt: parseAttempt, memSnapshotSourcePaths: sourcePaths,
                    memSnapshotImportPending: true, memSnapshotParseFailure: null,
                },
            });
        }
        let response: Response<ImportResultBody>;
        try {
            response = await importProject(params) as Response<ImportResultBody>;
        } finally {
            if (parseAttempt !== undefined && parseAttempt === session.memSnapshotParseAttempt) {
                session.memSnapshotImportPending = false;
                connector.send({
                    event: 'updateSession',
                    body: { memSnapshotParseAttempt: parseAttempt, memSnapshotImportPending: false },
                });
            }
        }
        const { body: result, error } = response;
        // Only snapshot responses use the snapshot initialization guard. Ordinary imports must
        // still run error handling, remote/import and project updates when their responses overlap.
        const hasMatchingFailure = parseAttempt === session.memSnapshotParseAttempt &&
            isFileInDataSource(project, session.memSnapshotParseFailure?.fileId ?? '');
        const importedSnapshotFile = sourcePaths.find(isMemSnapshotFile) || findMemSnapshotFile(result?.children);
        const isSnapshotImport = Boolean(importedSnapshotFile) || hasMatchingFailure;
        if (isSnapshotImport && parseAttempt !== undefined && parseAttempt !== session.memSnapshotParseAttempt) {
            return false;
        }

        if (error) {
            const { code, message } = error;
            errorCenter.handleError(new WsError(code, message));
        }

        if (!result) {
            if (latestImportRequestBySession.get(session) === importRequest) {
                connector.send({ event: 'remote/reset', body: {}, target: 'plugin' });
            }
            return false;
        }
        const res = result;
        if (importedSnapshotFile) {
            // Once the response identifies the selected snapshot, siblings in the imported
            // directory no longer own its failure state, even before switchDirectory is sent.
            session.memSnapshotSourcePaths = [importedSnapshotFile];
            session.memSnapshotParseFileId = importedSnapshotFile;
        }
        // An ordinary import only replaces snapshot state once it succeeds, never while a newer
        // snapshot import is pending. Late iframe hydration must not replay the previous failure.
        if (!isSnapshotImport && !session.memSnapshotImportPending) {
            session.memSnapshotParseFailure = null;
            session.memSnapshotParseFileId = '';
            session.memSnapshotSourcePaths = [];
            session.memSnapshotParseLoading = false;
            session.memSnapshotParseProgress = 0;
        }
        // 这里添加 session.actionListener.type !== SessionAction.ADD_DATA_UNDER_PROJECT 条件，防止在项目中添加卡的场景下，集群页面被重置
        // 这里添加 res.isCluster 判断导入数据是否是集群，如果是集群数据，也要重置 session 使 clusterCompleted = false, 确保 Summary 和 Communication 模块正常加载
        if (session.actionListener.type !== SessionAction.ADD_DATA_UNDER_PROJECT && (res.reset || res.isCluster)) {
            const parseFailure = session.memSnapshotParseFailure;
            const snapshotSourcePaths = session.memSnapshotSourcePaths;
            const snapshotImportPending = session.memSnapshotImportPending;
            session.reset();
            session.memSnapshotParseFailure = parseFailure;
            session.memSnapshotParseFileId = importedSnapshotFile || parseFailure?.fileId || '';
            session.memSnapshotSourcePaths = snapshotSourcePaths;
            session.memSnapshotImportPending = snapshotImportPending;
        }
        // 这里立即将 actionListener 恢复为默认值，防止在正常导入或切换项目的场景下，不执行上方的 session.reset
        session.resetActionListener();

        // 时间线需要判断是整体添加还是在项目内添加
        const { activeDataSource } = session;

        // 是否切换项目
        const switchProject = params?.projectAction !== ProjectAction.ADD_FILE || activeDataSource?.projectName !== params?.projectName;

        // 添加参数告诉是否为点击的添加
        connector.send({
            event: 'remote/import',
            body: {
                dataSource: transformTimelineDataSource(project), importResult: res, switchProject,
                memSnapshotImportPending: session.memSnapshotImportPending,
                ...(!isSnapshotImport || parseAttempt === undefined ? {} : {
                    memSnapshotParseAttempt: parseAttempt,
                    memSnapshotSourcePaths: session.memSnapshotSourcePaths,
                    memSnapshotParseFailure: session.memSnapshotParseFailure,
                }),
            },
            target: 'plugin',
        }); // 由于发送时页签应该只有 时间线、内存、算子 存在，所以这个事件只能被这三个页签收到
        afterImportProject(params, res);
    }
    return true;
};

const isMemSnapshotFile = (path: string): boolean => /\.(pkl|pickle)$/i.test(path);

const findMemSnapshotFile = (children: ImportTreeInfo[] = []): string => {
    for (const child of children) {
        const filePath = isMemSnapshotFile(child.filePath) ? child.filePath : findMemSnapshotFile(child.children);
        if (filePath) {
            return filePath;
        }
    }
    return '';
};

const transformTimelineDataSource = (project: Project): DataSource => {
    const data = { ...GLOBAL_HOST, ...project, dataPath: project.projectPath };
    return data as DataSource;
};

/**
 * 递归查找 rank 列表
 * @param children
 */
const reduceImportRankInfoList = (children: ImportTreeInfo[] = []): Array<{ rankId: string; filePath: string }> => {
    const reduceFunc = (item: ImportTreeInfo, depth: number): Array<{ rankId: string; filePath: string }> => {
        if (depth >= 5) { // 设置最大递归深度为 5
            return [];
        }
        if (item.type === 'RANK' || item.type === 'COMPUTE' || item.type === 'IPYNB') {
            return [{ rankId: item.rankId ?? '', filePath: item.filePath }];
        } else {
            return item.children.flatMap((child) => reduceFunc(child, depth + 1));
        }
    };

    return children.flatMap((child) => reduceFunc(child, 0));
};

/**
 * 导入项目后更新项目管理器操作
 * @param params 请求体
 * @param data 返回值
 */
const afterImportProject = (params: ImportProjectParams, data: ImportResultBody): void => {
    try {
        const projectAction = params.projectAction as ProjectAction;
        const hasConflict = params.isConflict as boolean;
        const projectName = params.projectName;
        const projectPath = params.path;
        const importRankList = reduceImportRankInfoList(data.children);
        let targetList = importRankList;

        // selectedFilePath、selectedRankId 仅在Cluster模块使用，联合导入需排除ProjectType.IE类型数据，避免初次导入selectedFilePath取到ProjectType.IE的filePath
        if (data.isCluster && data.isIE) {
            const subOfNotIE = data.children.filter(item => item.projectType !== ProjectType.IE);
            targetList = reduceImportRankInfoList(subOfNotIE);
        }
        const selectedFilePath = params.selectedFilePath ?? targetList?.[0]?.filePath ?? '';
        const selectedRankId = params.selectedRankId ?? targetList?.[0]?.rankId ?? '';
        const selectedFileType: LayerType = params.selectedFileType ?? 'RANK'; // 此处暂时用 RANK，实际应该和 data.subdirectoryList[0] 中应该带有的类型相同
        const children = data.children?.map((child) => transformFile(child, 0))
            ?.flat()?.filter((item) => item !== undefined) as FileOrDirectory[];
        // 更新场景
        updateDataScene(data);
        // 更新rank信息
        updateRankMap(projectAction, projectName, importRankList);
        // 更新项目目录
        updateProject({ projectAction, projectName, children, hasConflict, projectPath, selectedFileType, selectedFilePath, selectedRankId });
    } catch (error) {
        console.error(error);
    }
    closeLoading();
};

// framework发送的请求
export const request = async <T>(
    moduleName: ModuleName,
    args: DataRequest,
    options?: RequestOptions,
): Promise<T | ErrorMsg> => {
    const host = getActualHost();
    const connection: Connection | undefined = CONNECTION_MAP.get(getConnectionMapKey(host));

    if (connection === undefined) {
        return Promise.reject(Error('no connection'));
    }

    const res = await connection?.fetch<T>(moduleName, args);

    if ((res as Response)?.command === 'import/action') {
        return res;
    }

    if ((res as ErrorMsg).error) {
        const { error: { code, message } } = res as ErrorMsg;
        const wsError = new WsError(code, message);
        if (!options?.silent) {
            errorCenter.handleError(wsError);
        }

        throw wsError;
    }

    return res;
};

// 子模块发送的请求
export const requestModule = function <T>(
    moduleName: ModuleName,
    args: DataRequest,
): Promise<T | ErrorMsg> {
    const host = getActualHost();
    const connection: Connection | undefined = CONNECTION_MAP.get(getConnectionMapKey(host));

    if (connection === undefined) {
        return Promise.reject(Error('no connection'));
    }

    return connection?.fetch<T>(moduleName, args);
};
