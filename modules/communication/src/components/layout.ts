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

export const SectionTitle = styled.h2(({ theme }) => ({
    margin: 0, fontWeight: 600, color: 'inherit', fontSize: theme.fontSizeMedium,
}));

export const Subtitle = styled.h3(({ theme }) => ({
    margin: '0 0 12px', fontWeight: 600, color: 'inherit', fontSize: theme.fontSizeSmall,
}));

export const TwoColumnGrid = styled.div({
    display: 'grid',
    gridTemplateColumns: 'repeat(2, minmax(0, 1fr))',
    gap: 24,
    '@media (max-width: 1100px)': { gridTemplateColumns: 'minmax(0, 1fr)' },
});
