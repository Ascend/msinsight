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
import { useTheme } from '@emotion/react';
import styled from '@emotion/styled';
import { agentKindLogoAsset } from '../agentBrand';

const LogoImage = styled.img`
    width: 100%;
    height: 100%;
    display: block;
    object-fit: contain;
`;

export const AgentKindIcon = ({ name }: { name?: string }): JSX.Element | null => {
    const theme = useTheme();
    const logo = agentKindLogoAsset({ name });
    if (!logo) return null;
    const src = theme.mode === 'dark' && logo.darkSrc ? logo.darkSrc : logo.src;
    return <LogoImage alt="" data-agent-icon={logo.darkSrc ? 'themed' : 'static'} src={src} />;
};
