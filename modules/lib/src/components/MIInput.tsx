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
import { type InputProps, type InputRef, type InputNumberProps, Input, InputNumber } from 'antd';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import type { GroupProps } from 'antd/es/input';

// 默认最大输入200个字符
const DEFAULT_MAX_LENGTH = 200;
// 默认输入数字范围为32位整数范围
const DEFAULT_MIN_INPUT_NUMBER = -2147483648;
const DEFAULT_MAX_INPUT_NUMBER = 2147483647;

const sizeOption = {
    small: 100,
    middle: 160,
    large: 240,
};
const Support = React.forwardRef((props: InputProps, ref: React.ForwardedRef<InputRef>) => {
    const { t } = useTranslation('lib');
    // size在styled中使用
    const { size, ...restProps } = props;
    return <Input
        maxLength={DEFAULT_MAX_LENGTH}
        placeholder={t('Please enter')}
        {...restProps} ref={ref} />;
},
);
Support.displayName = 'Support';

export const MIInput = styled(Support)`
    width: ${(props): number => (props.size && sizeOption[props.size]) ?? sizeOption.middle}px;
    height: ${(props): number | string => (props.height ?? 32)}px;
    background-color: ${(props): string => props.theme.bgColor};
    border-color: ${(props): string => props.theme.borderColorLighter};
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;

    &:not(.ant-input-affix-wrapper-disabled):hover, &.ant-input-affix-wrapper-focused {
        border-color: ${(props): string => props.theme.primaryColor};
    }

    .ant-input {
        background-color: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        &::placeholder {
            color: ${(props): string => props.theme.textColorTertiary};
        }
    }

    .ant-input-clear-icon {
        color: ${(props): string => props.theme.textColorTertiary};
        &:hover {
            color: ${(props): string => props.theme.textColorPrimary};
        }
    }
`;

export const MIInputSplit = styled(MIInput)`
    width: 30px !important;
    border-left: 0;
    border-right: 0;
    pointer-events: none;

    &.ant-input[disabled] {
        background-color: ${(props): string => props.theme.bgColor};
        border-color: ${(props): string => props.theme.borderColorLighter};
    }
`;

export const MIInputGroup = React.forwardRef((props: GroupProps, _ref: React.ForwardedRef<unknown>) => (
    <Input.Group {...props} />
));

type ValueType = string | number;
export const MIInputNumber = styled((props: InputNumberProps<ValueType> & {
    children?: React.ReactNode;
    center?: boolean;
} & {
    ref?: React.Ref<HTMLInputElement>;
}) => {
    const { size, ...restProps } = props;
    return (
        <InputNumber
            min={DEFAULT_MIN_INPUT_NUMBER}
            max={DEFAULT_MAX_INPUT_NUMBER}
            maxLength={DEFAULT_MAX_LENGTH}
            {...restProps}
        />
    );
})`
    width: ${(props): number => (props.size && sizeOption[props.size]) ?? sizeOption.middle}px;
    height: ${(props): number | string => (props.height ?? 32)}px;
    background-color: ${(props): string => props.theme.bgColor};
    border-color: ${(props): string => props.theme.borderColorLighter};
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;

    &:hover, &.ant-input-number-focused {
        border-color: ${(props): string => props.theme.primaryColor};
    }

    .ant-input-number-handler-wrap{
        background-color: transparent;
        .ant-input-number-handler {
            border-color: ${(props): string => props.theme.borderColorLighter};
            > span {
                color: ${(props): string => props.theme.textColorTertiary};
            }
            &:hover >span {
                color: ${(props): string => props.theme.primaryColor};
            }

            &.ant-input-number-handler-up-disabled, &.ant-input-number-handler-down-disabled {
                > span {
                    color: ${(props): string => props.theme.textColorTertiary};
                }
            }
        }
    }

    .ant-input-number-input {
        text-align: ${(props): string => props.center ? 'center' : 'left'};
    }
`;
