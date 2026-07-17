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
import { useTranslation } from 'react-i18next';
import { ImportDataIcon } from '@insight/lib/icon';
import { type Session } from '@/entity/session';
import { SessionAction } from '@/utils/enum';
import FileExplorer from './FileExplorer';
import SetBtn from './SetBtn';
import connector from '@/connection';

const ImportContainer = styled.div`
    display: flex;
    align-items: center;
    padding: 8px;
    & > div:first-of-type {
        margin-right: 8px;
        flex: 1;
    }
    .btn-import > div:first-of-type{
        margin-right: 8px;
    }
`;

export const BtnItem = styled.div`
    display: flex;
    justify-content: center;
    padding: 8px;
    background: ${(props): string => props.theme.bgColorCommon};
    border-radius: var(--mi-border-radius-base);
    cursor: pointer;
    font-size: 12px;
    color: ${(props): string => props.theme.textColorPrimary};
    :not(.disabled):hover {
        color: ${(props): string => props.theme.primaryColor};
        transition: .3s;
    }

    &.disabled {
        cursor: not-allowed;
        pointer-events: none;
        color: ${(props): string => props.theme.textColorDisabled};
        &:hover {
            color: ${(props): string => props.theme.textColorDisabled};
        }
    }
    &.danger:hover {
        color: ${(props): string => props.theme.dangerColor};
    }
    &.small {
        padding: 4px;
    }
`;

const ImportData = observer(({ session }: {session: Session}) => {
    const { t } = useTranslation('framework');
    const [dialogOpen, setDialogOpen] = useState(false);
    const [currentProject, setCurrentProject] = useState('');
    const [customImport, setCustomImport] = useState(false);
    const [importTips, setImportTips] = useState('');

    const openDialog = (): void => {
        setDialogOpen(true);
    };
    const closeDialog = (): void => {
        setCustomImport(false);
        setDialogOpen(false);
        setImportTips('');
    };
    // 新导入数据
    const importData = (): void => {
        setCurrentProject('');
        openDialog();
    };

    const handleConfirm = (path: string): void => {
        connector.send({
            event: 'importExpertLoadDataConfirm',
            body: { path },
            to: 'Summary',
        });
    };

    const handleCancel = (): void => {
        session.resetActionListener();
    };

    useEffect(() => {
        switch (session.actionListener.type) {
            // 在已有项目下导入
            case SessionAction.ADD_DATA_UNDER_PROJECT:
                setCurrentProject(session.actionListener.value);
                openDialog();
                break;
            // Summary 专家负载均衡导入
            case SessionAction.IMPORT_MOE_LOAD_DATA:
                setCustomImport(true);
                setCurrentProject('');
                setImportTips(t('ImportExpertHotspotDataTips'));
                openDialog();
                break;
            default:
                break;
        }
    }, [session.actionListener]);
    return <>
        <ImportContainer>
            <BtnItem onClick={importData} className="btn-import"><ImportDataIcon/><span>{t('Import Data')}</span></BtnItem>
            <SetBtn session={session}/>
        </ImportContainer>
        <FileExplorer
            onConfirm={handleConfirm}
            onCancel={handleCancel}
            customImport={customImport}
            importTips={importTips}
            currentProject={currentProject}
            dialogOpen={dialogOpen}
            closeDialog={closeDialog}
        />
    </>;
});

export default ImportData;
