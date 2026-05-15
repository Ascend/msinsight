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
import styled from '@emotion/styled';
import type { File, Session } from '@/entity/session';
import type { TFunction } from 'i18next';
import { useTranslation } from 'react-i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { setBaselineData, setCompareData, cancelBaselineData, cancelCompareData } from '@/utils/Compare';
import type { DataSource, LayerType } from '@/centralServer/websocket/defs';

interface Position {
    x: number;
    y: number;
};

interface MenuItemModel {
    label: React.ReactNode;
    key: string;
    action: () => void;
    visible?: boolean;
    disabled?: boolean;
}

const MenuItem = styled.div`
    padding: 4px 16px;
    color: ${(props): string => props.theme.textColorPrimary};

    &:not(.disabled):hover{
      background: ${(props): string => props.theme.primaryColorHover};
      color: #ffffff;
    }
    &.disabled{
        color: ${(props): string => props.theme.textColorDisabled};
    }
`;

export const isSameFile = (one: File, anotherOne: File): boolean => {
    return one.projectName === anotherOne.projectName && one.filePath === anotherOne.filePath && one.rankId?.replace('Baseline_', '') === anotherOne.rankId?.replace('Baseline_', '');
};

// 判断是否是集群可对比数据
function checkProjectFileIsClusterComparable<T extends File>(projectFile: T, project?: DataSource): [boolean, string] {
    const clusterList = project?.children?.filter((item) => item.type === 'CLUSTER') ?? [];
    let clusterListNum = clusterList.length;
    let clusterPath = '';
    if (clusterListNum === 1) { // 单集群情况
        clusterPath = clusterList[0].path;
    }
    // 判断是否是 Project(Cluster) - Rank 的形式
    if (clusterListNum === 0 && (project?.children?.every((child) => child.type === 'RANK') ?? false)) {
        clusterListNum = 1;
    }
    if (clusterPath === '') { clusterPath = projectFile.filePath; }
    if (clusterListNum <= 0) {
        // 不是 Project - Cluster - Rank 或者 Project(Cluster) - Rank 形式
        return [false, clusterPath];
    } else if (clusterListNum === 1) {
        // 是 Project(Cluster) - Rank 形式
        return [projectFile.fileType === 'PROJECT', clusterPath];
    } else {
        // 是 Project - Cluster - Rank 形式
        return [projectFile.fileType === 'CLUSTER', clusterPath];
    }
}

// 判断是否是其他可对比数据
function checkProjectFileIsOtherComparable<T extends { fileType: LayerType; projectName: string }>(projectFile: T): boolean {
    return projectFile.fileType !== 'PROJECT' && projectFile.fileType !== 'CLUSTER';
}

function getClusterIsComparableAndSelectedClusterPath(baseline: File, comparison: File, selectedFile: File, session: Session): {
    isBaseline: boolean; isComparison: boolean; hasBaseline: boolean; isComparable: boolean; isCanBeBaseline: boolean; selectedClusterPath: string;
} {
    const baselineProject = session.getDataSourceByProjectName(baseline.projectName);
    const selectedProject = session.getDataSourceByProjectName(selectedFile.projectName);
    // 选中的是否已被设为基线
    const isBaseline = isSameFile(selectedFile, baseline);
    // 选中的是否已被设为对比
    const isComparison = isSameFile(selectedFile, comparison);
    const hasBaseline = baseline.projectName !== '';
    const [isClusterComparableBaseline] = checkProjectFileIsClusterComparable(baseline, baselineProject);
    const [isClusterComparableSelected, selectedClusterPath] = checkProjectFileIsClusterComparable(selectedFile, selectedProject);
    const isOtherComparableBaseline = checkProjectFileIsOtherComparable(baseline);
    const isOtherComparableSelected = checkProjectFileIsOtherComparable(selectedFile);
    // 可比较的定义：（要么基线是集群可比较类型自己也是集群可比较类型，要么基线是其他可比较类型自己也是其他可比较类型）
    const isComparable = (isClusterComparableBaseline && isClusterComparableSelected) || (isOtherComparableBaseline && isOtherComparableSelected);
    // 可设置为基线的定义：自己不是基线且（要么自己是集群可比较类型，要么自己是其他可比较类型）
    const isCanBeBaseline = !isBaseline && (isClusterComparableSelected || isOtherComparableSelected);
    return { isBaseline, isComparison, hasBaseline, isComparable, isCanBeBaseline, selectedClusterPath };
}

const openInExplorer = (selectedFile: File): void => {
    window.ipc?.postMessage(`openProjectInExplorer|${selectedFile.filePath}`);
};

// 右键菜单
const getMenuItems = ({ session }: IProps, t: TFunction): MenuItemModel[] => {
    const { activeDataSource, selectedFile, compareSet: { baseline, comparison } } = session;
    if (selectedFile.fileType === 'UNKNOWN') { return []; }
    const {
        isBaseline, isComparison, hasBaseline, isComparable, isCanBeBaseline, selectedClusterPath,
    } = getClusterIsComparableAndSelectedClusterPath(baseline, comparison, selectedFile, session);

    const currentClusterPath = session.getClusterPath(activeDataSource);

    const allMenuItems: MenuItemModel[] = [
        {
            label: t('Set as Baseline Data'),
            key: 'setAsBaselineData',
            action: (): void => { setBaselineData({ ...selectedFile, baselineClusterPath: selectedClusterPath, currentClusterPath }); },
            // 可设置为基线
            visible: isCanBeBaseline,
        },
        {
            label: t('Unset as Baseline Data'),
            key: 'unsetAsBaselineData',
            action: cancelBaselineData,
            // 此文件（项目）是基线文件
            visible: hasBaseline && isBaseline,
        },
        {
            label: t('Set as Comparison Data'),
            key: 'setAsComparisonData',
            action: (): void => { setCompareData(selectedFile); },
            // 有基线，自己类型不是 PROJECT 或 CLUSTER, 且自己不是基线也不是比对，且基线和自己是可比较的
            visible: selectedFile.fileType !== 'PROJECT' && selectedFile.fileType !== 'CLUSTER' && hasBaseline && !isBaseline && !isComparison && isComparable,
            // 选中的内容所属的 projectName 必须是激活的数据源
            disabled: activeDataSource.projectName !== selectedFile.projectName,
        },
        {
            label: t('Unset as Comparison Data'),
            key: 'unsetAsComparisonData',
            action: cancelCompareData,
            // 此文件（项目）是比对文件
            visible: isComparison,
        },
        {
            label: t('Open in Explorer'),
            key: 'openInExplorer',
            action: () => openInExplorer(selectedFile),
            visible: window.ipc !== undefined,
        },
    ];

    return allMenuItems.filter(menuItem => menuItem.visible !== false);
};

function openMenu(session: Session): void {
    runInAction(() => {
        session.contextMenu.visible = true;
    });
}
function closeMenu(session: Session): void {
    runInAction(() => {
        session.contextMenu.visible = false;
    });
}

// 调整菜单位置：不要超出窗口
function adjustMenuPosition({ menu, setMenuPosition, mousePosition }: {
    menu: HTMLDivElement;
    setMenuPosition: (_: Position) => void;
    mousePosition: Position;
}): void {
    const winWidth = document.documentElement.clientWidth || document.body.clientWidth;
    const winHeight = document.documentElement.clientHeight || document.body.clientHeight;
    let x = mousePosition.x;
    let y = mousePosition.y;
    if (x + menu.offsetWidth >= winWidth) {
        x -= menu.offsetWidth;
    }
    if (y + menu.offsetHeight > winHeight) {
        y -= menu.offsetHeight;
    }
    setMenuPosition({ x, y });
    menu.focus();
};

const MenuContainer = styled.div`
    padding: 3px 0;
    min-width: 100px;
    border-radius: ${(props): string => props.theme.borderRadiusBase};
    background-color:  ${(props): string => props.theme.contextMenuBgColor};
    position: fixed;
    z-index: 99999;
    box-shadow: ${(props): string => props.theme.boxShadowLight};
    user-select: none;
`;
interface IProps {
    session: Session;
}
const Menu = ({ session }: IProps): JSX.Element => {
    const menuRef = useRef<HTMLDivElement>(null);
    const [mousePosition, setMousePosition] = useState<Position>({ x: 0, y: 0 });
    const [menuPosition, setMenuPosition] = useState<Position>({ x: 0, y: 0 });
    const { t } = useTranslation('framework');

    const handleContextMenu = (event: MouseEvent): void => {
        event.preventDefault();
        event.stopPropagation();
        const targetElement = event.target as HTMLElement;
        if (targetElement?.closest('.can-right-click')) {
            setMousePosition({ x: event.clientX, y: event.clientY });
            setMenuPosition({ x: event.clientX, y: event.clientY });
            openMenu(session);
        }
    };

    const handleMouseDown = (e: MouseEvent): void => {
        if ((e.target as HTMLElement)?.parentNode !== menuRef.current) {
            closeMenu(session);
        }
    };

    const handleCloseMenu = (): void => {
        closeMenu(session);
    };

    useEffect(() => {
        document.addEventListener('contextmenu', handleContextMenu);
        window.addEventListener('mousedown', handleMouseDown);
        window.addEventListener('wheel', handleCloseMenu);

        return () => {
            document.removeEventListener('contextmenu', handleContextMenu);
            window.removeEventListener('mousedown', handleMouseDown);
            window.removeEventListener('wheel', handleCloseMenu);
        };
    });

    useEffect(() => {
        const menu = menuRef.current;
        if (session.contextMenu.visible && menu !== null) {
            adjustMenuPosition({ menu, setMenuPosition, mousePosition });
        }
    }, [session.contextMenu.visible, mousePosition]);

    const menuList = React.useMemo(() => {
        return getMenuItems({ session }, t);
    }, [session.dataSources, session.activeDataSource, session.selectedFile, session.compareSet.baseline, session.compareSet.comparison, t]);

    return (
        session.contextMenu.visible && menuList.length > 0
            ? <MenuContainer ref={menuRef} style={{ left: `${menuPosition.x}px`, top: `${menuPosition.y}px` }} tabIndex={-1} onBlur={(): void => { closeMenu(session); }} >
                {menuList.map((item: MenuItemModel): JSX.Element => (
                    <MenuItem className={`menu-item ${item.disabled ? 'disabled' : ''}`} key={item.key}
                        onClick={(): void => {
                            if (item.disabled) {
                                return;
                            }
                            item.action();
                            closeMenu(session);
                        }}>
                        {item.label}
                    </MenuItem>))}
            </MenuContainer>
            : <></>
    );
};

const ContextMenu = observer(Menu);
export default ContextMenu;
