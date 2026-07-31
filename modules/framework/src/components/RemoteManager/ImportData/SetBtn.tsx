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
import React, { useEffect } from 'react';
import { observer } from 'mobx-react';
import { Session } from '@/entity/session';
import { SetIcon } from '@insight/lib/icon';
import { BtnItem } from './Index';
import { runInAction } from 'mobx';

// 设置（按钮）
const SetBtn = observer(({ session }: {session: Session}) => {
    const enterEditMode = (): void => {
        runInAction(() => {
            session.projectContentEditStatus = true;
        });
    };
    useEffect(() => {
        // 没有数据时
        if (session.dataSources.length === 0) {
            runInAction(() => {
                session.projectContentEditStatus = false;
            });
        };
    }, [session.dataSources.length]);
    return <BtnItem className={`btn-set ${session.dataSources.length > 0 ? '' : 'disabled'}`} data-testid="btn-set" onClick={enterEditMode}>
        <SetIcon/>
    </BtnItem>;
});

export default SetBtn;
