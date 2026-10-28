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
import React, { useEffect, useRef, useState } from 'react';
import { observer } from 'mobx-react';
import styled from '@emotion/styled';
import { Form, Input, Modal, Typography } from 'antd';
import { useTranslation } from 'react-i18next';
import { Button, Tooltip, Select } from '@insight/lib/components';
import { RefreshIcon } from '@insight/lib/icon';
import { LocalStorageKey, localStorageService } from '@insight/lib';
import { checkPathValid, fileExist, getLastFilePath, getSearchDir, getTrimedPath } from '@/utils/Resource';
import type { ProjectCheckErrorDetail, ProjectCheckResult } from '@/utils/Resource';
import { ProjectAction, ProjectError } from '@/utils/enum';
import { type Project } from '@/centralServer/websocket/defs';
import type { FileSortBy, FileSortOrder } from '@/utils/Request';
import type { CatalogActionListener, SearchResult } from './ResourceCatalog';
import ResourceCatalog, { CatalogAction } from './ResourceCatalog';
import { handleProjectAction } from '@/utils/Project';
import { getInsightFileSelectorConfigUrl, loadInsightFileSelectorDefaultDirectory } from '@/utils/defaultBrowseDirectory';
import FileConflictDialog from './FileConflictDialog';

const orderStorageKey = (sortBy: FileSortBy): LocalStorageKey => {
    return sortBy === 'mtime' ? LocalStorageKey.IMPORT_SORT_ORDER_MTIME : LocalStorageKey.IMPORT_SORT_ORDER_NAME;
};

const readStoredSortBy = (): FileSortBy => {
    const value = localStorageService.getItem(LocalStorageKey.IMPORT_SORT_BY);
    return value === 'mtime' ? 'mtime' : 'name';
};

const readStoredSortOrder = (sortBy: FileSortBy): FileSortOrder => {
    const value = localStorageService.getItem(orderStorageKey(sortBy));
    if (value === 'asc' || value === 'desc') {
        return value;
    }
    return sortBy === 'mtime' ? 'desc' : 'asc';
};

const { Text } = Typography;
const { TextArea } = Input;

const FileExplorerContainer = styled.div`
    display: flex;
    flex-direction: column;
    flex: 1;
    min-width: 0;
    min-height: 0;

    .project-name, .import-tips, .ant-form-item, .ant-typography {
        flex-shrink: 0;
    }
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
    .ant-input-textarea {
        width: 100%;
    }
    .ant-input-textarea-show-count::after {
        color: ${(props): string => props.theme.infoColor};
    }
    .ant-input-affix-wrapper-textarea-with-clear-btn {
        background: ${(props): string => props.theme.bgColor} !important;
        border-color: ${(props): string => props.theme.borderColorLight};
        &:not(.ant-input-affix-wrapper-disabled):hover,
        &.ant-input-affix-wrapper-focused {
            border-color: ${(props): string => props.theme.primaryColor};
        }
        &.ant-input-affix-wrapper-status-error,
        &.ant-input-affix-wrapper-status-error:hover,
        &.ant-input-affix-wrapper-status-error:focus,
        &.ant-input-affix-wrapper-status-error.ant-input-affix-wrapper-focused {
            background: ${(props): string => props.theme.bgColor} !important;
            border-color: ${(props): string => props.theme.dangerColor};
        }
        > textarea.ant-input {
            background: transparent !important;
        }
    }
    .ant-input-clear-icon {
        color: ${(props): string => props.theme.textColorTertiary};
        &:hover {
            color: ${(props): string => props.theme.textColorPrimary};
        }
    }
    textarea.ant-input {
        resize: none;
        background-color: ${(props): string => props.theme.bgColor};
        border-color: ${(props): string => props.theme.borderColorLight};
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        &:hover, &:focus {
            border-color: ${(props): string => props.theme.primaryColor};
        }
        &::placeholder {
            color: ${(props): string => props.theme.textColorTertiary};
        }
    }
    //提示文字
    .ant-typography{
        display: inline-block;
        margin-top: 4px;
        color: ${(props): string => props.theme.textColorTertiary};
        font-size: 12px;
    }
    .ant-form-item {
        margin-bottom: 0;
    }
    .ant-form-item-explain-error {
        margin-top: 4px;
        color: ${(props): string => props.theme.dangerColor};
        font-size: 12px;
    }
`;

const StyledModal = styled(Modal)`
    padding-bottom: 0;

    .ant-modal-content {
        display: flex;
        flex-direction: column;
        max-height: calc(100vh - 48px);
    }
    .ant-modal-header, .ant-modal-footer {
        flex-shrink: 0;
    }
    .ant-modal-body {
        display: flex;
        min-height: 0;
        overflow-y: auto;
    }
    .ant-modal-footer {
        padding: 10px 0;
    }
    .modal-title {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding-right: 30px;
    }
    .modal-refresh {
        cursor: pointer;
        color: ${(props): string => props.theme.textColorSecondary};
        &:hover {
            color: ${(props): string => props.theme.primaryColor};
        }
    }
    .modal-sort {
        display: flex;
        align-items: center;
        gap: 8px;
        margin-right: 12px;
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
    const { t, i18n } = useTranslation('framework');
    const [inputPath, setInputPath] = useState(getLastFilePath());
    const [selectedPath, setSelectedPath] = useState('');
    const [actionListener, setActionListener] = useState<CatalogActionListener>({ type: CatalogAction.NO_ACTION });
    const [hit, setHit] = useState<{alert: boolean;message: string;options?: Record<string, string | number>}>({ alert: false, message: 'FileSearchDescribe' });
    const [conflictModalVis, setConflictModalVis] = useState<boolean>(false);
    const [checkResult, setCheckResult] = useState<ProjectError>(ProjectError.NO_ERRORS);
    const [checkErrors, setCheckErrors] = useState<ProjectCheckErrorDetail[]>([]);
    const [confirmLoading, setConfirmLoading] = useState<boolean>(false);
    const initialSortBy = readStoredSortBy();
    const [sortBy, setSortBy] = useState<FileSortBy>(initialSortBy);
    const [order, setOrder] = useState<FileSortOrder>(readStoredSortOrder(initialSortBy));
    const actionNonce = useRef(0);
    const initialDirectoryApplied = useRef(false);

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
    const searchCatalog = (value = inputPath): void => {
        initialDirectoryApplied.current = true;
        const path = getSearchDir(value);
        setInputPath(path);
        actionNonce.current += 1;
        setActionListener({ type: CatalogAction.SEARCH, value: path, nonce: actionNonce.current });
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

    const handleInputChange = (e: React.ChangeEvent<HTMLTextAreaElement>): void => {
        initialDirectoryApplied.current = true;
        setInputPath(e.target.value);
        setActionListener({ type: CatalogAction.INPUT_PATH_CHANGE, value: e.target.value, nonce: actionNonce.current });
    };

    const handleSelectedChange = (val: string): void => {
        setSelectedPath(val);
        if (val !== '' && val !== inputPath) {
            initialDirectoryApplied.current = true;
            setInputPath(val);
        }
    };

    // 每次打开对话框，刷新目录
    useEffect(() => {
        let active = true;
        if (dialogOpen) {
            if (initialDirectoryApplied.current || !getInsightFileSelectorConfigUrl(window.location.href)) {
                searchCatalog();
            } else {
                void loadInsightFileSelectorDefaultDirectory().then(path => {
                    if (!active || initialDirectoryApplied.current) {
                        return;
                    }
                    searchCatalog(path || inputPath);
                }).catch(() => {
                    if (active && !initialDirectoryApplied.current) {
                        setHit({ alert: true, message: 'FileCheckFailedDescribe' });
                    }
                });
            }
        }
        return () => { active = false; };
    }, [dialogOpen]);

    // 输入路径更新后，清掉告警
    useEffect(() => {
        setHit({ alert: false, message: 'FileSearchDescribe' });
    }, [inputPath]);

    const applySortBy = (next: FileSortBy): void => {
        setSortBy(next);
        setOrder(readStoredSortOrder(next));
        localStorageService.setItem(LocalStorageKey.IMPORT_SORT_BY, next);
    };

    const applyOrder = (next: FileSortOrder): void => {
        setOrder(next);
        localStorageService.setItem(orderStorageKey(sortBy), next);
    };

    const modalTitle = <div className="modal-title">
        <span>{t('Import File Dialog Title')}</span>
        <div style={{ display: 'flex', alignItems: 'center' }}>
            <div className="modal-sort" data-testid="importSortControls">
                <Select
                    size="small"
                    width={88}
                    value={sortBy}
                    aria-label={t('SortBy')}
                    // 语言切换时强制刷新选项文案
                    key={`sort-by-${i18n.language}`}
                    onChange={(value: FileSortBy): void => applySortBy(value)}
                    options={[
                        { value: 'name', label: t('SortByName') },
                        { value: 'mtime', label: t('SortByDate') },
                    ]}
                    getPopupContainer={(trigger: HTMLElement): HTMLElement => trigger.parentElement ?? document.body}
                />
                <Select
                    size="small"
                    width={100}
                    value={order}
                    aria-label={t('SortOrder')}
                    key={`sort-order-${i18n.language}`}
                    onChange={(value: FileSortOrder): void => applyOrder(value)}
                    options={[
                        { value: 'asc', label: t('SortAsc') },
                        { value: 'desc', label: t('SortDesc') },
                    ]}
                    getPopupContainer={(trigger: HTMLElement): HTMLElement => trigger.parentElement ?? document.body}
                />
            </div>
            <Tooltip placement="bottom" title={t('RefreshDirectory')}>
                <RefreshIcon className="modal-refresh" onClick={(): void => searchCatalog()}/>
            </Tooltip>
        </div>
    </div>;

    return <><StyledModal title={modalTitle} open={dialogOpen} onCancel={handleCancel}
        width={800}
        centered
        footer={<div>
            <Button onClick={handleConfirm} loading={confirmLoading} type="primary" style={{ marginRight: 8 }} disabled={selectedPath === ''}>{t('Confirm')}</Button>
            <Button onClick={handleCancel}>{t('Cancel')}</Button>
        </div>}>
        <FileExplorerContainer>
            {!currentProject ? <></> : <span className="project-name">{t('Current Project')} ：{currentProject}</span>}
            {importTips ? <div className="import-tips">{importTips}</div> : null}
            <Form.Item
                validateStatus={hit.alert ? 'error' : undefined}
                help={hit.alert ? t(hit.message, hit.options) : undefined}
            >
                <TextArea
                    placeholder={t('FileSearchDescribe')}
                    allowClear
                    showCount
                    maxLength={MAX_FILE_PATH_LENGTH}
                    autoSize={{ minRows: 1, maxRows: 2 }}
                    value={inputPath}
                    onChange={handleInputChange}
                    onPressEnter={(event): void => {
                        event.preventDefault();
                        searchCatalog();
                    }}
                    data-testid="filePathInput"
                />
            </Form.Item>
            {!hit.alert && <Text>{t(hit.message, hit.options)}</Text>}
            <ResourceCatalog
                actionListener={actionListener}
                sortBy={sortBy}
                order={order}
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
