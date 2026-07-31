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
import { runInAction } from 'mobx';
import { Session } from '@/entity/session';
import ImportData from './ImportData/Index';
import styled from '@emotion/styled';
import ProjectContents from './ProjectContents/Index';
import { useTranslation } from 'react-i18next';

const Container = styled.div`
    color:${(props): string => props.theme.textColorPrimary};
`;

const Header = styled.div`
    height: 36px;
    padding: 0 8px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 14px;
    line-height: 36px;
    font-weight: 500;
    user-select: none;
`;

interface IProps {
    session: Session;
}

const Index = observer(({ session }: IProps) => {
    const { t } = useTranslation('framework');
    const [checkedProjectKeys, setCheckedProjectKeys] = useState<React.Key[]>([]);
    const projectNames = session.dataSources.map(dataSource => dataSource.projectName);
    const validCheckedProjectKeys = checkedProjectKeys.filter(key => projectNames.includes(String(key)));

    useEffect(() => {
        if (!session.projectContentEditStatus) {
            setCheckedProjectKeys([]);
        }
    }, [session.projectContentEditStatus]);

    useEffect(() => {
        if (session.projectContentEditStatus && session.dataSources.length === 0) {
            runInAction(() => {
                session.projectContentEditStatus = false;
            });
        }
    }, [session.dataSources.length, session.projectContentEditStatus]);

    return <Container>
        {!session.projectContentEditStatus && <Header>{ t('Data Manager')}</Header>}
        <ImportData session={session} checkedProjectKeys={validCheckedProjectKeys}/>
        <ProjectContents checkedProjectKeys={validCheckedProjectKeys} setCheckedProjectKeys={setCheckedProjectKeys}/>
    </Container>;
});

export default Index;
