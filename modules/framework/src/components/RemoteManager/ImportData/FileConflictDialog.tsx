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
import { Button } from '@insight/lib/components';
import styled from '@emotion/styled';
import { Modal } from 'antd';
import { useTranslation } from 'react-i18next';
import { ProjectError } from '@/utils/enum';
import type { ProjectCheckErrorDetail } from '@/utils/Resource';

const StyledModal = styled(Modal)`
    .ant-modal-footer {
        padding: 10px 0;
    }
    .error-detail-list {
        max-height: 240px;
        overflow: auto;
        margin-top: 8px;
        white-space: pre-wrap;
        word-break: break-all;
    }
    .error-detail-item {
        margin-top: 8px;
    }
    .error-detail-path {
        font-weight: 600;
    }
`;

interface IProps {
    error: ProjectError;
    errors?: ProjectCheckErrorDetail[];
    open: boolean;
    onConfrim: () => void;
    onCancel: () => void;
}

const TITLE_MAP = new Map([
    [ProjectError.FILE_NOT_EXIST, 'FileNotExist'],
    [ProjectError.PROJECT_NAME_CONFLICT, 'FileConflict'],
    [ProjectError.IS_UNSAFE_PATH, 'FileUnsafe'],
    [ProjectError.EXISTING_LARGE_FILES, 'LargeFile'],
    [ProjectError.EXCEEDS_MXIMUN_LENGTH, 'PathLengthExceeded'],
    [ProjectError.IS_NOT_REGULAR_FILE, 'NotRegularFile'],
]);

const FileConflictDialog = ({ error, errors = [], open, onConfrim, onCancel }: IProps): JSX.Element => {
    const { t } = useTranslation('framework');

    const title = (TITLE_MAP.get(error) ?? error) as string;
    const content = TITLE_MAP.get(error) !== undefined ? `${TITLE_MAP.get(error)}Content` : error;
    return <StyledModal
        maskClosable={false} title={t(title)} width={errors.length > 0 ? 520 : 320} open={open} onCancel={onCancel}
        footer={<div>
            {[ProjectError.IS_UNSAFE_PATH, ProjectError.FILE_NOT_EXIST].includes(error)
                ? <></>
                : <Button onClick={onConfrim} type="primary" size="small" style={{ marginRight: 8 }} >{t('Confirm')}</Button>}
            <Button onClick={onCancel} size="small">{t('Cancel')}</Button>
        </div>}
    >
        {t(content as string)}
        {errors.length > 0 && <div className="error-detail-list">
            {errors.map(item => (
                <div className="error-detail-item" key={`${item.layer}-${item.path}-${item.message}`}>
                    <div className="error-detail-path">{item.path}</div>
                    <div>{item.message}</div>
                </div>
            ))}
        </div>}
    </StyledModal>;
};

export default FileConflictDialog;
