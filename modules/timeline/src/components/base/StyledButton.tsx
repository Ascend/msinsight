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
import styled from '@emotion/styled';
import type { ButtonProps } from 'antd/lib/button';
import { Button } from 'antd';
import * as React from 'react';
import { useTheme } from '@emotion/react';
import type { ForwardedRef, FunctionComponent, SVGProps } from 'react';
import { Tooltip } from '@insight/lib/components';

interface CustomButtonProps {
    isEmphasize?: boolean;
    isDisabled?: boolean;
    isSuspend?: boolean;
    tooltip?: string | null;
    icon?: FunctionComponent<SVGProps<SVGSVGElement> & { title?: string }>;
}

interface StyledButtonProps {
    width?: number;
    transparent?: boolean;
}

const StyledButtonBase = React.forwardRef(({ transparent, width, ...props }: StyledButtonProps & ButtonProps, ref: React.ForwardedRef<React.ComponentRef<typeof Button>>) => (
    <Button {...props} ref={ref} />
));
StyledButtonBase.displayName = 'StyledButtonBase';

export const StyledButton = styled(StyledButtonBase)<StyledButtonProps>`
    display: flex;
    align-items: center;
    width: ${(props): string => props.width !== undefined ? `${props.width}px` : '22px'};
    height: 20px;
    line-height: 20px;
    color: ${(props): string => props.theme.textColorPrimary};
    background-color: ${(props): string => props.transparent ? 'transparent' : props?.style?.backgroundColor ?? props.theme.bgColorLight};
    &:hover {
        background-color: ${(props): string => props.transparent ? 'transparent' : props?.style?.backgroundColor ?? props.theme.primaryColorHover};
        cursor: ${(props): string => props.disabled as boolean ? 'not-allowed' : 'pointer'};
    }

    border-radius: 4px;
    justify-content: center;
    border: none;
    margin-right: 3px;

    &:before {
        content: ${(props): string => props.transparent ? 'normal' : ''};
    }
`;
StyledButton.displayName = 'StyledButton';

export const CustomButton = React.forwardRef(({ icon, isDisabled, isSuspend, tooltip, isEmphasize, ...props }: ButtonProps & CustomButtonProps,
    ref?: ForwardedRef<HTMLButtonElement>): JSX.Element => {
    const theme = useTheme();
    let buttonFillColor = theme.buttonColor.enableClickColor;
    if (isDisabled) {
        buttonFillColor = theme.buttonColor.disableClickColor;
    } else {
        if (isEmphasize) {
            buttonFillColor = theme.buttonColor.emphasizeColor;
        }
    }

    const Icon = icon;
    return <Tooltip title={tooltip}>
        <StyledButton ref={ref} disabled={isDisabled} {...props} icon={Icon && <Icon fill={buttonFillColor}/>}
            style={{ backgroundColor: isSuspend ? theme.buttonColor.suspendBGColor : theme.buttonColor.unSuspendBGColor }}>
        </StyledButton>
    </Tooltip>;
});

CustomButton.displayName = 'CustomButton';
