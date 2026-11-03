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
import styled from '@emotion/styled/macro';
export const NoData = styled.div`
    font-size: 14px;
    color: ${(props): string => props.theme.tableTextColor};
`;

export const DetailTabsWrapper = styled.div`
    position: relative;
    display: flex;
    flex-direction: column;
    min-height: 0;
    height: 100%;

    > .ant-tabs {
        display: flex;
        flex: 1;
        flex-direction: column;
        min-height: 0;
    }

    > .ant-tabs > .ant-tabs-nav {
        margin-bottom: 6px;
    }

    > .ant-tabs > .ant-tabs-content-holder {
        flex: 1;
        min-height: 0;
        overflow: hidden;
    }

    > .ant-tabs > .ant-tabs-content-holder > .ant-tabs-content {
        height: 100%;
    }

    > .ant-tabs > .ant-tabs-content-holder > .ant-tabs-content > .ant-tabs-tabpane {
        height: 100%;
        overflow: hidden;
    }

    .ant-tabs-card > .ant-tabs-nav .ant-tabs-tab {
        padding: 2px 8px;
        max-width: 160px;
        border-color: ${(props): string => props.theme.borderColorLight};
        background: ${(props): string => props.theme.bgColor};
        font-size: 12px;
        line-height: 20px;
        transition: transform 160ms ease, background-color 160ms ease, opacity 160ms ease;
    }

    .ant-tabs-card > .ant-tabs-nav .ant-tabs-tab-btn {
        display: flex;
        min-width: 0;
        max-width: 100%;
        overflow: hidden;
    }

    .ant-tabs-card > .ant-tabs-nav .ant-tabs-tab-active {
        background: ${(props): string => props.theme.bgColorLight};
    }

    .ant-tabs-tab-remove {
        margin-left: 4px;
        font-size: 10px;
    }
`;

export const DetailContextMenu = styled.div`
    position: fixed;
    z-index: 1000;
    min-width: 140px;
    padding: 4px 0;
    border: 1px solid ${(props): string => props.theme.textColorDisabled};
    border-radius: ${(props): string => props.theme.borderRadiusBase};
    background: ${(props): string => props.theme.contextMenuBgColor};
    box-shadow: ${(props): string => props.theme.boxShadowLight};
`;

export const DetailContextMenuItem = styled.div`
    padding: 5px 12px;
    color: ${(props): string => props.theme.textColorPrimary};
    cursor: pointer;
    white-space: nowrap;

    &:hover {
        color: #ffffff;
        background: ${(props): string => props.theme.primaryColorHover};
    }
`;

export const DetailTabLabel = styled.span`
    position: relative;
    display: inline-flex;
    flex: 1 1 auto;
    min-width: 0;
    align-items: center;
    max-width: 120px;
    overflow: hidden;
    cursor: default;
    text-overflow: ellipsis;
    transition: transform 160ms ease, color 160ms ease, opacity 160ms ease;
    user-select: none;
    white-space: nowrap;

    &[data-dragging='true'] {
        cursor: grabbing;
        opacity: 0.45;
    }

    &[data-dragging='true']::before {
        flex: 0 0 auto;
        width: 8px;
        height: 12px;
        margin-right: 4px;
        background-image: radial-gradient(currentColor 1px, transparent 1px);
        background-position: 0 0, 4px 0;
        background-size: 4px 4px;
        content: '';
        opacity: 0.65;
    }

    &[data-drag-over='true'] {
        color: ${(props): string => props.theme.primaryColor};
        transform: translateX(3px);
    }
`;

export const DetailPanel = styled.div`
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    min-height: 0;
    height: 100%;
    gap: 0;
    padding: 2px 0 8px;
    color: ${(props): string => props.theme.tableTextColor};
`;

export const DetailCard = styled.div`
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    padding: 10px 12px;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColorLight};
`;

export const DetailFieldList = styled.div`
    display: flex;
    flex-direction: column;
    gap: 4px;
    --detail-label-width: minmax(168px, 48%);
`;

export const DetailField = styled.div`
    display: grid;
    grid-template-columns: var(--detail-label-width) minmax(0, 1fr);
    column-gap: 12px;
    min-width: 0;
    align-items: start;
    font-size: 13px;
    line-height: 20px;

    @media (max-width: 720px) {
        --detail-label-width: 1fr;
        grid-template-columns: 1fr;
        row-gap: 2px;
    }
`;

export const DetailFieldName = styled.div`
    min-width: 0;
    overflow: hidden;
    color: ${(props): string => props.theme.textColorSecondary};
    text-overflow: ellipsis;
    white-space: nowrap;
`;

export const DetailFieldValue = styled.div`
    min-width: 0;
    overflow-wrap: anywhere;
    white-space: pre-wrap;
`;
