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
import { observer } from 'mobx-react';
import { runInAction } from 'mobx';
import { useTranslation } from 'react-i18next';
import { Modal, Switch } from 'antd';
import { InputNumber } from '@insight/lib/components';
import { Label } from '../common/components/Common';
import type { MemScopeContext as Session } from '../state/MemScopeContext';

const ThresholdModal = observer(({ session, open, setOpen }: { session: Session; open: boolean; setOpen: any }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const { lazyUsedThreshold, delayedFreeThreshold, longIdleThreshold, onlyInefficient } = session;
    const [lazyThre, setLazyThre] = useState(lazyUsedThreshold);
    const [delayedThre, setDelayedThre] = useState(delayedFreeThreshold);
    const [longThre, setLongThre] = useState(longIdleThreshold);
    const [showInefficient, setShowInefficient] = useState(onlyInefficient);
    useEffect(() => {
        setLazyThre(lazyUsedThreshold);
        setDelayedThre(delayedFreeThreshold);
        setLongThre(longIdleThreshold);
        setShowInefficient(onlyInefficient);
    }, [JSON.stringify(lazyUsedThreshold), JSON.stringify(delayedFreeThreshold), JSON.stringify(longIdleThreshold), onlyInefficient]);
    const onOk = (): void => {
        runInAction(() => {
            session.lazyUsedThreshold = lazyThre;
            session.delayedFreeThreshold = delayedThre;
            session.longIdleThreshold = longThre;
            session.onlyInefficient = showInefficient;
        });
        setOpen(false);
    };
    const onCancel = (): void => {
        setLazyThre(lazyUsedThreshold);
        setDelayedThre(delayedFreeThreshold);
        setLongThre(longIdleThreshold);
        setShowInefficient(onlyInefficient);
        setOpen(false);
    };
    return (<Modal title={t('setThreshold')} open={open} destroyOnClose={true} data-testid={'thresholdModal'} onOk={onOk} onCancel={onCancel} style={{ top: '35%', minWidth: '520px' }}>
        <div>
            <Label name={t('lazyUsedThreshold')} style={{ marginRight: session.language === 'enUS' ? '28px' : '8px' }} />
            <InputNumber
                style={{ marginBottom: 8 }}
                value={lazyThre.valueT}
                precision={0}
                min={0}
                max={1000000000000}
                size="small"
                onChange={(value) => { setLazyThre({ ...lazyThre, valueT: value as number }); }}
            />
            <span> ns {t('or')} </span>
            <InputNumber
                style={{ marginBottom: 8 }}
                value={lazyThre.perT}
                onChange={(value) => { setLazyThre({ ...lazyThre, perT: value as number }); }}
                min={0}
                max={100}
                precision={0}
                size="small"
                data-testid={'lazyThrePer'}
            />
            <span> %</span>
        </div>
        <div>
            <Label name={t('delayedFreeThreshold')} />
            <InputNumber
                style={{ marginBottom: 8 }}
                value={delayedThre.valueT}
                onChange={(value) => { setDelayedThre({ ...delayedThre, valueT: value as number }); }}
                precision={0}
                min={0}
                max={1000000000000}
                size="small"
            />
            <span> ns {t('or')} </span>
            <InputNumber
                style={{ marginBottom: 8 }}
                value={delayedThre.perT}
                onChange={(value) => { setDelayedThre({ ...delayedThre, perT: value as number }); }}
                min={0}
                max={100}
                precision={0}
                size="small"
            />
            <span> %</span>
        </div>
        <div>
            <Label name={t('longIdleThreshold')} style={{ marginRight: session.language === 'enUS' ? '32px' : '8px' }} />
            <InputNumber
                style={{ marginBottom: 8 }}
                value={longThre.valueT}
                onChange={(value) => { setLongThre({ ...longThre, valueT: value as number }); }}
                precision={0}
                min={0}
                max={1000000000000}
                size="small"
            />
            <span> ns {t('or')} </span>
            <InputNumber
                style={{ marginBottom: 8 }}
                value={longThre.perT}
                onChange={(value) => { setLongThre({ ...longThre, perT: value as number }); }}
                min={0}
                max={100}
                precision={0}
                size="small"
            />
            <span> %</span>
        </div>
        <div>
            <Label name={t('onlyInefficient')} />
            <Switch checked={showInefficient} data-testid={'showInefficient'} onChange={(value) => { setShowInefficient(value); }} />
        </div>
    </Modal>);
});
export default ThresholdModal;
