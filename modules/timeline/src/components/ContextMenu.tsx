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

import React, { useRef, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import type { Theme } from '@emotion/react';
import styled from '@emotion/styled';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import type { Session } from '../entity/session';
import { EyeCloseOtuLine } from '@insight/lib/icon';
import type { ChartInteractorHandles, InteractorMouseState } from './charts/ChartInteractor/ChartInteractor';
import { unit } from '../entity/insight';

import {
    actionClearBenchmarkSlice,
    actionCollapseAllUnits,
    actionEnableAutoUnitHeight,
    actionExpandAllUnits,
    actionFindInCommunication,
    actionGenerateCurve,
    actionGenerateBubbleCurve,
    actionFitToScreen,
    actionHideFlagEvents,
    actionHideUnits,
    actionTimeRangeAnalysis,
    actionRemoveTimeRangeAnalysis,
    actionTimeRangeAnalysisAndZoomIn,
    actionApplyTimeRangeAnalysis,
    actionLockSelection,
    actionRecoverDefaultOffset,
    actionResetZoom,
    actionSetBenchmarkSlice,
    actionAlignByOperator,
    actionShowFlagEvents,
    actionShowHiddenUnits,
    actionShowInEventsView,
    actionUndoZoom,
    actionUnLockSelection,
    actionUnpinAll,
    actionZoomIntoSelection,
    actionSetCardAlias,
    actionPinByUnitName,
    actionUnpinByUnitName,
    actionParseCardsOfRelatedGroup,
    actionMergeUnits,
    actionUnmergeUnits,
    actionSliceSelection,
    actionJumpToModelStream,
    actionJumpToLinkSlice,
} from '../actions';
import { Action } from '../actions/types';
import { getShortcutFromShortcutName, ShortcutName } from '../actions/shortcuts';
import { EmptyMetaData } from '../entity/data';
import { activateMenuAtDepth, truncateMenuPath } from './contextMenuPath';

interface Position {
    left: string;
    top: string;
}

interface Props {
    session: Session;
    interactorMouseState: InteractorMouseState;
    theme?: Theme;
    chartInteractorRef: React.RefObject<ChartInteractorHandles>;
    subMenus?: ContextMenuItem[];
    style?: { [key: string]: any };
}

type MenuItemsProps = Pick<Props, 'session'>;

export type ContextMenuItem = typeof CONTEXT_MENU_SEPARATOR | Action;

const MENU_VIEWPORT_MARGIN = 4;
const SUB_MENU_MAX_WIDTH = 200;
const SUB_MENU_MAX_HEIGHT = 300;
const SUB_MENU_OVERLAP = 4;

const MenuContainer = styled.div`
    font-size: 12px;
    padding: 3px 0;
    min-width: 200px;
    max-height: calc(100vh - 8px);
    overflow-x: hidden;
    overflow-y: auto;
    border-radius: ${(props): string => props.theme.borderRadiusBase};
    background-color:  ${(props): string => props.theme.contextMenuBgColor};
    position: fixed;
    z-index: 99999;
    transition: all .1s ease;
    box-shadow: ${(props): string => props.theme.boxShadowLight};
    user-select: none;
`;

const MenuItem = styled.div`
    display: grid;
    grid-template-columns: 1fr 0.2fr;
    align-items: center;
    padding: 4px 16px 4px 20px;
    color: ${(props): string => props.theme.textColorPrimary};
    position: relative;

    &:not(.disabled):hover{
      background: ${(props): string => props.theme.primaryColorHover};
      color: #ffffff;
    }
    &.disabled{
        color: ${(props): string => props.theme.textColorDisabled};
    }

    &.checkmark::before {
        position: absolute;
        left: 6px;
        margin-bottom: 1px;
        content: "√";
    }

    .menu-item__label {
        margin-right: 20px;
        white-space: nowrap; /* 防止文本换行 */
        overflow: hidden;    /* 隐藏溢出的内容 */
        text-overflow: ellipsis; /* 添加省略号 */
        max-width: 300px;        /* 设置一个固定宽度或根据需要调整 */
    }

    .menu-item__shortcut {
        opacity: 0.6;
    }

    .menu-item__shortcut-area {
        display: flex;
        align-items: center;
        justify-content: flex-end;
        gap: 4px;
    }

    .menu-item__arrow {
        display: inline-block;
        width: 5px;
        height: 5px;
        border-top: 1.5px solid currentColor;
        border-right: 1.5px solid currentColor;
        transform: rotate(45deg);
        opacity: 0.6;
    }
`;
const SubMenuContainer = styled.div`
    font-size: 12px;
    padding: 3px 0;
    min-width: 200px;
    border-radius: ${(props): string => props.theme.borderRadiusBase};
    background-color:  ${(props): string => props.theme.contextMenuBgColor};
    position: fixed;
    z-index: 99999;
    transition: all .1s ease;
    box-shadow: ${(props): string => props.theme.boxShadowLight};
    user-select: none;
    max-height: 300px;
    overflow-y: auto;
    overflow-x: hidden;
    .menu-item__label {
        grid-column: 1 / -1;
        margin-right: 0 !important;
    }
`;

const Separator = styled.hr`
    border: none;
    border-top: 1px solid ${(props): string => props.theme.borderColorLight};
`;

function closeMenu(session: Session): void {
    runInAction(() => {
        session.contextMenu.isVisible = false;
        session.contextMenu.activeMenuPath = [];
    });
}

function openMenu(session: Session): void {
    if (session.selectedUnits.length === 0) {
        return;
    }
    runInAction(() => {
        session.contextMenu.isVisible = true;
    });
}

export const EmptyUnit = unit<EmptyMetaData>({
    name: 'Empty',
    pinType: 'copied',
    renderInfo: (session: Session, metadata: { count: number}) =>
        <div>
            <EyeCloseOtuLine style={{ width: '15px', height: '15px', top: '3px', position: 'relative' }}/>
            <span style={{
                marginLeft: 3,
                overflow: 'hidden',
                fontSize: 14,
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
            }}>
                {metadata.count}{' unit'}{metadata.count > 1 ? 's' : ''}{' hidden'}
            </span>
        </div>,
});

function getViewportSize(): { width: number; height: number } {
    return {
        width: Math.max(document.documentElement.clientWidth, document.body.clientWidth),
        height: Math.max(document.documentElement.clientHeight, document.body.clientHeight),
    };
}

function adjustMenuPosition({ menu, setPosition, xPos, yPos }: {
    menu: HTMLDivElement;
    setPosition: (_: Position) => void;
    xPos: React.MutableRefObject<number>;
    yPos: React.MutableRefObject<number>;
}): void {
    const { width, height } = getViewportSize();
    if (xPos.current >= width - menu.offsetWidth) {
        xPos.current -= menu.offsetWidth;
    }
    if (yPos.current > height - menu.offsetHeight) {
        yPos.current = Math.max(MENU_VIEWPORT_MARGIN, height - menu.offsetHeight - MENU_VIEWPORT_MARGIN);
    }
    setPosition({ left: `${xPos.current + 1}px`, top: `${yPos.current}px` });
    menu.focus();
}

function getSubMenuStyle(element: HTMLElement): { top?: string; bottom?: string; left?: string; right?: string } {
    const { width, height } = getViewportSize();
    const { top, bottom, left, right } = element.getBoundingClientRect();
    const style: { top?: string; bottom?: string; left?: string; right?: string } = {};

    if (height - top <= SUB_MENU_MAX_HEIGHT) {
        style.bottom = `${Math.max(MENU_VIEWPORT_MARGIN, height - bottom)}px`;
    } else {
        style.top = `${top}px`;
    }

    if (width - right <= SUB_MENU_MAX_WIDTH) {
        style.right = `${Math.max(MENU_VIEWPORT_MARGIN, width - left - SUB_MENU_OVERLAP)}px`;
    } else {
        style.left = `${right - SUB_MENU_OVERLAP}px`;
    }

    return style;
}

export const CONTEXT_MENU_SEPARATOR = 'separator';
const contextMenuItems: ContextMenuItem[] = [
    // 特定操作
    actionFindInCommunication,
    actionGenerateCurve,
    actionGenerateBubbleCurve,
    actionSetCardAlias,
    actionParseCardsOfRelatedGroup,
    actionJumpToModelStream,
    CONTEXT_MENU_SEPARATOR,
    // 时间范围分析
    actionTimeRangeAnalysis,
    actionTimeRangeAnalysisAndZoomIn,
    actionRemoveTimeRangeAnalysis,
    actionApplyTimeRangeAnalysis,
    CONTEXT_MENU_SEPARATOR,
    // 泳道缩放
    actionFitToScreen,
    actionZoomIntoSelection,
    actionLockSelection,
    actionUnLockSelection,
    actionUndoZoom,
    actionResetZoom,
    CONTEXT_MENU_SEPARATOR,
    // 泳道置顶
    actionUnpinAll,
    actionPinByUnitName,
    actionUnpinByUnitName,
    CONTEXT_MENU_SEPARATOR,
    // 泳道偏移（对齐）
    actionSetBenchmarkSlice,
    actionClearBenchmarkSlice,
    actionAlignByOperator,
    actionRecoverDefaultOffset,
    CONTEXT_MENU_SEPARATOR,
    // 泳道收缩
    actionCollapseAllUnits,
    actionExpandAllUnits,
    CONTEXT_MENU_SEPARATOR,
    // 泳道隐藏
    actionMergeUnits,
    actionUnmergeUnits,
    actionHideUnits,
    actionShowHiddenUnits,
    CONTEXT_MENU_SEPARATOR,
    // 隐藏相关事件
    actionHideFlagEvents,
    actionShowFlagEvents,
    CONTEXT_MENU_SEPARATOR,
    // 高度自适应
    actionEnableAutoUnitHeight,
    CONTEXT_MENU_SEPARATOR,
    // 在 Events View 中显示
    actionShowInEventsView,
    actionSliceSelection,
    actionJumpToLinkSlice,
];

const SubMenu = (props: { session: Session; subMenus: ContextMenuItem[]; style: {[key: string]: any}; depth: number }): JSX.Element => {
    const { subMenus, style, depth } = props;
    const { t } = useTranslation();
    return (
        <SubMenuContainer className="sub-menu-container" style={style}>
            {getMenuItems(props, t, subMenus ?? [], depth)}
        </SubMenuContainer>
    );
};

function mouseEnterEvent(event: React.MouseEvent<HTMLDivElement, MouseEvent>, session: Session, menu: Action,
    disabled: boolean, depth: number): void {
    runInAction(() => {
        menu.style = event.currentTarget.classList.contains('has-sub-menu') ? getSubMenuStyle(event.currentTarget) : {};
        session.contextMenu.activeMenuPath = disabled
            ? truncateMenuPath(session.contextMenu.activeMenuPath, depth)
            : activateMenuAtDepth(session.contextMenu.activeMenuPath, depth, menu.name);
    });
}

function mouseLeaveEvent(event: React.MouseEvent<HTMLDivElement, MouseEvent>, session: Session, menu: Action,
    depth: number): void {
    if (menu.subMode ?? (event.relatedTarget instanceof HTMLElement && event.relatedTarget.closest('.sub-menu-container') !== null)) {
        return;
    }
    runInAction(() => {
        session.contextMenu.activeMenuPath = truncateMenuPath(session.contextMenu.activeMenuPath, depth);
    });
}

const getMenuItems = (props: MenuItemsProps, t: TFunction, menuItems: ContextMenuItem[], depth = 0): JSX.Element => {
    const { session } = props;
    if (!Array.isArray(session.selectedUnits) || session.selectedUnits.length === 0 || menuItems.length === 0) {
        return <></>;
    }
    const filteredItems = menuItems.filter(menu => menu === CONTEXT_MENU_SEPARATOR || (menu.visible?.(session) ?? true));
    if (!filteredItems.find(menuItem => menuItem !== CONTEXT_MENU_SEPARATOR)) { return <></>; }
    if (filteredItems[filteredItems.length - 1] === CONTEXT_MENU_SEPARATOR) {
        filteredItems.pop();
    }
    return <>
        {
            filteredItems.map((item, index) => {
                const prevIsLine = !filteredItems[index - 1] || filteredItems[index - 1] === CONTEXT_MENU_SEPARATOR;
                if (item === CONTEXT_MENU_SEPARATOR && prevIsLine) { return null; }
                if (item === CONTEXT_MENU_SEPARATOR) { return <Separator key={index} />; }
                const disabled = item.disabled?.(session) ?? false;
                const label = typeof item.label === 'function' ? item.label(session, t) : t(item.label);
                const subMenus = item.subMenus?.(session) ?? [];
                const subMenuIsVisible = item.subMode && session.contextMenu.activeMenuPath[depth] === item.name && session.contextMenu.isVisible;
                return <MenuItem
                    className={`menu-item ${disabled ? 'disabled' : ''} ${item.checked?.(session) ? 'checkmark' : ''} ${item.subMode ? 'has-sub-menu' : ''}`}
                    key={item.name}
                    title={label}
                    onClick={(e): void => {
                        if (disabled || item.subMode) { return; }
                        item.perform(session);
                        closeMenu(session);
                    }}
                    onMouseEnter={(event): void => { mouseEnterEvent(event, session, item, disabled, depth); }}
                    onMouseLeave={(event): void => { mouseLeaveEvent(event, session, item, depth); }}
                >
                    <div className="menu-item__label">{label}</div>
                    <div className="menu-item__shortcut-area">
                        <kbd className="menu-item__shortcut">{item.name ? getShortcutFromShortcutName(item.name as ShortcutName) : ''}</kbd>
                        {item.subMode && <span className="menu-item__arrow" />}
                    </div>
                    {subMenuIsVisible
                        ? <SubMenu style={item.style ?? {}} session={session} subMenus={subMenus} depth={depth + 1}></SubMenu>
                        : <></>}
                </MenuItem>;
            })
        }
    </>;
};

const Menu = (props: Props): JSX.Element => {
    const { session } = props;
    const menuRef = useRef<HTMLDivElement>(null);
    const [position, setPosition] = useState<Position>({ left: '0px', top: '0px' });
    const xPos = useRef(0); const yPos = useRef(0);
    const { t } = useTranslation();

    useEffect(() => {
        document.addEventListener('contextmenu', handleContextMenu);
        window.addEventListener('wheel', handleCloseMenu);

        return () => {
            document.removeEventListener('contextmenu', handleContextMenu);
            window.removeEventListener('wheel', handleCloseMenu);
        };
    });

    useEffect(() => {
        const menu = menuRef.current;
        if (session.contextMenu.isVisible && menu !== null) {
            adjustMenuPosition({ menu, setPosition, xPos, yPos });
        }
    }, [session.contextMenu.isVisible]);

    const handleContextMenu = (event: MouseEvent): void => {
        const targetElement = event.target as HTMLElement;
        if (targetElement?.closest('.laneWrapper') !== null) {
            xPos.current = event.clientX; yPos.current = event.clientY;
            setPosition({ left: `${xPos.current + 1}px`, top: `${yPos.current}px` });
            openMenu(session);
        }
    };

    const handleCloseMenu = (event: WheelEvent): void => {
        if (event.target instanceof Node && menuRef.current?.contains(event.target)) {
            return;
        }
        closeMenu(session);
    };

    const handleMenuScroll = (): void => {
        runInAction(() => {
            session.contextMenu.activeMenuPath = [];
        });
    };

    return (
        session.contextMenu.isVisible
            ? <MenuContainer ref={menuRef} style={{ ...position }} tabIndex={-1} onScroll={handleMenuScroll} onBlur={(): void => {
                closeMenu(session);
            }} >
                {getMenuItems(props, t, contextMenuItems)}
            </MenuContainer>
            : <></>
    );
};

export const ContextMenu = observer(Menu);
