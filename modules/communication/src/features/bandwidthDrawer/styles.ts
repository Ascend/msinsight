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
import { Drawer } from 'antd';
import { TwoColumnGrid } from '../../components/layout';
import { EmptyStatus } from '../../components/Section';

export const DrawerPanel = styled(Drawer)(({ theme }) => ({
    '.ant-drawer-body': { overscrollBehavior: 'contain' },
    '.ant-drawer-content, .ant-drawer-header': { backgroundColor: theme.bgColor, color: theme.textColorPrimary },
    '.ant-drawer-header': { borderColor: theme.borderColorLight },
    '.ant-drawer-title': { margin: 0, fontSize: theme.fontSizeMedium, fontWeight: 600, color: theme.textColorPrimary },
    '.ant-drawer-close': { color: theme.textColorPrimary },
}));
export const PacketGrid = styled(TwoColumnGrid)({ margin: '16px 0 24px' });
export const PacketCell = styled.div({ padding: 16 });
export const PacketEmpty = styled(EmptyStatus)({ gridColumn: '1 / -1' });
