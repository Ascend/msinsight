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

import * as React from 'react';
import { Button } from 'antd';
import styled from '@emotion/styled';
import type { ButtonProps } from 'antd/lib/button';

const sizeOption = {
    large: {
        width: 80,
        height: 32,
    },
    small: {
        width: 64,
        height: 24,
    },
};

const ButtonWithRef = React.forwardRef<React.ComponentRef<typeof Button>, ButtonProps>(
    (props, ref) => <Button {...props} ref={ref} />,
);
ButtonWithRef.displayName = 'MIButton';

export const MIButton = styled(ButtonWithRef)`
    height: ${(props): number => props.size === 'large' ? sizeOption.large.height : sizeOption.small.height}px;
    min-width: ${(props): number => props.size === 'large' ? sizeOption.large.width : sizeOption.small.width}px;
    padding: ${(props): string => props.size === 'large' ? '0 16px' : '0 8px'};

    > span {
        font-size: 12px;
        color: ${(props): string => props.theme.textColorSecondary};
    }

    &.ant-btn-default {
        background-color: transparent;
        border-color: ${(props): string => props.theme.borderColorLighter};
        &:hover {
            border-color: ${(props): string => props.theme.primaryColorHover};
            > span {
                color: ${(props): string => props.theme.primaryColorHover};
            }
        }
        &:disabled {
            background-color: ${(props): string => props.theme.bgColorDisabled};
            border-color: ${(props): string => props.theme.borderColorDisabled};
            > span {
                color: ${(props): string => props.theme.borderColorDisabled};
            }
        }
    }

    &.ant-btn-primary {
        > span {
            color: ${(props): string => props.theme.textColorFourth};
        }
        background-color: ${(props): string => props.theme.primaryColor};
        border-color: ${(props): string => props.theme.primaryColor};
        &:hover {
            background-color: ${(props): string => props.theme.primaryColorHover};
            border-color: ${(props): string => props.theme.primaryColorHover};
        }
        &:disabled {
            background-color: ${(props): string => props.theme.primaryColorDisabled};
            border-color: ${(props): string => props.theme.primaryColorDisabled};
            > span {
                color: ${(props): string => props.theme.textColorFourth};
                opacity: 0.6;
            }
        }
    }

    &.ant-btn-link {
        > span {
            color: ${(props): string => props.theme.primaryColorHover};
        }
    }
`;
