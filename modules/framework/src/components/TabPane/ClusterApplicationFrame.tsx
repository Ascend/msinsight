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
import React, { useEffect, useRef, useState } from 'react';
import { Result } from 'antd';
import { Button, Spin } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import { clusterNavigation } from '../../cluster/navigation';
import { clusterView } from '../../cluster/viewVersion';
interface Props { name: string; src?: string; visible: boolean; modern: boolean; onReady: (ready?: boolean) => void }
/** Modern applications acknowledge readiness; an HTTP success alone is not a mounted application. */
export function ClusterApplicationFrame({ name, src, visible, modern, onReady }: Props): JSX.Element {
    const { t } = useTranslation('framework');
    const frame = useRef<HTMLIFrameElement>(null);
    const callback = useRef(onReady); callback.current = onReady;
    const [status, setStatus] = useState<'loading' | 'ready' | 'failed'>('loading');
    const [attempt, setAttempt] = useState(0);
    useEffect(() => {
        clusterNavigation.begin(name);
        if (!src) { return () => clusterNavigation.remove(name); }
        setStatus('loading');
        const timer = window.setTimeout(() => setStatus(value => value === 'ready' ? value : 'failed'), 15000);
        const ready = (event: MessageEvent): void => {
            if (event.source !== frame.current?.contentWindow) { return; }
            const expected = new URL(src, window.location.href).origin;
            if (expected !== 'null' && event.origin !== expected) { return; }
            try {
                const data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
                if (data?.event !== 'clusterPage/ready' && !(name === 'Communication' && data?.event === 'communication/ready')) { return; }
                clearTimeout(timer); setStatus('ready'); callback.current(true);
            } catch { /* Ignore unrelated messages. */ }
        };
        window.addEventListener('message', ready);
        return () => { clusterNavigation.remove(name); clearTimeout(timer); window.removeEventListener('message', ready); };
    }, [src, attempt, name]);
    const failed = !src || status === 'failed';
    return <div style={{ display: visible ? 'block' : 'none', position: 'relative', height: '100%' }}>
        {failed && <Result status="warning" title={t('clusterView.unavailable', { page: name })} extra={<>
            {src && <Button onClick={() => setAttempt(value => value + 1)}>{t('clusterView.retry')}</Button>}
            <Button type="primary" onClick={() => clusterView.select('legacy')}>{t('clusterView.legacy')}</Button>
        </>} />}
        {src && <iframe ref={frame} key={attempt} id={name} name={name} title={name} src={src} data-cluster-application={name}
            style={{ display: failed ? 'none' : 'block', height: '100%', width: '100%' }} onError={() => setStatus('failed')}
            onLoad={() => { callback.current(!modern); if (!modern) { setStatus('ready'); } }} />}
        {src && status === 'loading' && <Spin style={{ position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)' }} />}
    </div>;
}
