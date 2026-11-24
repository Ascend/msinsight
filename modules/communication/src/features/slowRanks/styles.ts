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
import { SectionTitle } from '../../components/layout';
import { AdvicePanel, AdviceParagraph } from '../../components/AdvicePanel';

export const SlowRankPanel = styled(AdvicePanel)({ marginTop: 16 });
export const SlowRankTitle = styled(SectionTitle.withComponent('h3'))({ marginBottom: 12 });
export const SlowRankDescription = styled(AdviceParagraph)({ strong: { marginLeft: 8 } });
export const SlowRankTable = styled.div(({ theme }) => ({ marginTop: 12, backgroundColor: theme.bgColor }));
