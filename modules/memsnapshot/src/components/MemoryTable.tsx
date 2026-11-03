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
import React from 'react';
import { type RadioChangeEvent } from 'antd';
import type { CheckboxChangeEvent } from 'antd/lib/checkbox';
import { useTranslation } from 'react-i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { Radio, Checkbox } from '@insight/lib/components';
import type { MemSnapshotContext as Session } from '../state/MemSnapshotContext';

import BlocksTable from './BlocksTable';
import EventsTable from './EventsTable';

const SYSTEM_TABLE_TOOLBAR_HEIGHT = 42;
const MemoryTable = observer(({ session, height }: {
    session: Session;
    height?: number;
}): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const { tableType, autoFilterPotentialLeaks } = session;

    const closeAutoFilter = (): void => {
        runInAction(() => {
            session.autoFilterPotentialLeaks = false;
            session.blocksCurrentPage = 1;
        });
    };
    const autoFilterChange = (event: CheckboxChangeEvent): void => {
        if (event.target.checked) {
            runInAction(() => { session.autoFilterPotentialLeaks = true; });
            return;
        }
        closeAutoFilter();
    };
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
            }
        });
    };
    return (<>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <Radio.Group value={tableType} style={{ marginBottom: 10 }} onChange={radioChange}>
                <Radio data-testid={'blockViewRadio'} value={'blocks'}>{t('Block View')}</Radio>
                <Radio data-testid={'eventViewRadio'} value={'events'}>{t('Event View')}</Radio>
            </Radio.Group>
            {<></>}
            {tableType === 'blocks'
                ? <Checkbox checked={autoFilterPotentialLeaks} onChange={autoFilterChange}>{t('autoFilterPotentialLeaks')}</Checkbox>
                : <></>}
        </div>
        {tableType === 'blocks'
            ? <><BlocksTable session={session} height={height === undefined ? undefined : height - SYSTEM_TABLE_TOOLBAR_HEIGHT}/></>
            : <EventsTable session={session} height={height === undefined ? undefined : height - SYSTEM_TABLE_TOOLBAR_HEIGHT}/>}
    </>);
});
export default MemoryTable;
