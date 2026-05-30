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
import { Card } from 'antd/lib/index';
import type { CardProps } from 'antd/lib/card';
import { isEmpty } from 'lodash';
import { observer } from 'mobx-react';
import React, { type ReactNode, useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import type { BottomPanelSingleRender } from '../entity/insight';
import type { Session } from '../entity/session';
import { BOTTOM_HEIGHT } from '../pages/SessionPage';
import { DragDirection, useDraggableContainer } from '@insight/lib';
import { ChartErrorBoundary } from './error/ChartErrorBoundary';
import { getDetailViewItem } from './detailViews/DetailView';
import { useFindDetail } from './detailViews/FindInWindow';
import { StyledTabs } from './base/StyledTabs';
import i18n from '@insight/lib/i18n';

interface CssProps {
    className?: string;
}

interface BottomPanelProps {
    session: Session;
}

interface DataCardType {
    height: number;
    session: Session;
    type: string;
}

const FILTER_HEIGHT = 31;
export const DETAIL_HEADER_HEIGHT_PX = 36;
const BOTTOM_PANEL_PADDING_X = 16; // 底部面板上下padding
const MORE_HEADER_HEIGHT_PX = 22;
const enum TriggerType {
    SELECTED_DATA = 'SELECTED_DATA',
    SELECTED_RANGE = 'SELECTED_RANGE',
}

const BottomTabs = styled(StyledTabs)`
    .ant-tabs-content-holder{
      padding: 8px 16px;
      background: ${(props): string => props.theme.bgColorDark};

      .ant-tabs-content{
        border-radius: 4px;
        overflow: hidden;
        background: ${(props): string => props.theme.bgColor};
      }
    }
`;

const Container = styled.div`
    display: flex;
    justify-content: flex-start;
    overflow: hidden;
    height: 100%;
    width: 100%;
    border-bottom: 1px solid ${(props): string => props.theme.backgroundColor};
    transition: height 0.5s;
    flex-shrink: 0;
    z-index: 3; // to cover tooltip, z-index of tooltip equals 2
    .ant-card, .ant-card-head {
        border-radius: 0;
        line-height: 1.2;
    }
    .title {
        height: ${DETAIL_HEADER_HEIGHT_PX - 2}px; // 2: draggable border-top width
        line-height: ${DETAIL_HEADER_HEIGHT_PX - 2}px;
    }
`;

const StyledCard = styled((props: CardProps & { width?: number}) => <Card {...props}/>)`
    height: 100%;
    width: 100%;
    .ant-card-head-wrapper {
        width: 100%;
    }
    .empty {
        margin: 0 auto;
        align-self: center;
        color: ${(props): string => props.theme.fontColor};
    }
    .ant-card-head-title {
        padding: 0;
    }
    & > .ant-card-head {
      position: relative;
      font-weight: normal;
      display: flex;
      border-bottom: 1px solid ${(p): string => p.theme.solidLine};
      padding: 0 16px;
      background-color: ${(p): string => p.theme.cardHeadBackgroundColor};
      color: ${(p): string => p.theme.fontColor};
      font-size: 0.875rem;
      width: 100%;

    }
    .ant-card-body {
        display: flex;
        background-color: ${(p): string => p.theme.contentBackgroundColor};
        height: 100%;
        width: 100%;
        padding: 0;
    }
`;

export const StyledMoreCard = styled(StyledCard)`
    background-color: ${(p): string => p.theme.contentBackgroundColor};
    & > .ant-card-head {
        min-height: 0;
        font-size: 1rem;
        background-color: ${(p): string => p.theme.contentBackgroundColor};
        border-left: ${(p): string => p.theme.dividerColor} 1px solid;
    }
    .ant-card-body {
        border-left: ${(p): string => p.theme.dividerColor} 1px solid;
        border-top: 1px solid ${(p): string => p.theme.solidLine};
    }
    .ant-card-head-wrapper {
        background-color: ${(p): string => p.theme.contentBackgroundColor};
    }
    .ant-card-head-title {
        background-color: ${(p): string => p.theme.contentBackgroundColor};
        height: ${MORE_HEADER_HEIGHT_PX}px;
        padding-top: 3px;
        text-align: start;
        & span {
            margin-right: 16px;
            font-size: 1rem;
        }
    }
`;

const DetailContainer = styled.div`
    width: 100%;
    transition: width 0.5s;
    user-select: text;
    .emptyContainer {
        height: 100%;
        display: flex;
    }
`;

export const MoreContainer = styled.div`
    overflow: hidden;
    display: flex;
    position: relative;
    width: 100%;
    height: 100%;
`;

const NoDetail = (): JSX.Element => {
    const { t } = useTranslation();
    return <div className="empty">{t('No Data')}</div>;
};

interface BottomPanelReactNodes {
    detail: ReactNode;
    moreTitle: ReactNode;
    more: ReactNode;
    toolbar: ReactNode;
    moreWh?: number;
    open?: boolean;
}

const useBottomPanelReactNodes = (session: Session, height: number, type: string): BottomPanelReactNodes => {
    const isSliceDetail = type === TriggerType.SELECTED_DATA;
    const { selectedUnitKeys, selectedUnits } = session;
    const bottomPanelComponents = React.useMemo(() => {
        const sessionUnit = selectedUnits?.find(unit => unit.bottomPanelRender);
        return sessionUnit?.bottomPanelRender?.(session, sessionUnit?.metadata);
    }, [session, session.units.length, isSliceDetail ? String(selectedUnitKeys) : session.selectedRange]);
    const bottomPanelComponent = isSliceDetail ? bottomPanelComponents?.[0] : bottomPanelComponents?.[1];
    const contentHeight = bottomPanelComponent?.Toolbar !== undefined
        ? (height - DETAIL_HEADER_HEIGHT_PX - FILTER_HEIGHT - BOTTOM_PANEL_PADDING_X)
        : (height - DETAIL_HEADER_HEIGHT_PX - BOTTOM_PANEL_PADDING_X);
    const { t } = useTranslation('timeline');
    return React.useMemo(() => {
        return {
            detail: getDetailContent(session, contentHeight, bottomPanelComponent),
            moreTitle: getMoreTitle(session, bottomPanelComponent),
            // More Container has extra height
            more: getMoreContent(session, contentHeight - MORE_HEADER_HEIGHT_PX, bottomPanelComponent),
            toolbar: getFilterContent(session, bottomPanelComponent),
            moreWh: bottomPanelComponent?.moreWh ?? 590,
            open: bottomPanelComponent?.open ?? true,
        };
    }, [bottomPanelComponents, height, t]);
};

/* decide what to put in Detail Index */
const getDetailContent = (session: Session, height: number, bottomPanelComponents?: ReturnType<BottomPanelSingleRender>): JSX.Element => {
    if (session.selectedUnitKeys.length === 0 || !bottomPanelComponents?.Detail) {
        return <div className="emptyContainer"><NoDetail/></div>;
    }
    return <bottomPanelComponents.Detail session={session} height={height} />;
};

/* decide what to put in More container */
const getMoreContent = (session: Session, height: number, bottomPanelComponents?: ReturnType<BottomPanelSingleRender>): JSX.Element | undefined => {
    if (session.selectedUnitKeys.length === 0) {
        return <NoDetail/>;
    }
    return bottomPanelComponents?.More && <bottomPanelComponents.More session={session} height={height} />;
};

/* More title */
const getMoreTitle = (session: Session, bottomPanelComponents?: ReturnType<BottomPanelSingleRender>): JSX.Element => {
    const Title = bottomPanelComponents?.MoreTitle;
    if (typeof Title === 'string' || Title === undefined) {
        return <span>{Title ?? i18n.t('Details')}</span>;
    }
    return <Title session={session} />;
};

/* Filter container in bottom */
const getFilterContent = (session: Session, bottomPanelComponents?: ReturnType<BottomPanelSingleRender>): (JSX.Element | undefined) => {
    return bottomPanelComponents?.Toolbar && <bottomPanelComponents.Toolbar session={session} />;
};

const DataCard = observer(({ session, height, type }: DataCardType) => {
    const { detail, moreTitle, more, toolbar, moreWh = 590 } = useBottomPanelReactNodes(session, height, type);
    const [view] = useDraggableContainer({ dragDirection: DragDirection.RIGHT, draggableWH: moreWh });
    return <div style={{ width: '100%', zIndex: 3, height: '100%' }}>
        {
            !isEmpty(more)
                ? view({
                    mainContainer: <StyledCard
                        bordered={ false }>
                        <ChartErrorBoundary className={'detail-more-error'}>
                            <DetailContainer>
                                {detail}
                            </DetailContainer>
                        </ChartErrorBoundary>
                    </StyledCard>,
                    draggableContainer: <StyledMoreCard
                        className="moreContainer"
                        title={moreTitle}
                        bordered={ false } >
                        <ChartErrorBoundary className={'more-error'}>
                            <MoreContainer>
                                {more}
                            </MoreContainer>
                        </ChartErrorBoundary>
                    </StyledMoreCard>,
                    id: 'detailMore',
                })
                : <StyledCard
                    bordered={ false }>
                    <ChartErrorBoundary className={'detail-error'}>
                        <DetailContainer>
                            {detail}
                        </DetailContainer>
                    </ChartErrorBoundary>
                </StyledCard>
        }
        {toolbar}
    </div>;
});

export const BottomPanel = observer((props: BottomPanelProps & CssProps) => {
    const { session } = props;
    const [bottomHeight, setBottomHeight] = useState(BOTTOM_HEIGHT);
    const [item, setItem] = useState<string>('SliceDetail');
    const ref = useRef<HTMLDivElement>(null);
    const items = [
        getDataCardItem(bottomHeight, session, TriggerType.SELECTED_DATA),
        getDataCardItem(bottomHeight, session, TriggerType.SELECTED_RANGE),
        getDetailViewItem(session, bottomHeight),
        useFindDetail(session, bottomHeight),
    ];

    useEffect(() => {
        const bottomResize = (): void => setBottomHeight(ref.current?.clientHeight ?? BOTTOM_HEIGHT);
        window.addEventListener('resize', bottomResize);
        window.addEventListener('bottomResize', bottomResize);
        return (): void => {
            window.removeEventListener('bottomResize', bottomResize);
            window.removeEventListener('resize', bottomResize);
        };
    }, [setBottomHeight]);
    useEffect(() => {
        if (session.selectedData?.showSelectedData === true || session.selectedData?.showDetail === false) {
            setItem('SliceDetail');
            return;
        }
        if (session.selectedRange) {
            setItem('SliceList');
        } else {
            setItem('SliceDetail');
        }
    }, [session.selectedData, session.selectedRange]);

    useEffect(() => {
        setItem('Find');
    }, [session.doContextSearch]);

    useEffect(() => {
        setItem('SystemView');
    }, [session.showEvent, session.isTimeAnalysisMode]);

    return (<Container ref={ref} className="bottomPanelContainer">
        <BottomTabs style={{ width: '100%' }} items={items} activeKey={item} onTabClick={(key): void => setItem(key)}/>
    </Container>);
});

function getDataCardItem(bottomHeight: number, session: Session, triggerType: string): any {
    if (triggerType === TriggerType.SELECTED_RANGE) {
        return {
            label: DataCardTitle('Slice List'),
            key: 'SliceList',
            children: <DataCard height={bottomHeight} session={session} type={triggerType} />,
        };
    }
    return {
        label: DataCardTitle('Slice Detail'),
        key: 'SliceDetail',
        children: <DataCard height={bottomHeight} session={session} type={triggerType} />,
    };
}

const DataCardTitle = (title: string): JSX.Element => {
    const { t } = useTranslation('timeline');
    return <div className={'title'}>{<span>{t(title)}</span>}</div>;
};
