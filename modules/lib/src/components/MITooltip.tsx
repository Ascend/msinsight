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

import React from 'react';
import { Tooltip } from 'antd';
import type { TooltipProps } from 'antd/lib/tooltip';
import { useTheme } from '@emotion/react';
import { QuestionCircleOutlined } from '@ant-design/icons';

export const MITooltip = React.forwardRef(({ children, overlayInnerStyle, ...props }: TooltipProps, ref: React.ForwardedRef<React.ComponentRef<typeof Tooltip>>) => {
    const theme = useTheme();
    return <Tooltip
        color={theme.bgColorCommon}
        overlayInnerStyle={
            {
                borderRadius: 4,
                borderColor: theme.borderColorLight,
                color: theme.textColorPrimary,
                boxShadow: theme.boxShadow,
                whiteSpace: 'pre-wrap',
                padding: 8,
                fontSize: 12,
                maxWidth: 400,
                ...overlayInnerStyle,
            }
        }
        mouseEnterDelay={0.3}
        {...props}
        ref={ref}
    >
        {children}
    </Tooltip>;
});

MITooltip.displayName = 'MITooltip';

export const MITooltipHelp = React.forwardRef((props: TooltipProps, ref: React.ForwardedRef<React.ComponentRef<typeof Tooltip>>) => {
    const theme = useTheme();

    return <MITooltip {...props} ref={ref}>
        <QuestionCircleOutlined style={{ color: theme.icon }} />
    </MITooltip>;
});

MITooltipHelp.displayName = 'MITooltipHelp';
