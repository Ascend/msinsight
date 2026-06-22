/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import TopWarpStallReasonChart from './TopWarpStallReasonChart';
import TopWarpStallReasonTable from './TopWarpStallReasonTable';
import { queryTopWarpStallReason } from '../../RequestUtils';
import { CollapsiblePanel } from '@insight/lib/components';
import { type Session } from '../../../entity/session';
import { CompareData } from '../../../utils/interface';

export interface IStallReasonData {
    name: string;
    value: number;
}

interface Idata {
    unit: string;
    data: Array<CompareData<IStallReasonData>>;
}

const defaultData = { unit: 'count', data: [] };

const index = observer(({ session }: { session: Session }): JSX.Element | null => {
    const [data, setData] = useState<Idata>(defaultData);
    const { t } = useTranslation('details');

    const fetchData = async (isCompared: boolean): Promise<void> => {
        try {
            const res = await queryTopWarpStallReason({ isCompared });
            const renderData = {
                unit: res?.unit ?? 'count',
                data: res?.data ?? [],
            } as Idata;
            setData(renderData);
        } catch {
            setData(defaultData);
        }
    };

    useEffect(() => {
        if (!session.parseStatus) {
            setData(defaultData);
            return;
        }
        fetchData(session.dirInfo.isCompare);
    }, [session.parseStatus, session.updateId, session.dirInfo]);

    if (data.data.length === 0) {
        return null;
    }

    return (
        <CollapsiblePanel title={t('TopWarpStallReason')} collapsible>
            <TopWarpStallReasonChart data={data.data} isCompared={session.dirInfo.isCompare} />
            <TopWarpStallReasonTable data={data.data} isCompared={session.dirInfo.isCompare} />
        </CollapsiblePanel>
    );
});

export default index;
