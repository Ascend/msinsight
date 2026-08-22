/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import styled from '@emotion/styled';
import { Alert } from 'antd';
import type { AlertProps } from 'antd/lib/alert';
import React from 'react';

const AlertBase = (props: AlertProps): JSX.Element => {
    return <Alert showIcon type="info" {...props} />;
};

export const MIAlert = styled(AlertBase)`
    width: 100%;
    border: 0;
    border-radius: ${(props): string => props.theme.borderRadiusLarge};
    padding: 8px 12px;
    background: ${(props): string => props.theme.primaryColorLight5};

    .ant-alert-icon {
        margin-right: 10px;
        color: ${(props): string => props.theme.primaryColor};
        font-size: 16px;
    }

    .ant-alert-message {
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        font-weight: 400;
        line-height: 22px;
    }
`;
