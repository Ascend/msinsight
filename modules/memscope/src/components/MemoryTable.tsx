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
import React, { useState } from 'react';
import { type RadioChangeEvent } from 'antd';

import { useTranslation } from 'react-i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { Radio, Button } from '@insight/lib/components';
import type { MemScopeContext as Session } from '../state/MemScopeContext';
import { isHostMemoryEventType } from '../common/utils/utils';
import BlocksTable from './BlocksTable';
import EventsTable from './EventsTable';
import ThresholdModal from './ThresholdModal';
const SYSTEM_TABLE_TOOLBAR_HEIGHT = 42;
const MemoryTable = observer(({ session, height }: {
    session: Session;
    height?: number;
}): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const { tableType, eventType } = session;
    const [open, setOpen] = useState(false);
    const supportsInefficientFilter = !isHostMemoryEventType(eventType);
    const radioChange = (e: RadioChangeEvent): void => {
        runInAction(() => {
            const type = e.target.value;
            session.tableType = type;
            if (type === 'blocks') {
                session.blocksTableData = [];
                session.blocksTableHeader = [];
                session.blocksCurrentPage = 1;
                session.blocksPageSize = 10;
                session.blocksTotal = 0;
                session.blocksOrder = '';
                session.blocksOrderBy = '';
                session.blocksFilters = {};
                session.blocksRangeFilters = {};
            } else {
                session.eventsTableData = [];
                session.eventsTableHeader = [];
                session.eventsCurrentPage = 1;
                session.eventsPageSize = 10;
                session.eventsTotal = 0;
                session.eventsOrder = '';
                session.eventsOrderBy = '';
                session.eventsFilters = {};
                session.eventsRangeFilters = {};
                session.lazyUsedThreshold = { perT: null, valueT: null };
                session.delayedFreeThreshold = { perT: null, valueT: null };
                session.longIdleThreshold = { perT: null, valueT: null };
                session.onlyInefficient = false;
            }
        });
    };
    return (<>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <Radio.Group value={tableType} style={{ marginBottom: 10 }} onChange={radioChange}>
                <Radio data-testid={'blockViewRadio'} value={'blocks'}>{t('Block View')}</Radio>
                <Radio data-testid={'eventViewRadio'} value={'events'}>{t('Event View')}</Radio>
            </Radio.Group>
            {tableType === 'blocks' && supportsInefficientFilter
                ? <Button type="primary" onClick={() => { setOpen(true); }}>{t('setThreshold')}</Button>
                : <></>}
            {<></>}
        </div>
        {tableType === 'blocks'
            ? <><BlocksTable session={session} height={height === undefined ? undefined : height - SYSTEM_TABLE_TOOLBAR_HEIGHT}/><ThresholdModal session={session} open={open} setOpen={setOpen}/></>
            : <EventsTable session={session} height={height === undefined ? undefined : height - SYSTEM_TABLE_TOOLBAR_HEIGHT}/>}
    </>);
});
export default MemoryTable;
