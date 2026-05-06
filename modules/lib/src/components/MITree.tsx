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
import React, { forwardRef } from 'react';
import { Tree, type TreeProps } from 'antd';
import type RcTree from 'rc-tree';
import styled from '@emotion/styled'; import type { BasicDataNode } from 'rc-tree';
import type { DataNode } from 'rc-tree/lib/interface';

const InternalMITree = forwardRef(function MITree<T extends BasicDataNode | DataNode = DataNode>(props: React.PropsWithChildren<TreeProps<T>> & {
    ref?: React.Ref<RcTree>;
}, _ref: React.ForwardedRef<RcTree>) {
    return (
        <Tree {...props} />
    );
});

const StyledMITree = styled(InternalMITree)`
    color: ${(props): string => props.theme.textColorPrimary};
    background: ${(props): string => props.theme.bgColorDark};
    font-size: 14px;
`;

// 强制类型断言，使 StyledMITree 支持泛型 T
type GenericTreeComponent = <T extends BasicDataNode | DataNode>(
    props: React.PropsWithChildren<TreeProps<T>> & { ref?: React.Ref<RcTree> }
) => JSX.Element;

export const MITree = StyledMITree as GenericTreeComponent;
