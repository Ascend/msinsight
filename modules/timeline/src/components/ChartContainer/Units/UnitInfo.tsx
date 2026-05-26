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
import * as React from 'react';
import { flushSync } from 'react-dom';
import styled from '@emotion/styled';
import cls from 'classnames';
import { isEmpty } from 'lodash';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
// support utils/types
import { useTranslation } from 'react-i18next';
import { level } from '../../../entity/common';
import type { Session } from '../../../entity/session';
import type { KeyedInsightUnit } from './types';
import type { InsightUnit } from '../../../entity/insight';
import { getAutoKey } from '../../../utils/dataAutoKey';
// assets
import { ReactComponent as Arrow } from '../../../assets/images/insights/PullDownIcon.svg';
import { ReactComponent as StickyIcon } from '../../../assets/images/sticky_unit_button_icon.svg';
// components
import { message } from 'antd';
import { Checkbox, Tooltip } from '@insight/lib/components';
import { StartIcon, PinIcon, UnPinIcon } from '@insight/lib/icon';
import { StyledButton } from '../../base/StyledButton';
import { ReactComponent as Supported } from '../../../assets/images/insights/Supported.svg';
import { CardUnit, ROOT_UNIT, ThreadUnit } from '../../../insight/units/AscendUnit';
import { UnitProgress } from '../../charts/UnitProgress';
// trace/platform
import { platform } from '../../../platforms';
import { traceSingle } from '../../../utils/traceLogger';
// common constant variable
import { CheckboxChangeEvent } from 'antd/lib/checkbox';
import { isPinned, switchPinned } from '../unitPin';
import { useSelectUnit } from './hooks';
import { useDeselectUnits, useSelectUnits } from './hooks/useSelectUnit';
import { parseCards } from '../../../api/request';
import type { ParseCardsParam } from '../../../api/interface';
import { useComplexMouseEvent } from './mouseEvent';
import { CardMetaData, ThreadMetaData } from '../../../entity/data';
import { getTimeOffset, bigSubtract } from '../../../insight/units/utils';
import connector from '../../../connection/index';
import { useEffect } from 'react';
import { updateThreadsToFetch } from '../../../actions/actionExpandUnits';
import { getUnitUniqueId } from '../../../utils';

const DefaultInfoContainer = styled.div`
    display: flex;
    justify-content: space-between;
    font-size: 14px;
    height: 100%;
    flex: auto;

    .insight-lane-info-header {
        padding-left: 6px;
        min-width: 0;
        flex: 1;

        .insight-lane-info-name {
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }

        .insight-lane-info-outer-name {
            display: flex;
            align-items: center;
            &.noTag {
                height: 100%;
                font-size: 12px;
            }
        }

       .insight-lane-info-tag-info {
            font-size: 12px;
            line-height: 14px;

            .insight-lane-info-tag-text {
                padding: 1px 8px;
                background-color: ${(props): string => `${props.theme.bgColorCommon}bd`};
                border-radius: 4px;
            }
        }
    }

    .insight-lane-info-header.expandable {
        align-items: center;
    }

    .insight-lane-info-header.draggable {
        cursor: grab;
    }

    .insight-lane-info-header.undraggable {
        cursor: not-allowed;
    }
`;

const TagDiv = styled.div`
    overflow: hidden;
    white-space: nowrap;
    padding: 0 5px;

    .tag-content {
        margin-left: 6px;
        background-color: ${(props): string => `${props.theme.bgColorCommon}bd`};
        overflow: hidden;
        text-overflow: ellipsis;
        font-size: 13px;
        border-radius: 4px;
        padding: 0 2px;
    }
`;

const RootLabelDiv = styled.div`
    overflow: hidden;
    white-space: nowrap;

    .label-content {
        background-color: ${(props): string => `${props.theme.bgColorCommon}bd`};
        overflow: hidden;
        text-overflow: ellipsis;
        font-size: 11px;
        border-radius: 4px;
        display: inline-block;
    }
`;

interface DefaultInfoProps {
    session: Session;
    unit: KeyedInsightUnit;
    isHovered: boolean;
    hasPinButton: boolean;
    name: string;
    isSelected: boolean;
    enableDrag?: boolean;
    mouseDown: (e: React.MouseEvent) => void;
    onConfigBarClick?: () => void;
}

const DefaultInfo = observer(({ unit, name, session, ...props }: DefaultInfoProps): JSX.Element => {
    const tag = (typeof unit.tag === 'string') ? `${unit.tag}` : unit.tag?.(session, unit.metadata) ?? undefined;
    const tooltipInfo = getDefaultInfoTooltipTitle(unit, name);
    const allNumeric = tooltipInfo.cardNames?.every(str => str?.trim() !== '' && !isNaN(Number(str))) ?? false;
    return <DefaultInfoContainer>
        <div
            key={ `${getAutoKey(unit)} lane info` }
            className={cls('insight-lane-info-header', {
                expandable: unit.children && unit.children.length > 0,
                draggable: !!props.enableDrag && !unit.isExpanded,
                undraggable: !!props.enableDrag && unit.isExpanded,
            }) }
            onMouseDown={props.mouseDown}
        >
            <div className={cls('insight-lane-info-outer-name', { noTag: isEmpty(tag) })}>
                { [...unit.notifications ?? []]?.map((item, index) => {
                    const notifyRes = item(unit.metadata);
                    if (notifyRes !== false) {
                        return <Tooltip key={index} title={notifyRes}><Supported style={{ flex: 'none', marginRight: 8 }}/></Tooltip>;
                    }
                    return null;
                })}
                <Tooltip title={tooltipInfo.content}>
                    <div style={{ width: '100%' }}>
                        <div className="insight-lane-info-name">{name}</div>
                        <div>
                            {
                                (unit instanceof ROOT_UNIT && tooltipInfo.cardNames && tooltipInfo.cardNames.length > 0) &&
                                <RootLabelDiv>
                                    <div className="label-content">
                                        Card: {
                                            tooltipInfo.cardNames.length > 1 && allNumeric
                                                ? `${Math.min(...tooltipInfo.cardNames.map(Number))}-${Math.max(...tooltipInfo.cardNames.map(Number))}`
                                                : tooltipInfo.cardNames[0]
                                        }
                                    </div>
                                </RootLabelDiv>
                            }
                        </div>
                    </div>
                </Tooltip>
            </div>
            { !isEmpty(tag) && <div className={ 'insight-lane-info-tag-info' }>
                <span className="insight-lane-info-tag-text">{tag}</span>
            </div> }
        </div>
        <ConfigBar unit={unit} session={session} {...props} />
    </DefaultInfoContainer>;
});

interface TooltipInfo {
    content: string | JSX.Element;
    cardNames?: string[];
}

const getDefaultInfoTooltipTitle = (unit: KeyedInsightUnit, name: string): TooltipInfo => {
    // 判断是否为HCCL的group甬道，并且rank列表不为空
    const isGroupUnit = unit instanceof ThreadUnit && unit.metadata.groupNameValue !== '' && unit.metadata.rankList.length > 0;
    if (unit instanceof ROOT_UNIT) {
        if (!(unit.children && unit.children.length > 0)) {
            return { content: '' };
        }
        let clusterName: string = '';
        const cardNames: string[] = [];
        // 遍历root泳道下的card泳道
        for (const childUnit of unit.children) {
            const metaData = childUnit.metadata as CardMetaData;
            clusterName = metaData.cluster;
            cardNames.push(metaData.cardName);
        }
        return {
            content: (
                <>
                    {`Host: ${unit.metadata.host}`}<br/>
                    {`Cluster: ${clusterName}`}<br/>
                    {`Card: ${cardNames.join(', ')}`}
                </>
            ),
            cardNames,
        };
    } else if (isGroupUnit) {
        return { content: `(${unit.metadata.rankList.join(', ')})` };
    }
    return { content: name };
};

const shouldDisplayStickyButton = (session: Session, isHovered: boolean, hasPinButton: boolean, _isPinned: boolean): boolean => {
    return session.phase === 'download' && ((hasPinButton && isHovered) || _isPinned);
};

interface PinButtonProps {
    session: Session;
    unit: KeyedInsightUnit;
    isHovered: boolean;
    hasPinButton: boolean;
    isPinned: boolean;
}
const PinButton = observer(({ session, unit, isHovered, hasPinButton, isPinned: _isPinned }: PinButtonProps): JSX.Element => {
    const { t } = useTranslation();
    const style = { backgroundColor: 'transparent', marginLeft: 1 };
    const placeholder = <StyledButton style={style} icon={<StickyIcon fill="transparent" />} />;
    return <>
        {shouldDisplayStickyButton(session, isHovered, hasPinButton, _isPinned)
            ? <Tooltip title={t(`headerButtonTooltip:${_isPinned ? 'UnpinButton' : 'PinButton'}`)}>
                <StyledButton
                    data-testid={'pin-btn'}
                    style={style}
                    icon={_isPinned ? <PinIcon/> : <UnPinIcon/>}
                    onClick={(e: React.MouseEvent): void => {
                        e.stopPropagation();
                        e.preventDefault();
                        let execute = (): void => {};
                        const { pinnedUnits } = session;
                        if (!_isPinned) {
                            execute = (): void => {
                                platform.trace('stickyLane', {});
                                pinnedUnits.push(unit);
                                session.pinnedUnits = [...pinnedUnits];
                            };
                        } else {
                            execute = (): void => {
                                pinnedUnits.splice(pinnedUnits.indexOf(unit), 1);
                                session.pinnedUnits = [...pinnedUnits];
                            };
                        }
                        runInAction(() => {
                            execute();
                            switchPinned(unit);
                        });
                    }}
                />
            </Tooltip>
            : placeholder}
    </>;
});

interface ConfigBarProps {
    session: Session;
    unit: KeyedInsightUnit;
    isHovered: boolean;
    hasPinButton: boolean;
    isSelected: boolean;
    onConfigBarClick?: () => void;
}
const ConfigBar = observer(({ session, unit, isHovered, hasPinButton, isSelected, onConfigBarClick }: ConfigBarProps): JSX.Element => {
    const selectUnits = useSelectUnits(session);
    const deselectUnits = useDeselectUnits(session);
    const onStopPropagation = React.useCallback((e: React.MouseEvent) => {
        // 阻止事件冒泡
        e.stopPropagation();
        // 阻止默认事件行为
        e.preventDefault();
    }, []);
    const onCheckChange = React.useCallback((e: CheckboxChangeEvent) => {
        const checked = (e.target as HTMLInputElement).checked;
        if (Object.is(checked, true)) {
            selectUnits(unit);
        } else {
            deselectUnits(unit);
        }
    }, [unit]);
    return <div className="insight-lane-configbar" style={{ flex: 'none' }}>
        <div style={{ display: 'flex', marginLeft: 5, alignItems: 'center' }} onMouseUp={(e: React.MouseEvent): void => {
            if (!session.isDragging) {
                e.stopPropagation();
            }
            e.preventDefault();
        }}>
            {unit.configBar?.(session, unit.metadata, onConfigBarClick, isHovered, isSelected)}
            <UnitInfoActionDiv
                showCheckbox={session.phase === 'download' && (isHovered || isSelected)}
                onMouseUp={onStopPropagation}>
                <Checkbox onChange={onCheckChange} checked={isSelected}/>
            </UnitInfoActionDiv>
            <PinButton
                session={session}
                unit={unit}
                isHovered={isHovered || isSelected}
                hasPinButton={hasPinButton}
                isPinned={isPinned(unit)}
            />
        </div>
    </div>;
});

interface UnitInfoContentProps {
    unit: KeyedInsightUnit;
    session: Session;
    isHovered: boolean;
    hasPinButton: boolean;
    isPinned: boolean;
    isSelected: boolean;
    enableDrag?: boolean;
    onMouseDown: (e: React.MouseEvent) => void;
    onConfigBarClick?: () => void;
}

const InsightLaneInfoContainer = styled.div`
    height: 100%;
    display: flex;
    flex: auto;
    justify-content: space-between;
`;

function getProgressVisiable(unit: KeyedInsightUnit): boolean {
    return unit instanceof CardUnit && unit.metadata?.cardName !== 'Host' &&
        (unit.phase === 'analyzing' || unit.phase === 'download') && unit.shouldParse;
}

function getParserVisiable(unit: KeyedInsightUnit): boolean {
    return unit instanceof CardUnit && unit.metadata?.cardName !== 'Host' && unit.shouldParse;
}

const UnitInfoContent = observer(({ unit, session, ...props }: UnitInfoContentProps): JSX.Element => {
    const info = unit.renderInfo?.(session, unit.metadata, unit) ?? `${unit.name}`;
    const onDragMouseDown = (e: React.MouseEvent): void => {
        // 只允许鼠标左键拖拽
        if (e.button === 0 && !unit.isExpanded && props.enableDrag) {
            props.onMouseDown(e);
        }
    };
    if (typeof (info) === 'string') {
        return <DefaultInfo
            session={session}
            unit={unit}
            name={info}
            mouseDown={onDragMouseDown}
            {...props}
        />;
    }
    const tooltip = (children: JSX.Element): JSX.Element => unit.name === 'Card'
        ? <Tooltip placement="leftBottom"
            title={
                <>
                    <div>{(unit.metadata as CardMetaData).cardPath}</div>
                    <div>Timestamp Counter value at t=0: { bigSubtract(session.startTime, getTimeOffset(session, unit.metadata as CardMetaData)) } ns</div>
                </>
            }
        >{children}</Tooltip>
        : children;
    React.useEffect(() => {
        if ((unit.metadata as CardMetaData).cardName?.startsWith('Baseline')) { return; }
        if (session.isParserLoading) {
            runInAction((): void => {
                unit.isParseLoading = true;
            });
        } else {
            runInAction((): void => {
                unit.isParseLoading = false;
            });
        }
    }, [session.isParserLoading]);
    const handleStartClick = (): void => {
        const param: ParseCardsParam = { cards: [], dbPaths: [] };
        if (unit instanceof CardUnit && unit.metadata?.cardName !== '' && unit.metadata?.cardName !== 'Host') {
            param.cards.push(unit.metadata.cardId);
            param.dbPaths.push(unit.metadata.dbPath);
        }
        parseCards(param).then(() => {
            runInAction((): void => {
                unit.isParseLoading = true;
            });
        }).catch(err => {
            message.error(err);
        });
    };
    const getCursorStyle = (): string => {
        if (props.enableDrag) {
            return unit.isExpanded ? 'not-allowed' : 'grab';
        }
        return 'auto';
    };
    return <InsightLaneInfoContainer className="insight-lane-info">
        {
            tooltip(
                <div
                    onMouseDown={onDragMouseDown}
                    style={{ display: 'flex', flexGrow: 1, overflow: 'hidden', cursor: getCursorStyle() }}
                >
                    <div style={{ flexBasis: '50%', flexGrow: 1, flexShrink: 0, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                        {info}
                    </div>
                    {
                        (unit instanceof CardUnit && unit.metadata?.label !== '') && <TagDiv>
                            <div className="tag-content">
                                {unit.metadata.label}
                            </div>
                        </TagDiv>
                    }
                </div>,
            )
        }
        { !unit.shouldParse && unit.configBar && <ConfigBar
            session={session}
            unit={unit}
            {...props}
        />}
        { (getProgressVisiable(unit) && unit.isParseLoading)
            ? <div><UnitProgress unit={unit} realProgress={unit.progress} showProgress={unit.showProgress}/></div>
            : <></> }
        { getParserVisiable(unit)
            ? <div>
                <StyledButton transparent
                    icon={<StartIcon height={14} width={14}/>}
                    loading={unit.isParseLoading} onClick={(): void => handleStartClick()}/>
            </div>
            : <></> }
    </InsightLaneInfoContainer>;
});

const ExpandIcon = observer(({ unit }: { unit: KeyedInsightUnit }): JSX.Element => {
    return <div style={{ height: '20px', marginLeft: '4px' }}>
        <Arrow data-testid={'expand-btn'} style={{ transform: `rotate(${unit.isExpanded ? 0 : '-90deg'})`, cursor: 'pointer' }}
            className={`insight-unit-${unit.isExpanded ? 'expanded' : 'fold'}`} />
    </div>;
});

const UnitInfoContainer = styled.div<{ unit: InsightUnit; laneInfoWidth: number }>`
    position: relative;
    flex-grow: 0;
    flex-shrink: 0;
    width: ${(props): number => props.laneInfoWidth}px;
    flex-basis: ${(props): number => props.laneInfoWidth}px;
    height: ${(props): number => props.unit.height()}px;
    padding-left: ${(props): number => 14 * ((props.unit as any)[level] ?? 0)}px;
    text-align: left;
    color: ${(props): string => props.theme.unitInfoTextColor};
    display: flex;
    align-items: center;
`;

const UnitInfoActionDiv = styled.div<{ showCheckbox: boolean }>`
    display: ${(props): string => props.showCheckbox ? 'flex' : 'none'};
    align-items: center;
    max-height: 20px;
    scale: 0.8;
    margin-left: 6px;
`;

const UnitInfoBody = styled.div<{ offset: number }>`
    width: calc(100% - ${(props): number => props.offset}px);
    flex-grow: 1;
    display: flex;
    align-items: center;
    position: relative;
    transition: width 0.3s ease-in;
`;

interface UnitInfoProps {
    session: Session;
    unit: KeyedInsightUnit;
    laneInfoWidth: number;
    hasExpandIcon: boolean;
    hasPinButton: boolean;
    isPinned: boolean;
    height: number;
    className: string;
    isSelected: boolean;
    enableDrag?: boolean;
    onMouseDown: (e: React.MouseEvent) => void;
}

export function checkIsSameUnit(referenceMetadata?: ThreadMetaData, comparisonMetadata?: ThreadMetaData): boolean {
    if (!referenceMetadata || !comparisonMetadata) {
        return false;
    }
    const isSameBase = referenceMetadata.processId === comparisonMetadata.processId &&
        referenceMetadata.cardId === comparisonMetadata.cardId &&
        referenceMetadata.metaType === comparisonMetadata.metaType;
    return Array.isArray(referenceMetadata.threadIdList)
        ? isSameBase && referenceMetadata?.threadIdList.toString() === comparisonMetadata.threadIdList?.toString()
        : isSameBase && referenceMetadata.threadId === comparisonMetadata.threadId;
}

export const UnitInfo = observer(({ session, unit, laneInfoWidth, hasExpandIcon, className, ...props }: UnitInfoProps): JSX.Element => {
    const { isSelected } = props;
    const isDragging = session.isDragging;
    const [isHovered, setIsHovered] = React.useState(false);
    const isClickDownRef = React.useRef<boolean>(false);
    const selectUnit = useSelectUnit(session);
    const expandable = hasExpandIcon && (Boolean(unit.children) || (Boolean(unit.collapsible) && Boolean(unit.collapseAction)));
    const [isLoading, setLoading] = React.useState(false);
    const dbPath: string = unit.metadata.dbPath as string;

    useEffect(() => {
        if (
            // 如果是 Overlap Analysis 泳道
            unit.metadata.processName === 'Overlap Analysis' &&
            // 如果有卡地址
            dbPath &&
            // 如果卡的地址状态有被赋值
            session.asyncDataLoadingList[dbPath] &&
            // 如果 Overlap Analysis 正在加载中
            !session.asyncDataLoadingList[dbPath].OVERLAP_ANALYSIS) {
            setLoading(true);
            connector.addListener('updateAnalysisLoading', (e: any) => {
                if (e?.data?.body?.data?.dbId !== unit.metadata.dbPath) {
                    return;
                }
                setLoading(false);
            });
        }
    }, [unit.metadata.processName, session.asyncDataLoadingList[dbPath]?.OVERLAP_ANALYSIS]);

    const selectSelf = React.useCallback(() => {
        if (isSelected) { return; }
        selectUnit(unit);
        traceSingle('selectLane', [unit.name]);
    }, [selectUnit, unit, props.isSelected]);
    const onExpand = React.useCallback(async (_unit: KeyedInsightUnit) => {
        if (!expandable) {
            return;
        }
        // 清理历史记录
        delete _unit.onceExpand;
        _unit.children?.forEach(item => {
            delete item.onceExpand;
        });
        const spreadUnits = _unit.spreadUnits;
        if (spreadUnits?.phase === 'expand') {
            await spreadUnits.action?.(_unit, session);
        }
        runInAction(() => {
            _unit.isExpanded = !_unit.isExpanded;
            const _threadsToFetchMap: Map<string, InsightUnit> = new Map();
            if (_unit.isExpanded) {
                platform.trace(`unfold${_unit.name.replace(/\s*/g, '')}`, {});
                _unit.children?.forEach(item => {
                    if (item.collapsible && !item.isExpanded && item.collapseAction !== undefined) {
                        item.collapseAction?.(item);
                        item.isExpanded = true;
                    }
                    if (item.name === 'Thread' && !item.hasExpanded) {
                        const unitKey = getUnitUniqueId(item);
                        _threadsToFetchMap.set(unitKey, item);
                    }
                });
            }
            session.renderTrigger = !session.renderTrigger;
            if (_threadsToFetchMap.size > 0) {
                updateThreadsToFetch(session, _unit.isExpanded, _threadsToFetchMap);
            }
        });
        _unit.collapseAction?.(_unit);
    }, [session, expandable]);
    const onMouseDown = (e: React.MouseEvent<HTMLDivElement, MouseEvent>): void => {
        e.stopPropagation(); // 阻止冒泡防止事件冒泡到 ChartContainer 上
        isClickDownRef.current = true;
    };

    const selectedUnitChanged = (): void => {
        if (!session.sliceSelection.active) {
            return;
        }
        const comparisonMetadata = unit.metadata as ThreadMetaData;
        const referenceMetadata = session.sliceSelection.targetUnit?.metadata as ThreadMetaData;
        if (unit.name !== 'Thread' || !checkIsSameUnit(referenceMetadata, comparisonMetadata)) {
            session.selectedRange = undefined;
            session.sliceSelection.targetUnit = null;
        }
        if (unit.name === 'Thread' && !session.sliceSelection.targetUnit) {
            session.sliceSelection.targetUnit = unit as InsightUnit;
        }
    };

    const onMouseLeft = (): void => {
        // 拖拽时或不是在此泳道触发的mousedown，则不触发点击事件
        if (isDragging || !isClickDownRef.current) {
            return;
        }
        isClickDownRef.current = false;
        selectSelf();
        onExpand(unit);
        selectedUnitChanged();
    };
    const onMouseRight = (): void => {
        // 不是在此泳道触发的mousedown，则不触发点击事件
        if (!isClickDownRef.current) {
            return;
        }
        isClickDownRef.current = false;
        // 当前泳道已选中，不再对当前泳道选中
        if (!isSelected) {
            selectSelf();
            // 显示右键菜单需使用 contentmenu 事件，不在 mouseup 事件中处理
        }
        selectedUnitChanged();
    };
    const onUnitInfoContainerMouseUp = useComplexMouseEvent({
        // 为拖拽情况考虑，允许事件冒泡，在上层阻止
        stopPropagation: false,
        left: onMouseLeft,
        right: onMouseRight,
    });

    const handleMouseEnter = React.useCallback(() => {
        flushSync(() => {
            setIsHovered(true);
        });
    }, []);

    const handleMouseLeave = React.useCallback(() => {
        flushSync(() => {
            setIsHovered(false);
        });
    }, []);

    return <UnitInfoContainer
        className={`unit-info ${className ?? ''}`}
        unit={unit}
        laneInfoWidth={laneInfoWidth}
        onMouseEnter={handleMouseEnter}
        onMouseLeave={handleMouseLeave}
        onMouseDown={onMouseDown}
        onMouseUp={onUnitInfoContainerMouseUp}
    >
        <UnitInfoBody offset={0}>
            {/* position: 'absolute' 将 ExpandIcon 从文档流移出，不影响 UnitInfoContent 宽度显示 */}
            {!isLoading && expandable &&
                <div style={{ position: 'absolute' }}><ExpandIcon unit={unit}/></div>} {/* ExpandIcon(14px) */}
            {isLoading && <div style={{ position: 'absolute' }} className={'in-time-line-load'}></div>} {/* ExpandIcon(14px) */}
            {/* paddingLeft: '14px' 为 ExpandIcon 的显示留出空间 */}
            <div style={{ paddingLeft: '14px', width: '100%' }}>
                <UnitInfoContent
                    unit={unit}
                    session={session}
                    isHovered={isHovered}
                    onConfigBarClick={selectSelf}
                    {...props}
                />
            </div>
        </UnitInfoBody>
    </UnitInfoContainer>;
});
