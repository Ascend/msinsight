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
import React, { useEffect, useState } from 'react';
import { observer } from 'mobx-react';
import styled from '@emotion/styled';
import { Modal, Typography } from 'antd';
import { useTranslation } from 'react-i18next';
import { Button, Input, Tooltip } from '@insight/lib/components';
import { RefreshIcon } from '@insight/lib/icon';
import { checkPathValid, fileExist, getLastFilePath, getSearchDir, getTrimedPath } from '@/utils/Resource';
import type { ProjectCheckErrorDetail, ProjectCheckResult } from '@/utils/Resource';
import { ProjectAction, ProjectError } from '@/utils/enum';
import { type Project } from '@/centralServer/websocket/defs';
import type { CatalogActionListener, SearchResult } from './ResourceCatalog';
import ResourceCatalog, { CatalogAction } from './ResourceCatalog';
import { handleProjectAction } from '@/utils/Project';
import FileConflictDialog from './FileConflictDialog';

const { Text } = Typography;

const FileExplorerContainer = styled.div`
    .project-name {
        display: flow;
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
        font-weight: 700;
        padding-bottom: 10px;
    }
    .import-tips {
        color: ${(props): string => props.theme.textColorSecondary};
        margin-bottom: 10px;
    }
    // 输入框
    .ant-input-affix-wrapper {
        width: 100%;
    }
    .ant-input-show-count-suffix {
        color: ${(props): string => props.theme.infoColor};
    }
    .icon-refresh {
        cursor: pointer;
        color: ${(props): string => props.theme.textColorPlaceholder};
    }
    .icon-refresh:hover {
        color: ${(props): string => props.theme.primaryColor};
    }
    //提示文字
    .ant-typography{
        color: ${(props): string => props.theme.primaryColor};
    }
    .ant-typography-danger {
        color: ${(props): string => props.theme.dangerColor};
    }
`;

const StyledModal = styled(Modal)`
    .ant-modal-footer {
        padding: 10px 0;
    }
`;

// 文件最大路径长度
const MAX_FILE_PATH_LENGTH = 4096;
interface IProps {
    customImport: boolean;
    importTips: string;
    currentProject: string;
    dialogOpen: boolean;
    closeDialog: () => void;
    onConfirm: (path: string) => void;
    onCancel: () => void;
}
// 文件资源管理器
const FileExplorer = observer(({ dialogOpen, closeDialog, currentProject, customImport, importTips, onConfirm, onCancel }: IProps) => {
    const { t } = useTranslation('framework');
    const [inputPath, setInputPath] = useState(getLastFilePath());
    const [selectedPath, setSelectedPath] = useState('');
    const [actionListener, setActionListener] = useState<CatalogActionListener>({ type: CatalogAction.NO_ACTION });
    const [hit, setHit] = useState<{alert: boolean;message: string;options?: Record<string, string | number>}>({ alert: false, message: 'FileSearchDescribe' });
    const [conflictModalVis, setConflictModalVis] = useState<boolean>(false);
    const [checkResult, setCheckResult] = useState<ProjectError>(ProjectError.NO_ERRORS);
    const [checkErrors, setCheckErrors] = useState<ProjectCheckErrorDetail[]>([]);
    const [confirmLoading, setConfirmLoading] = useState<boolean>(false);

    // 点击确认
    const handleConfirm = async (): Promise<void> => {
        setConfirmLoading(true);
        try {
            await confirmFile();
        } finally {
            setConfirmLoading(false);
        }
    };

    // 点击确认
    const handleCancel = (): void => {
        onCancel();
        closeDialog();
    };

    const confirmFile = async (): Promise<void> => {
        const path = selectedPath;
        // 若currentProject存在，在已有项目下导入数据，否则新增项目
        const projectName = currentProject !== '' ? currentProject : path;
        const newProject: Project = { projectName, projectPath: [path], children: [] };

        // 导入场景：拿到选择的文件路径，单独处理后续逻辑
        if (customImport) {
            await confirmSelectedPath(path);
            return;
        }

        // 校验
        let validRes: ProjectCheckResult;
        try {
            validRes = await checkPathValid(newProject);
        } catch (error) {
            console.error('Check path valid failed:', error);
            setHit({ alert: true, message: 'FileCheckFailedDescribe' });
            return;
        }
        const projectError = validRes.result;
        // 校验通过
        if ([ProjectError.NO_ERRORS, ProjectError.IMPORTED].includes(projectError)) {
            const action = projectError === ProjectError.NO_ERRORS ? ProjectAction.ADD_FILE : ProjectAction.SWITCH_PROJECT;
            handleProjectAction({ action, project: newProject, isConflict: false });
            closeDialog();
            // 校验告警
        } else {
            if (projectError === ProjectError.FILE_NOT_EXIST) {
                setHit({ alert: true, message: 'FileNotFundDescribe' });
            }
            if (projectError > ProjectError.OTHER) {
                setCheckResult(projectError);
                setCheckErrors(validRes.errorDetail);
                setConflictModalVis(true);
            }
        }
    };

    const confirmSelectedPath = async (path: string): Promise<void> => {
        const existed = await fileExist(path);
        if (existed) {
            onConfirm(path);
            closeDialog();
        } else {
            setHit({ alert: true, message: 'FileNotFundDescribe' });
        }
    };

    // 查询目录树
    const searchCatalog = (): void => {
        const path = getSearchDir(inputPath);
        setInputPath(path);
        setActionListener({ type: CatalogAction.SEARCH, value: path });
    };

    // 目录查询返回结果
    const handleSearchReturn = ({ success, result }: SearchResult): void => {
        setHit({ alert: !success, message: result?.message ? result.message : 'FileSearchDescribe', options: result?.options });
    };

    const onContinue = (): void => {
        const path = getTrimedPath(inputPath);
        const project: Project = { projectName: currentProject === '' ? path : currentProject, projectPath: [path], children: [] };
        handleProjectAction({ action: ProjectAction.ADD_FILE, project, isConflict: true });
        setConflictModalVis(false);
        setTimeout(() => {
            closeDialog();
        }, 0);
    };

    const closeConflictModal = (): void => {
        setConflictModalVis(false);
        setCheckErrors([]);
    };

    const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>): void => {
        setInputPath(e.target.value);
        setActionListener({ type: CatalogAction.INPUT_PATH_CHANGE, value: e.target.value });
    };

    const handleSelectedChange = (val: string): void => {
        setSelectedPath(val);
        if (val !== '' && val !== inputPath) {
            setInputPath(val);
        }
    };

    // 每次打开对话框，刷新目录
    useEffect(() => {
        if (dialogOpen) {
            searchCatalog();
        }
    }, [dialogOpen]);

    // 输入路径更新后，清掉告警
    useEffect(() => {
        setHit({ alert: false, message: 'FileSearchDescribe' });
    }, [inputPath]);

    return <><StyledModal title={t('File Explorer')} open={dialogOpen} onCancel={handleCancel}
        width={800}
        footer={<div>
            <Button onClick={handleConfirm} loading={confirmLoading} type="primary" style={{ marginRight: 8 }} disabled={selectedPath === ''}>{t('Confirm')}</Button>
            <Button onClick={handleCancel}>{t('Cancel')}</Button>
        </div>}>
        <FileExplorerContainer>
            {!currentProject ? <></> : <span className="project-name">{t('Current Project')} ：{currentProject}</span>}
            {importTips ? <div className="import-tips">{importTips}</div> : null}
            <Input
                placeholder={t('FileSearchDescribe')}
                showCount
                maxLength={MAX_FILE_PATH_LENGTH}
                suffix={<Tooltip placement="bottom" title={t('RefreshDirectory')} ><RefreshIcon className={'icon-refresh'} onClick={searchCatalog}/></Tooltip>}
                value={inputPath}
                onChange={handleInputChange}
                onPressEnter={searchCatalog}
                data-testid="filePathInput"
            />
            <Text type={hit.alert ? 'danger' : undefined}>{t(hit.message, hit.options)}</Text>
            <ResourceCatalog
                actionListener={actionListener}
                onSelectedChange={handleSelectedChange}
                onSearchReturnChange={handleSearchReturn}
            />
        </FileExplorerContainer>
    </StyledModal>
    <FileConflictDialog
        open={conflictModalVis}
        error={checkResult}
        errors={checkErrors}
        onCancel={closeConflictModal}
        onConfrim={onContinue}
    />
    </>;
});

export default FileExplorer;
