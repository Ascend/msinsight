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
import { CaretDownIcon } from '@insight/lib/icon';
import { safeJSONParse } from '@insight/lib/utils';
import { ResizeTable } from '@insight/lib/resize';
import { getDefaultColumData, getPageData, queryTableDataDetails } from './detailViews/Common';
import { DragDirection, useDraggableContainer } from '@insight/lib';
import { ChartErrorBoundary } from './error/ChartErrorBoundary';
import { MoreContainer, StyledMoreCard } from './BottomPanel';
import { DETAIL_HEADER_HEIGHT_ETC_PX } from './detailViews/SystemView';
import { runInAction } from 'mobx';

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

const StyledSliceArgsDiv = styled.div`
    width: 100%;
    color: ${(props): string => props.theme.tableTextColor};
    text-align: left;
    font-size: 12px;
`;

const StyledSliceArgsRow = styled.div`
    display: flex;
    margin-left: 24px;
    padding: 8px 0;
    word-break: break-all;

    .key {
        flex: none;
        width: 180px;
        margin-right: 20px;
    }
`;

const createContentWithBreaks = (content: string): React.ReactNode => {
    return content?.split('\n').map((line, index) => (
        <React.Fragment key={index}>
            {line}
            <br />
        </React.Fragment>
    ));
};

const createContentNormal = (content: number | string | Array<number | string> | object[], setDataLink?: (data: string) => void): string | React.ReactNode => {
    if (Array.isArray(content)) {
        if (content.length > 0 && typeof content[0] === 'object' && content[0] !== null) {
            const headers = Object.keys(content[0]);
            if (headers.length === 0) {
                return '';
            }
            const columns = headers.map((key: string) => ({
                title: key,
                dataIndex: key,
                ...getDefaultColumData(key),
                sorter: (a: any, b: any) => {
                    const numA = Number(a[key]);
                    const numB = Number(b[key]);
                    if (!isNaN(numA) && !isNaN(numB)) {
                        return numA - numB;
                    }
                    return a[key].localeCompare(b[key]);
                },
            }));
            const dataSource = content as object[];
            return <ResizeTable
                onRow={(record: any): { onClick: () => void } => ({
                    onClick: (): void => {
                        if (record.rid !== undefined && setDataLink !== undefined) {
                            setDataLink(record.rid);
                        }
                    },
                })}
                columns={columns}
                dataSource={dataSource}
                pagination={false}/>;
        }
        return createContentWithArray(content as string[], setDataLink);
    }
    if (content === undefined || content === '') {
        return '""';
    }
    if (content === null) {
        return 'null';
    }
    return Number.isFinite(Number(content)) ? `${content}` : `"${content}"`;
};

const createContentWithArray = (content: Array<number | string>, setDataLink?: (data: string) => void): string => {
    return `[${content.map((item) => createContentNormal(item, setDataLink)).filter((x) => typeof x === 'string').join(', ')}]`;
};

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

const ArgsData = observer(({ data, linkUpdater }: { data: AscendSliceDetail; linkUpdater?: (data: string) => void}): JSX.Element => {
    const argsJson = data.args;
    const [isHiddenArgs, setHidden] = useState(false);
    const { t } = useTranslation('timeline', { keyPrefix: 'sliceDetail' });
    if (argsJson === undefined) {
        return <></>;
    }
    const args = safeJSONParse(argsJson);
    if (args === null) {
        return <></>;
    }
    const breakKeys = ['Call stack', 'code'];
    return <div>
        <StyledSliceArgsDiv>
            <CaretDownIcon
                onClick={ (): void => setHidden(!isHiddenArgs) } style={{ margin: '-2px 0 0 8px', float: 'left', transform: `rotate(${!isHiddenArgs ? 0 : '-90deg'}) translate(${!isHiddenArgs ? '-2' : '1'}px, ${!isHiddenArgs ? '0' : '-2'}px)`, cursor: 'pointer' }}/>
            <div style={{ fontWeight: 'bold', margin: '8px 0 0 8px' }}>{t('Args')}</div>
            {!isHiddenArgs
                ? Object.keys(args).map(key => {
                    return <StyledSliceArgsRow key={key}>
                        <div className="key">{key}</div>
                        <div className="value">
                            { breakKeys.includes(key) ? createContentWithBreaks(args[key]) : createContentNormal(args[key], linkUpdater) }
                        </div>
                    </StyledSliceArgsRow>;
                })
                : <></>}
        </StyledSliceArgsDiv>
    </div>;
});

export const SelectedDataBottomPanel = observer(<T extends Record<string, unknown>>(props: DetailProps<T>): JSX.Element => {
    const { session, detail, height } = props;
    const { renderFields, data } = useSelectedDataDetailUpdater(session, detail, session.selectedData);
    const sliceDetailData = data as AscendSliceDetail;
    const [view] = useDraggableContainer({ dragDirection: DragDirection.RIGHT, draggableWH: 800 });
    const [dataLink, setDataLink] = useState<string>('');
    useEffect(() => {
        runInAction(() => {
            session.drawLineMode = 'all';
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
        {(data as AscendSliceDetail).args === undefined ? <></> : <ArgsData data={sliceDetailData}/>}
        </div>;
    } else {
        return view({
            mainContainer: <div style={{ height: '100%', overflow: 'scroll' }}><StyledSliceDetailDiv>
                <SelectedDataBase renderer={renderFields}/>
                <div></div>
            </StyledSliceDetailDiv>
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
