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
import { type RadioChangeEvent } from 'antd';
import type { CheckboxChangeEvent } from 'antd/lib/checkbox';
import { useTranslation } from 'react-i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { Radio, Button, Checkbox } from '@insight/lib/components';
import { Session } from '../entity/session';
import BlocksTable from './BlocksTable';
import EventsTable from './EventsTable';
import ThresholdModal from './ThresholdModal';
const MemoryTable = observer(({ session }: { session: Session }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const { tableType, module, autoFilterPotentialLeaks } = session;
    const [open, setOpen] = useState(false);
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
                session.lazyUsedThreshold = { perT: null, valueT: null };
                session.delayedFreeThreshold = { perT: null, valueT: null };
                session.longIdleThreshold = { perT: null, valueT: null };
                session.onlyInefficient = false;
            }
        });
    };
    return (
        <>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <Radio.Group value={tableType}
                    style={{ marginBottom: 10 }}
                    onChange={radioChange}>
                    <Radio data-testid={'blockViewRadio'} value={'blocks'}>{t('Block View')}</Radio>
                    <Radio
                        data-testid={'eventViewRadio'} value={'events'}>{t('Event View')}</Radio>
                </Radio.Group>
                {tableType === 'blocks' && module === 'leaks' ? <Button type="primary" onClick={() => { setOpen(true); }}>{t('setThreshold')}</Button> : <></>}
                {tableType === 'blocks' && module === 'memsnapshot'
                    ? <Checkbox checked={autoFilterPotentialLeaks} onChange={autoFilterChange}>{t('autoFilterPotentialLeaks')}</Checkbox>
                    : <></>}
            </div>
            {tableType === 'blocks' ? <><BlocksTable session={session} /><ThresholdModal session={session} open={open} setOpen={setOpen} /></> : <EventsTable session={session} />}
        </>
    );
});

export default MemoryTable;
