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
import { observer } from 'mobx-react';
import { Alert } from 'antd';
import { defaultOperator } from '../api';
import type { Services } from '../app/services';
import type { Operator, RankSelection, Scope } from '../domain/types';
import { Result, Section } from '../components/Section';
import { Selector, options } from '../components/Selector';
import { OperatorTimeline } from '../features/operatorTimeline';
import type { Range } from '../features/operatorTimeline/navigation';
import { useCommunicationStore } from '../store/context';
import { useQuery } from '../state/useQuery';
import { useTranslation } from 'react-i18next';

interface Props {
    scope: Scope;
    services: Services;
}

export const TimelineSection = observer(function TimelineSection({ scope, services }: Props): JSX.Element {
    const { dark, durationReady, canLocate, locationIntent: intent } = useCommunicationStore();
    const { t } = useTranslation('communication');
    const { api, bridge } = services;
    const key = JSON.stringify(scope);
    const thumbnailOperators = useQuery(`${key}:thumbnail-operators`, durationReady, signal => api.operators(scope, false, signal));
    const [thumbnailChoice, setThumbnailChoice] = useState('');
    const thumbnailOp = thumbnailOperators.data?.includes(thumbnailChoice) ? thumbnailChoice : defaultOperator(thumbnailOperators.data ?? []);
    const sectionRef = useRef<HTMLElement>(null);
    const timelineRange = useRef<Range>([0, 100]);
    const [alignment, setAlignment] = useState('');
    const [locateError, setLocateError] = useState(false);
    const locateRequest = useRef<AbortController>();
    const thumbnail = useQuery(`${key}:${thumbnailOp}:${alignment}`, durationReady && !!thumbnailOp, signal => api.timeline(scope, thumbnailOp, alignment, signal));
    useEffect(() => {
        if (!intent?.operatorName) { return; }
        timelineRange.current = [0, 100];
        setAlignment('');
        setThumbnailChoice(intent.operatorName);
        // The host can deliver the jump while this iframe is still hidden. Observe the viewport,
        // since the thumbnail itself can be entirely below the fold.
        const observer = new IntersectionObserver(([entry]) => {
            if (!entry.isIntersecting || !entry.intersectionRect.height) { return; }
            observer.disconnect();
            sectionRef.current?.scrollIntoView({ block: 'start', inline: 'nearest' });
        });
        observer.observe(document.documentElement);
        return () => observer.disconnect();
    }, [intent]);
    useEffect(() => () => locateRequest.current?.abort(), []);
    const locate = async ({ rankId, dbPath }: RankSelection, op: Operator): Promise<void> => {
        locateRequest.current?.abort(); const controller = new AbortController(); locateRequest.current = controller; setLocateError(false);
        try {
            const result = await api.locate({ rankId, dbPath }, op.operatorName, scope.clusterPath, controller.signal);
            if (controller.signal.aborted) { return; }
            bridge.send('switchModule', {
                switchTo: 'timeline',
                toModuleEvent: 'locateUnit',
                params: {
                    ...result,
                    dbPath,
                    name: op.operatorName,
                    clusterPath: scope.clusterPath,
                    processId: result.pid,
                    startTime: result.startTime,
                    rankId: result.rankId,
                    duration: result.duration ?? op.duration,
                    showSelectedData: true,
                },
            });
        } catch { if (!controller.signal.aborted) { setLocateError(true); } }
    };
    return (
        <Section ref={sectionRef} title={t('sessionTitle.Communication')} tools={<Selector label={t('searchCriteria.OperatorName')} value={thumbnailOp} options={options(thumbnailOperators.data ?? [])}
            loading={thumbnailOperators.loading} onChange={value => { timelineRange.current = [0, 100]; setThumbnailChoice(value); setAlignment(''); }} />}>
            {!durationReady
                ? <Alert type="info" message={t('modern.parsing')} />
                : <Result query={thumbnailOperators}>{() => <Result query={thumbnail} empty={!thumbnail.data?.rows.length}>{data =>
                    <OperatorTimeline data={data} dark={dark} canLocate={canLocate}
                        initialRange={timelineRange.current} onRangeChange={value => { timelineRange.current = value; }}
                        onLocate={(row, op) => { void locate(row, op); }} onAlign={value => { setAlignment(value); }} aligned={alignment} />
                }</Result>}</Result>}
            {locateError && <Alert type="error" message={t('modern.timelineUnavailable')} />}
        </Section>
    );
});
