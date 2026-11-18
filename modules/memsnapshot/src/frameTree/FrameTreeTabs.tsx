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
import { Tabs } from '@insight/lib/components';

export const FrameTreeTabs = styled(Tabs)`
    && > .ant-tabs-nav .ant-tabs-tab {
        color: ${(p): string => p.theme.textColorSecondary};
        background-color: ${(p): string => p.theme.bgColorLight};
        border-color: ${(p): string => p.theme.borderColorLight};
    }
    && > .ant-tabs-nav .ant-tabs-tab-active {
        background-color: ${(p): string => p.theme.bgColor};
        border-color: ${(p): string => p.theme.primaryColorDark};
    }
    && > .ant-tabs-nav .ant-tabs-tab-active .ant-tabs-tab-btn {
        color: ${(p): string => p.theme.mode === 'dark' ? p.theme.primaryColorDark : p.theme.textColorPrimary};
        font-weight: 600;
        text-shadow: none;
    }
    && > .ant-tabs-nav .ant-tabs-tab-remove { color: ${(p): string => p.theme.textColorSecondary}; }
    && > .ant-tabs-nav .ant-tabs-tab-remove:hover,
    && > .ant-tabs-nav .ant-tabs-tab-remove:focus-visible {
        color: ${(p): string => p.theme.textColorPrimary};
        background-color: ${(p): string => p.theme.bgColorLight};
    }
    && > .ant-tabs-nav .ant-tabs-tab-remove:focus-visible {
        outline: 2px solid ${(p): string => p.theme.primaryColorDark}; outline-offset: 2px;
    }
`;
