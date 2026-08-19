/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio Insight is licensed under Mulan PSL v2.
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
import styled from '@emotion/styled';
import { InsightBotLogo } from '@insight/lib/icon';

const EntryButton = styled.button`
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 0;
    border: 0;
    color: ${({ theme }): string => theme.textColorPrimary};
    font: inherit;
    font-size: 14px;
    line-height: 24px;
    background: transparent;
    transition: opacity 0.2s ease;

    &:hover {
        opacity: 0.72;
    }

    &:focus-visible {
        outline: 2px solid ${({ theme }): string => theme.primaryColor};
        outline-offset: 3px;
        border-radius: 2px;
    }

    img {
        width: 20px;
        height: 20px;
        object-fit: contain;
    }
`;

interface InsightBotEntryProps {
    active: boolean;
    onClick: () => void;
}

const InsightBotEntry = ({ active, onClick }: InsightBotEntryProps): JSX.Element => {
    return <EntryButton
        type="button"
        aria-label="InsightBot"
        aria-pressed={active}
        onClick={onClick}
    >
        <img src={InsightBotLogo} alt="" />
        <span>InsightBot</span>
    </EntryButton>;
};

export default InsightBotEntry;
