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

import React, { ReactNode, useEffect, useMemo, useState } from 'react';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import { formatDate } from '../Common';
import type { Session } from '../../entity/session';
import { queryTopSummary } from '../../utils/RequestUtils';
import { CollapsiblePanel } from '@insight/lib/components';
import { MIDescriptions, MIDescriptionsItem, formateMicrosecond } from '@insight/lib/utils';
import { CompareData } from '../../utils/interface';

type BaseInfoData = Record<string, React.ReactNode>;
interface DisplayItem {
    label: ReactNode;
    key: string;
    visible?: boolean;
    value?: ReactNode;
}

const useDisplayFields = (session: Session): DisplayItem[] => {
    const { t } = useTranslation('summary');
    return [
        {
            label: t('ReportFile'),
            key: 'filePath',
        },
        {
            label: `${t('ReportSize')}(MB)`,
            key: 'dataSize',
        },
        {
            label: t('ReportCaptureTime'),
            key: 'collectStartTime',
            visible: session.unitcount !== 0,
        },
        {
            label: t('DeviceCount'),
            key: 'rankCount',
        },
        {
            label: t('StepCount'),
            key: 'stepNum',
        },
        {
            label: t('ProfilingSessionDuration'),
            key: 'collectDuration',
            visible: session.unitcount !== 0,
        },
    ].filter(displayItem => displayItem.visible !== false);
};

const updateBaseInfoData = async (setBaseinfo: (val: BaseInfoData) => void, session: Session): Promise<void> => {
    const res: any = await queryTopSummary({ isCompare: session.isCompare });
    runInAction(() => {
        session.rankCount = res.baseInfo.compare.rankCount;
        session.stepList = res.baseInfo.compare.stepList;
        session.baselineStepList = res.baseInfo.baseline.stepList;
    });
    setBaseinfo(wrapData(res?.baseInfo ?? {}, session.isCompare));
};

function wrapData(compareData: CompareData<BaseInfoData>, isCompare: boolean): BaseInfoData {
    const sourceList: Array<'compare' | 'baseline'> = isCompare ? ['compare', 'baseline'] : ['compare'];
    const wrapedData: CompareData<BaseInfoData> = {} as CompareData<BaseInfoData>;
    sourceList.forEach(source => {
        const data = compareData[source];
        wrapedData[source] = {
            ...data,
            collectDuration: formateMicrosecond(Number(data.collectDuration)),
            collectStartTime: formatDate(new Date(data.collectStartTime as number)),
            dataSize: typeof data.dataSize === 'number' && data.dataSize > 0.01 ? Number(data.dataSize?.toFixed(2)) : data.dataSize,
        };
    });
    const fields = Object.keys(compareData.compare);
    if (isCompare) {
        fields.push(...Object.keys(compareData.baseline));
    }
    const fieldSet = new Set(fields);
    const baseinfo: BaseInfoData = {};
    [...fieldSet].forEach(field => {
        baseinfo[field] = isCompare
            ? (<div><div>{wrapedData.compare[field]}</div><div>{wrapedData.baseline[field]}</div></div>)
            : <>{wrapedData.compare[field]}</>;
    });
    return baseinfo;
}

const BaseInfo = observer(({ session }: { session: Session}): JSX.Element => {
    // 基本信息原始数据
    const [baseinfo, setBaseinfo] = useState<BaseInfoData>({});
    const { t } = useTranslation('summary');
    const displayFields = useDisplayFields(session);
    // 界面显示
    const displaylist = useMemo<DisplayItem[]>(() => displayFields.map(infoItem => ({ ...infoItem, value: baseinfo[infoItem.key] }))
        , [baseinfo, displayFields]);

    useEffect(() => {
        if (!session.isClusterPathInList || !session.clusterCompleted) {
            setBaseinfo({});
            return;
        }
        const timer = setTimeout(() => {
            updateBaseInfoData(setBaseinfo, session);
        });
        return () => clearTimeout(timer);
    }, [session.isCompare, session.renderId, session.isClusterPathInList]);

    return <CollapsiblePanel title={t('BaseInfo')}>
        <MIDescriptions>
            {
                displaylist.map((item, index) => <MIDescriptionsItem key={index} label={item.label}>
                    { item.value}
                </MIDescriptionsItem>)
            }
        </MIDescriptions>
    </CollapsiblePanel>;
});

export default BaseInfo;
