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
import { Select } from 'antd';
import type { DefaultOptionType } from 'rc-select/lib/Select';
import type { BaseSelectRef } from 'rc-select/lib/BaseSelect';
import styled from '@emotion/styled';
import caretDownIcon from '../icon/img/caret.svg';
import removeIcon from '../icon/img/select_multiple_remove.svg';
import type { SelectProps } from 'antd/lib/select';
import { useTranslation } from 'react-i18next';

const sizeOption = {
    small: 100,
    middle: 160,
    large: 240,
};

const NoDataContent = styled.div`
    color: ${(props): string => props.theme.textColorPlaceholder};
`;

const StyledSuffixIcon = styled.div`
    height: 12px;
    width: 12px;
    background-image: url(${caretDownIcon});
    background-size: cover;
`;

const StyledRemoveIcon = styled.div`
    height: 16px;
    width: 16px;
    background-image: url(${removeIcon});
    background-size: cover;
`;

interface MISelectProps<T extends DefaultOptionType = DefaultOptionType> extends SelectProps {
    width?: number | string;
    height?: number;
    name?: string;
    options?: T[];
    optionRender?: (option: T) => JSX.Element;
}

const BaseMISelect = <T extends DefaultOptionType>(props: MISelectProps<T>): JSX.Element => {
    const { t } = useTranslation('lib');
    const { size, dropdownMatchSelectWidth, options, optionRender, ...restProps } = props;
    return <Select
        suffixIcon={<StyledSuffixIcon />}
        removeIcon={<StyledRemoveIcon />}
        placeholder={t('Please select')}
        dropdownMatchSelectWidth={dropdownMatchSelectWidth ?? false}
        notFoundContent={<NoDataContent>{t('No data')}</NoDataContent>}
        {...restProps}
    >
        {Array.isArray(options) && options.map(option =>
            optionRender
                ? (
                    <Select.Option key={option.value} value={option.value} label={option.label} disabled={Boolean(option.disabled)}>
                        {optionRender(option as T)}
                    </Select.Option>
                )
                : (
                    <Select.Option key={option.value} value={option.value} label={option.label} disabled={Boolean(option.disabled)}>
                        {option.label}
                    </Select.Option>
                ),
        )}
    </Select>;
};

const StyledMISelect = styled(BaseMISelect)`
    height: ${(props): string => props.height ? `${props.height}px` : 'auto'};
    width: ${(props): string => {
        if (typeof props.width === 'string') {
            if (!isNaN(Number(props.width))) {
                return `${props.width}px`;
            }
            return props.width;
        }
        return `${props.width ?? (props.size && sizeOption[props.size]) ?? sizeOption.middle}px`;
    }
};
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;

    &:not(.ant-select-customize-input) .ant-select-selector {
        background-color: ${(props): string => props.theme.bgColor};
        border-color: ${(props): string => props.theme.borderColor};
    }

    &:hover, &.ant-select-focused:not(.ant-select-disabled) {
        &:not(.ant-select-customize-input) .ant-select-selector {
            border-color: ${(props): string => props.theme.primaryColor};
        }
    }

    &.ant-select-disabled:not(.ant-select-customize-input) .ant-select-selector {
        color: ${(props): string => props.theme.borderColorDisabled};
        background-color: ${(props): string => props.theme.bgColorDisabled};
        border-color: ${(props): string => props.theme.textColorPlaceholder};
    }

    &.ant-select-open > .ant-select-arrow > div {
        transform: rotate(180deg);
    }

    &.ant-select-multiple .ant-select-selection-item {
        background-color: ${(props): string => props.theme.borderColorLight};
        min-width: 48px;
        padding: 0 8px;
        justify-content: space-between;
    }

    .ant-select-selection-placeholder {
        color: ${(props): string => props.theme.textColorPlaceholder};
    }

    .ant-select-clear {
        color: ${(props): string => props.theme.textColorTertiary};
        background-color: transparent;
        &:hover {
            color: ${(props): string => props.theme.textColorPrimary};
        }
    }
`;

// 强制类型断言，使 StyledMISelected 支持泛型 T
type GenericSelectComponent = <T extends DefaultOptionType = DefaultOptionType>(
    props: React.PropsWithChildren<MISelectProps<T>> & { ref?: React.Ref<BaseSelectRef> }
) => JSX.Element;

export const MISelect = StyledMISelect as GenericSelectComponent;
