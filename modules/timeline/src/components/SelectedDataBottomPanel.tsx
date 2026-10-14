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

import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import { SelectedDataBase } from './details/base/SelectedData';
import type { Session } from '../entity/session';
import type { SingleDataDesc } from '../entity/insight';
import { useSelectedDataDetailUpdater } from './details/hooks';
import type { AscendSliceDetail } from '../entity/data';
import { ResizeTable } from '@insight/lib/resize';
import { getDefaultColumData, getPageData, queryTableDataDetails } from './detailViews/Common';
import { DragDirection, useDraggableContainer } from '@insight/lib';
import { ChartErrorBoundary } from './error/ChartErrorBoundary';
import { MoreContainer, StyledMoreCard } from './BottomPanel';
import { DETAIL_HEADER_HEIGHT_ETC_PX } from './detailViews/SystemView';
import { runInAction } from 'mobx';
import { ArgsData } from './ArgsData';

import { CommunicationBandwidthInfoTable } from './details/CommunicationBandwidthInfoTable';

export { ArgsData } from './ArgsData';

interface DetailProps<T extends Record<string, unknown>> {
    session: Session;
    detail: SingleDataDesc<Record<string, unknown>, unknown>;
    children: React.FC<{data: T; session: Session}>;
    height: number;
}

const StyledSliceDetailDiv = styled.div`
    width: 100%;
    color: ${(props): string => props.theme.tableTextColor};
    display: flex;
    font-size: 12px;
`;

interface RidMoreProps {
    card: string | undefined;
    value: string;
    bottomHeight: number;
}

interface EqualCondition {
    col: string;
    content: string;
}

interface TableData {
    columnAttr: any[];
    tableData: any[];
    totalNum: number;
}

const RidMoreTable = observer(({ card, value, bottomHeight }: RidMoreProps) => {
    const [dataSource, setDataSource] = useState<any[]>([]);
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const [page, setPage] = useState(defaultPage);
    const defaultSorter = { field: '', order: '' };
    const [sorter, setSorter] = useState(defaultSorter);
    const [column, setColumn] = useState<any[]>([]);
    const [isLoading, setLoading] = useState(false);
    const [condition, setCondition] = useState({ page, sorter });
    useTranslation();
    useEffect(() => {
        setCondition({ page, sorter });
    }, [sorter, page.current, page.pageSize]);
    useEffect(() => {
        const equalCondition: EqualCondition = { col: 'rid', content: String(value) };
        const equalConditions: EqualCondition[] = [];
        equalConditions.push(equalCondition);
        const param = {
            rankId: card as string,
            pageSize: condition.page.pageSize,
            currentPage: condition.page.current,
            order: condition.sorter.order,
            orderBy: condition.sorter.field,
            selectKey: 0,
            type: '1',
            equalConditions,
        };
        if (param.rankId === '' || param.rankId === undefined) {
            return;
        }
        setLoading(true);
        queryTableDataDetails(param).then((res) => {
            const data = res as TableData;
            const cols = data.columnAttr.map((item) => {
                return { title: item.key as string, dataIndex: item.key, ...getDefaultColumData(item.key) };
            });
            setColumn(cols);
            setDataSource(data.tableData);
            setPage((prevPage: any) => ({ ...prevPage, total: res.totalNum }));
            setLoading(false);
        });
    }, [condition.page.current, condition.page.pageSize,
        condition.sorter.field, condition.sorter.order, card, value]);
    return <ResizeTable
        onChange={(pagination: unknown, filters: any, newsorter: unknown, extra: {action: string}): void => {
            if (extra.action === 'sort') {
                setSorter(newsorter as typeof sorter);
            }
        }}
        rowClassName={(): string => {
            return 'click-able';
        }}
        pagination={getPageData(page, setPage)} dataSource={dataSource} columns={column} size="small" loading={isLoading}
        scroll={{ y: bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
    />;
});

export const SelectedDataBottomPanel = observer(<T extends Record<string, unknown>>(props: DetailProps<T>): JSX.Element => {
    const { session, detail, height } = props;
    const { renderFields, data } = useSelectedDataDetailUpdater(session, detail, session.selectedData);
    const sliceDetailData = data as AscendSliceDetail;
    const [view] = useDraggableContainer({ dragDirection: DragDirection.RIGHT, draggableWH: 800 });
    const [dataLink, setDataLink] = useState<string>('');
    useEffect(() => {
        runInAction(() => {
            if (dataLink !== '') {
                session.drawLineMode = 'all';
            }
            session.ridLineType = dataLink;
            session.renderTrigger = !session.renderTrigger;
        });
    }, [dataLink]);
    useEffect(() => {
        setDataLink('');
    }, [renderFields]);
    if (!session.isIE) {
        return <div style={{ height: '100%', overflow: 'scroll' }}><StyledSliceDetailDiv>
            <SelectedDataBase renderer={renderFields}/>
            <div></div>
        </StyledSliceDetailDiv>
        <CommunicationBandwidthInfoTable details={sliceDetailData.communicationBandwidthInfo}/>
        {(data as AscendSliceDetail).args === undefined ? <></> : <ArgsData data={sliceDetailData}/>}
        </div>;
    } else {
        return view({
            mainContainer: <div style={{ height: '100%', overflow: 'scroll' }}><StyledSliceDetailDiv>
                <SelectedDataBase renderer={renderFields}/>
                <div></div>
            </StyledSliceDetailDiv>
            <CommunicationBandwidthInfoTable details={sliceDetailData.communicationBandwidthInfo}/>
            {(data as AscendSliceDetail).args === undefined ? <></> : <ArgsData linkUpdater={setDataLink} data={sliceDetailData}/>}
            </div>,
            draggableContainer: <StyledMoreCard
                className="moreContainer"
                bordered={false}>
                <ChartErrorBoundary className={'more-error'}>
                    <MoreContainer>
                        <RidMoreTable bottomHeight={height} card={session.selectedData?.cardId} value={dataLink}></RidMoreTable>
                    </MoreContainer>
                </ChartErrorBoundary>
            </StyledMoreCard>,
            id: 'SelectedDataBottomPanel',
        });
    }
});
