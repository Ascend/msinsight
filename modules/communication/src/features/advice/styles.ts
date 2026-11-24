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
import { Subtitle } from '../../components/layout';

export const AdviceList = styled.div({ display: 'flex', flexDirection: 'column', gap: 12 });
export const DiagnosisCard = styled.article(({ theme }) => ({
    padding: '12px 16px', borderRadius: `calc(${theme.borderRadiusBase} * 2)`, backgroundColor: theme.bgColor,
}));
export const DiagnosisTitle = styled(Subtitle)({ marginBottom: 8 });
export const DiagnosisTable = styled.div({ marginTop: 8 });
export const Recommendations = styled.div({ marginTop: 12, 'p + p': { marginTop: 6 } });
