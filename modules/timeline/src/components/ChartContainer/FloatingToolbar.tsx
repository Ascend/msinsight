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
import React from 'react';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react-lite';
import { runInAction } from 'mobx';
import { Tooltip } from '@insight/lib/components';
import { getShortcutKey } from '@insight/lib/utils';
import type { Session } from '../../entity/session';
import eventBus from '../../utils/eventBus';
import { ReactComponent as ArrowIcon } from '../../assets/images/floating-tools/arrow.svg';
import { ReactComponent as ChevronDoubleRightIcon } from '../../assets/images/floating-tools/chevron-double-right.svg';
import { ReactComponent as HeightIcon } from '../../assets/images/floating-tools/height.svg';
import { ReactComponent as HelpIcon } from '../../assets/images/floating-tools/help.svg';
import { ReactComponent as MoveIcon } from '../../assets/images/floating-tools/move.svg';
import { ReactComponent as SliceIcon } from '../../assets/images/floating-tools/slice.svg';
import sampleDefault from '../../assets/images/floating-tools/sample-1.png';
import sampleHeight from '../../assets/images/floating-tools/sample-2.png';
import sampleSlice from '../../assets/images/floating-tools/sample-3.png';
import sampleDrag from '../../assets/images/floating-tools/sample-4.png';

const TOOLBAR_TOP_OFFSET_PX = 78;
const TOOLBAR_RIGHT_OFFSET_PX = 8;
const TOOLBAR_BUTTON_SIZE_PX = 24;

type FloatingToolbarProps = {
    session: Session;
};

type ToolbarIcon = React.FunctionComponent<React.SVGProps<SVGSVGElement> & { title?: string }>;

type ToolbarItem = {
    id: 'default-mode' | 'drag-mode' | 'auto-fit-height' | 'selection-mode';
    icon: ToolbarIcon;
    tooltipKey: string;
    descriptionKey: string;
    testId: string;
    image: string;
};

type ToolbarMode = Exclude<ToolbarItem['id'], 'auto-fit-height'>;

const TOOLBAR_ITEMS: ToolbarItem[] = [
    {
        id: 'default-mode',
        icon: ArrowIcon,
        tooltipKey: 'floatingToolbar.defaultMode',
        descriptionKey: 'floatingToolbar.defaultModeDescription',
        testId: 'timeline-floating-toolbar-default-mode',
        image: sampleDefault,
    },
    {
        id: 'drag-mode',
        icon: MoveIcon,
        tooltipKey: 'floatingToolbar.dragMode',
        descriptionKey: 'floatingToolbar.dragModeDescription',
        testId: 'timeline-floating-toolbar-drag-mode',
        image: sampleDrag,
    },
    {
        id: 'auto-fit-height',
        icon: HeightIcon,
        tooltipKey: 'floatingToolbar.autoFitHeight',
        descriptionKey: 'floatingToolbar.autoFitHeightDescription',
        testId: 'timeline-floating-toolbar-auto-fit-height',
        image: sampleHeight,
    },
    {
        id: 'selection-mode',
        icon: SliceIcon,
        tooltipKey: 'floatingToolbar.selectionMode',
        descriptionKey: 'floatingToolbar.selectionModeDescription',
        testId: 'timeline-floating-toolbar-selection-mode',
        image: sampleSlice,
    },
];

const COLLAPSED_TOP_OFFSET_PX = 206;

const ToolbarContainer = styled.div<{ collapsed: boolean }>`
    position: absolute;
    top: ${TOOLBAR_TOP_OFFSET_PX}px;
    right: 0;
    z-index: 11;
    display: flex;
    align-items: flex-start;
    transform: ${(props): string => props.collapsed
        ? `translate(0, ${COLLAPSED_TOP_OFFSET_PX - TOOLBAR_TOP_OFFSET_PX}px)`
        : `translate(-${TOOLBAR_RIGHT_OFFSET_PX}px, 0)`};
    transition: transform 200ms ease-out;
`;

const ToolbarPanel = styled.div<{ collapsed: boolean }>`
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: ${(props): string => props.collapsed ? '0' : '4px'};
    width: ${(props): string => props.collapsed ? '16px' : '32px'};
    padding: ${(props): string => props.collapsed ? '0' : '4px'};
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-right: ${(props): string => props.collapsed ? 'none' : `1px solid ${props.theme.borderColor}`};
    border-radius: ${(props): string => props.collapsed ? '12px 0 0 12px' : '6px'};
    background-color: ${(props): string => props.theme.contentBackgroundColor};
    box-shadow: ${(props): string => props.theme.boxShadow};
    transition: width 200ms ease-out, padding 200ms ease-out, border-radius 200ms ease-out;
`;

const ToolbarItems = styled.div<{ collapsed: boolean }>`
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    max-height: ${(props): string => props.collapsed ? '0' : '136px'};
    opacity: ${(props): number => props.collapsed ? 0 : 1};
    overflow: hidden;
    transform: ${(props): string => props.collapsed ? 'translateX(8px)' : 'translateX(0)'};
    transform-origin: right top;
    pointer-events: ${(props): string => props.collapsed ? 'none' : 'auto'};
    transition: opacity 160ms ease-out, max-height 200ms ease-out, transform 200ms ease-out;
`;

const ToolbarButton = styled.button<{ selected?: boolean; disabled?: boolean }>`
    display: flex;
    align-items: center;
    justify-content: center;
    width: ${TOOLBAR_BUTTON_SIZE_PX}px;
    height: ${TOOLBAR_BUTTON_SIZE_PX}px;
    padding: 0;
    border: none;
    border-radius: 4px;
    color: ${(props): string => props.disabled ? props.theme.textColorDisabled : props.theme.iconColor};
    background-color: ${(props): string => props.selected ? props.theme.selectedChartBackgroundColor : 'transparent'};
    cursor: ${(props): string => props.disabled ? 'not-allowed' : 'pointer'};
    opacity: ${(props): number => props.disabled ? 0.5 : 1};

    &:hover {
        background-color: ${(props): string => props.disabled ? props.selected ? props.theme.selectedChartBackgroundColor : 'transparent' : props.selected ? props.theme.selectedChartBackgroundColor : props.theme.bgColorLight};
    }

    svg {
        width: 16px;
        height: 16px;
    }
`;

const HelpButtonWrap = styled.div`
    margin-top: 2px;
    padding-top: 4px;
    border-top: 1px solid ${(props): string => props.theme.borderColorLighter};
`;

const ToggleButton = styled(ToolbarButton)<{ collapsed: boolean }>`
    width: ${(props): string => props.collapsed ? '16px' : `${TOOLBAR_BUTTON_SIZE_PX}px`};
    height: ${(props): string => props.collapsed ? '48px' : `${TOOLBAR_BUTTON_SIZE_PX}px`};
    border-radius: ${(props): string => props.collapsed ? '12px 0 0 12px' : '4px'};
    transition: width 200ms ease-out, height 200ms ease-out, border-radius 200ms ease-out, background-color 160ms ease-out;

    &:hover {
        background-color: ${(props): string => props.theme.bgColorLight};
    }

    svg {
        width: 16px;
        height: 16px;
        transform: ${(props): string => props.collapsed ? 'rotate(180deg)' : 'none'};
        transition: transform 200ms ease-out;
    }
`;

const GuideOverlay = styled.div`
    position: fixed;
    inset: 0;
    z-index: 10;
    background-color: ${(props): string => props.theme.bgColor};
    opacity: 0.22;
    pointer-events: none;
`;

const GuidePanel = styled.div`
    position: absolute;
    top: 0;
    right: 42px;
    z-index: 12;
    max-height: calc(100vh - 120px);
    padding: 16px 16px 12px;
    overflow-y: auto;
    border: 1px solid ${(props): string => props.theme.borderColorLighter};
    border-radius: 4px;
    background-color: ${(props): string => props.theme.bgColorLight};
    box-shadow: ${(props): string => props.theme.boxShadow};
`;

const GuideHeader = styled.div`
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 12px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 14px;
    font-weight: 600;
`;

const CloseButton = styled.button`
    width: 20px;
    height: 20px;
    padding: 0;
    border: none;
    color: ${(props): string => props.theme.iconColor};
    background: transparent;
    cursor: pointer;
    font-size: 20px;
    line-height: 18px;

    &:hover {
        color: ${(props): string => props.theme.primaryColor};
    }
`;

const GuideItem = styled.div`
    margin-bottom: 14px;
`;

const GuideTitle = styled.div`
    display: flex;
    align-items: center;
    gap: 6px;
    margin-bottom: 4px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 13px;
    font-weight: 600;

    svg {
        width: 16px;
        height: 16px;
        color: ${(props): string => props.theme.iconColor};
    }
`;

const GuideDescription = styled.div`
    margin-left: 22px;
    margin-bottom: 8px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    line-height: 18px;
`;

const GuideImage = styled.img`
    display: block;
    width: 318px;
    height: 96px;
    margin-left: 22px;
    object-fit: cover;
    border-radius: 4px;
    background-color: ${(props): string => props.theme.bgColorLight};
`;

const stopToolbarEvent = (event: React.MouseEvent<HTMLElement>): void => {
    event.preventDefault();
    event.stopPropagation();
};

const toggleAutoFitHeight = (session: Session): void => {
    runInAction(() => {
        session.autoAdjustUnitHeight = !session.autoAdjustUnitHeight;
        session.renderTrigger = !session.renderTrigger;
    });
};

const toggleSelectionMode = (session: Session, active: boolean): void => {
    runInAction(() => {
        session.sliceSelection.active = active;
        session.sliceSelection.activeIsChanged = true;
        eventBus.emit('sliceActiveChanged', session.sliceSelection.active);
    });
};

const getActiveMode = (session: Session): ToolbarMode => {
    if (session.panMode) {
        return 'drag-mode';
    }
    if (session.sliceSelection.active) {
        return 'selection-mode';
    }
    return 'default-mode';
};

const getVisualActiveMode = (session: Session): ToolbarMode => {
    if (session.panModePressed) {
        return 'drag-mode';
    }
    return getActiveMode(session);
};

const FloatingToolbar = observer(({ session }: FloatingToolbarProps): JSX.Element => {
    const { t } = useTranslation('timeline');
    const [collapsed, setCollapsed] = React.useState(false);
    const [guideVisible, setGuideVisible] = React.useState(false);
    const activeMode = getActiveMode(session);
    const visualActiveMode = getVisualActiveMode(session);

    const handleItemClick = (event: React.MouseEvent<HTMLElement>, item: ToolbarItem): void => {
        stopToolbarEvent(event);
        if (isDisabled(item)) {
            return;
        }
        switch (item.id) {
            case 'auto-fit-height':
                toggleAutoFitHeight(session);
                break;
            case 'default-mode':
                runInAction(() => {
                    session.panMode = false;
                });
                toggleSelectionMode(session, false);
                break;
            case 'drag-mode': {
                runInAction(() => {
                    session.panMode = activeMode !== 'drag-mode';
                });
                toggleSelectionMode(session, false);
                break;
            }
            case 'selection-mode': {
                const active = activeMode !== 'selection-mode';
                runInAction(() => {
                    session.panMode = false;
                });
                toggleSelectionMode(session, active);
                break;
            }
            default:
                break;
        }
    };

    const isSelected = (item: ToolbarItem): boolean => {
        if (item.id === 'auto-fit-height') {
            return session.autoAdjustUnitHeight;
        }
        return visualActiveMode === item.id;
    };

    const isDisabled = (item: ToolbarItem): boolean => {
        if (item.id === 'selection-mode') {
            return session.isTimeAnalysisMode;
        }
        return false;
    };

    const getToolbarTitle = (item: ToolbarItem): string => {
        if (item.id === 'drag-mode') {
            return `${t(item.tooltipKey, { key: getShortcutKey('CtrlOrCmd') })}`;
        }
        return t(item.tooltipKey);
    };

    return <>
        {guideVisible && <GuideOverlay />}
        <ToolbarContainer collapsed={collapsed} onMouseDown={stopToolbarEvent} onClick={stopToolbarEvent}>
            {guideVisible && <GuidePanel>
                <GuideHeader>
                    <span>{t('floatingToolbar.guideTitle')}</span>
                    <CloseButton
                        type="button"
                        aria-label={t('floatingToolbar.closeGuide')}
                        onClick={(event): void => {
                            stopToolbarEvent(event);
                            setGuideVisible(false);
                        }}
                    >×</CloseButton>
                </GuideHeader>
                {TOOLBAR_ITEMS.map((item) => {
                    const Icon = item.icon;
                    const title = getToolbarTitle(item);
                    return <GuideItem key={item.id}>
                        <GuideTitle>
                            <Icon />
                            <span>{title}</span>
                        </GuideTitle>
                        <GuideDescription>{t(item.descriptionKey)}</GuideDescription>
                        <GuideImage src={item.image} alt={title} />
                    </GuideItem>;
                })}
            </GuidePanel>}
            <ToolbarPanel collapsed={collapsed}>
                <ToolbarItems collapsed={collapsed}>
                    {TOOLBAR_ITEMS.map((item) => {
                        const Icon = item.icon;
                        return <Tooltip key={item.id} title={getToolbarTitle(item)} placement="left">
                            <ToolbarButton
                                type="button"
                                selected={isSelected(item)}
                                disabled={isDisabled(item)}
                                data-testid={item.testId}
                                onClick={(event): void => handleItemClick(event, item)}
                            >
                                <Icon />
                            </ToolbarButton>
                        </Tooltip>;
                    })}
                    <HelpButtonWrap>
                        <Tooltip title={t('floatingToolbar.guideTitle')} placement="left">
                            <ToolbarButton
                                type="button"
                                selected={guideVisible}
                                data-testid="timeline-floating-toolbar-guide"
                                onClick={(): void => {
                                    setGuideVisible((prev) => !prev);
                                }}
                            >
                                <HelpIcon />
                            </ToolbarButton>
                        </Tooltip>
                    </HelpButtonWrap>
                </ToolbarItems>
                <Tooltip title={collapsed ? t('floatingToolbar.expand') : t('floatingToolbar.collapse')} placement="left">
                    <ToggleButton
                        type="button"
                        collapsed={collapsed}
                        data-testid="timeline-floating-toolbar-toggle"
                        onClick={(): void => {
                            setCollapsed((prev) => !prev);
                            setGuideVisible(false);
                        }}
                    >
                        <ChevronDoubleRightIcon />
                    </ToggleButton>
                </Tooltip>
            </ToolbarPanel>
        </ToolbarContainer>
    </>;
});

export default FloatingToolbar;
