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

export const AdvicePanel = styled.section(({ theme }) => ({
    padding: 12,
    border: `1px solid ${theme.primaryColorLight2}`,
    borderRadius: `calc(${theme.borderRadiusBase} * 2)`,
    backgroundColor: theme.primaryColorLight5,
}));

export const AdviceParagraph = styled.p(({ theme }) => ({
    margin: 0,
    color: theme.textColorSecondary,
    fontSize: theme.fontSizeSmall,
    lineHeight: 1.6,
    overflowWrap: 'anywhere',
    strong: { color: theme.textColorPrimary, fontWeight: 600 },
}));
