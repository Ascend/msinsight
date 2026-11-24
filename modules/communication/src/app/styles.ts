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
import { FilterRow } from '../components/filters';

export const Page = styled.main(({ theme }) => ({
    height: '100vh',
    overflow: 'auto',
    minWidth: 820,
    color: theme.textColorPrimary,
    backgroundColor: theme.mode === 'light' ? theme.backgroundColor : theme.bgColorDark,
}));

export const PageContent = styled.div(({ theme }) => ({
    margin: theme.pagePadding,
    padding: theme.pagePadding,
    border: `1px solid ${theme.borderColorLight}`,
    borderRadius: `calc(${theme.borderRadiusBase} * 2)`,
    backgroundColor: theme.bgColor,
}));

export const ParsingStatus = styled.div(({ theme }) => ({
    minHeight: 240,
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    gap: theme.pagePadding,
    textAlign: 'center',
    fontSize: theme.fontSizeSmall,
    lineHeight: 1.6,
    color: theme.textColorPrimary,
    '.ant-spin': { color: theme.primaryColor },
    '.ant-spin-dot-item': { backgroundColor: theme.primaryColor },
    p: {
        margin: '8px 0 0',
        fontSize: theme.fontSizeExtraSmall,
        color: theme.textColorSecondary,
    },
}));

export const PageFilterRow = styled(FilterRow)(({ theme }) => ({
    margin: theme.pagePadding,
    '> label:first-of-type': {
        minWidth: 0,
        maxWidth: '100%',
        '.ant-select': {
            minWidth: 160,
            maxWidth: '100%',
        },
    },
}));

export const ScopeFilterRow = styled.div({
    display: 'inline-flex', alignItems: 'center', padding: '12px 16px 12px 0', gap: 16, marginBottom: 8,
});
