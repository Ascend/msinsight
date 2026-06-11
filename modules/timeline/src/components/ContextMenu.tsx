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
    actionJumpToLinkSlice,
} from '../actions';
import { Action } from '../actions/types';
import { getShortcutFromShortcutName, ShortcutName } from '../actions/shortcuts';
import { EmptyMetaData } from '../entity/data';

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

export type ContextMenuItem = typeof CONTEXT_MENU_SEPARATOR | Action;

const MenuContainer = styled.div`
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
    position: absolute;
    z-index: 99999;
    transition: all .1s ease;
    box-shadow: ${(props): string => props.theme.boxShadowLight};
    user-select: none;
    max-height: 300px;
    overflow-y: auto;
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
        session.contextMenu.activeMenuKey = '';
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

function adjustMenuPosition({ menu, setPosition, xPos, yPos }: {
    menu: HTMLDivElement;
    setPosition: (_: Position) => void;
    xPos: React.MutableRefObject<number>;
    yPos: React.MutableRefObject<number>;
}): void {
    const winWidth = document.documentElement.clientWidth || document.body.clientWidth;
    const winHeight = document.documentElement.clientHeight || document.body.clientHeight;
    if (xPos.current >= winWidth - menu.offsetWidth) {
        xPos.current -= menu.offsetWidth;
    }
    if (yPos.current > winHeight - menu.offsetHeight) {
        yPos.current -= menu.offsetHeight;
    }
    setPosition({ left: `${xPos.current + 1}px`, top: `${yPos.current}px` });
    menu.focus();
}

export const CONTEXT_MENU_SEPARATOR = 'separator';
const contextMenuItems: ContextMenuItem[] = [
    // 特定操作
    actionFindInCommunication,
    actionGenerateCurve,
    actionGenerateBubbleCurve,
    actionSetCardAlias,
    actionParseCardsOfRelatedGroup,
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

const SubMenu = (props: { session: Session; subMenus: ContextMenuItem[]; style: {[key: string]: any} }): JSX.Element => {
    const { subMenus, style } = props;
    const { t } = useTranslation();
    return (
        <SubMenuContainer className="sub-menu-container" style={style}>
            {getMenuItems(props as Props, t, subMenus ?? [])}
        </SubMenuContainer>
    );
};

function mouseEnterEvent(event: React.MouseEvent<HTMLDivElement, MouseEvent>, session: Session, menu: Action, disabled: boolean): void {
    runInAction(() => {
        const SUB_MENU_MAX_WIDTH = 200;
        const SUB_MENU_MAX_HEIGHT = 300;
        const element = event.target as HTMLElement;
        const style: { top?: string; bottom?: string; left?: string; right?: string } = {};
        const clientWidth = window.innerWidth || document.documentElement.clientWidth;
        const clientHeight = window.innerHeight || document.documentElement.clientHeight;
        if (element.closest('.has-sub-menu')) {
            const { bottom, right } = element.getBoundingClientRect();
            if (clientHeight - bottom <= SUB_MENU_MAX_HEIGHT) {
                style.bottom = '0';
            } else {
                style.top = '0';
            }
            if (clientWidth - right <= SUB_MENU_MAX_WIDTH) {
                style.right = '100%';
            } else {
                style.left = '100%';
            }
        }
        menu.style = style;
        session.contextMenu.activeMenuKey = disabled && !menu.parentMenuKey ? '' : menu.parentMenuKey ?? menu.name;
    });
}

function mouseLeaveEvent(session: Session, menu: Action): void {
    runInAction(() => {
        session.contextMenu.activeMenuKey = menu.parentMenuKey ? session.contextMenu.activeMenuKey : '';
    });
}

const getMenuItems = (props: Props, t: TFunction, menuItems: ContextMenuItem[]): JSX.Element => {
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
                const subMenuIsVisible = item.subMode && session.contextMenu.activeMenuKey === item.name && session.contextMenu.isVisible;
                return <MenuItem
                    className={`menu-item ${disabled ? 'disabled' : ''} ${item.checked?.(session) ? 'checkmark' : ''} ${item.subMode ? 'has-sub-menu' : ''}`}
                    key={item.name}
                    title={label}
                    onClick={(e): void => {
                        if (disabled || item.subMode) { return; }
                        item.perform(session);
                        runInAction(() => {
                            session.contextMenu.isVisible = false;
                        });
                    }}
                    onMouseEnter={(event): void => { mouseEnterEvent(event, session, item, disabled); }}
                    onMouseLeave={(): void => { mouseLeaveEvent(session, item); }}
                >
                    <div className="menu-item__label">{label}</div>
                    <div className="menu-item__shortcut-area">
                        <kbd className="menu-item__shortcut">{item.name ? getShortcutFromShortcutName(item.name as ShortcutName) : ''}</kbd>
                        {item.subMode && <span className="menu-item__arrow" />}
                    </div>
                    {subMenuIsVisible ? <SubMenu style={item.style ?? {}} session={session} subMenus={subMenus}></SubMenu> : <></>}
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

    const handleCloseMenu = (): void => {
        closeMenu(session);
    };

    return (
        session.contextMenu.isVisible
            ? <MenuContainer ref={menuRef} style={{ ...position }} tabIndex={-1} onBlur={(): void => {
                closeMenu(session);
            }} >
                {getMenuItems(props, t, contextMenuItems)}
            </MenuContainer>
            : <></>
    );
};

export const ContextMenu = observer(Menu);
