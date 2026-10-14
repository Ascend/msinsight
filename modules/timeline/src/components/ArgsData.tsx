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

import React, { useState } from 'react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import type { AscendSliceDetail } from '../entity/data';
import { AlarmIcon, CaretDownIcon } from '@insight/lib/icon';
import { Tooltip } from '@insight/lib/components';
import { safeJSONParse } from '@insight/lib/utils';
import { ResizeTable } from '@insight/lib/resize';
import { getDefaultColumData } from './detailViews/Common';

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

export const ArgsData = observer(({ data, linkUpdater }: { data: AscendSliceDetail; linkUpdater?: (data: string) => void}): JSX.Element => {
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
    // 后端在字段取值不保证对应当前算子时下发 _ambiguousKeys；所有 _ 前缀 key 都是协议标记，不渲染为参数行
    const ambiguousFields: string[] = Array.isArray(args._ambiguousKeys) ? args._ambiguousKeys : [];
    const displayKeys = Object.keys(args).filter((key) => !key.startsWith('_'));
    return <div>
        <StyledSliceArgsDiv>
            <CaretDownIcon
                onClick={ (): void => setHidden(!isHiddenArgs) } style={{ margin: '-2px 0 0 8px', float: 'left', transform: `rotate(${!isHiddenArgs ? 0 : '-90deg'}) translate(${!isHiddenArgs ? '-2' : '1'}px, ${!isHiddenArgs ? '0' : '-2'}px)`, cursor: 'pointer' }}/>
            <div style={{ fontWeight: 'bold', margin: '8px 0 0 8px' }}>{t('Args')}</div>
            {!isHiddenArgs
                ? displayKeys.map(key => {
                    return <StyledSliceArgsRow key={key}>
                        <div className="key">
                            {key}
                            {ambiguousFields.includes(key) &&
                                <Tooltip
                                    title={t('Ambiguous Field Hint')}
                                    placement={'right'}
                                    overlayStyle={{ maxWidth: '400px' }}>
                                    {/* AlarmIcon 内部会覆盖 style 里的 margin 并自带 marginTop 2px，间距与对齐统一由外层 span 控制 */}
                                    <span style={{ display: 'inline-flex', alignItems: 'center', verticalAlign: 'middle', marginLeft: '6px', marginTop: '-2px', cursor: 'help' }}>
                                        <AlarmIcon width={12} height={12}/>
                                    </span>
                                </Tooltip>
                            }
                        </div>
                        <div className="value">
                            { breakKeys.includes(key) ? createContentWithBreaks(args[key]) : createContentNormal(args[key], linkUpdater) }
                        </div>
                    </StyledSliceArgsRow>;
                })
                : <></>}
        </StyledSliceArgsDiv>
    </div>;
});
