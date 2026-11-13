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
import React, { useEffect, useMemo, useState } from 'react';
import styled from '@emotion/styled';
import { Button, Checkbox, CollapsiblePanel } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import { FrameTreeExplorer } from '../frameTree/FrameTreeExplorer';
import { requestFrameTreePage } from '../frameTree/api';
import { formatExactBytes, groupUnknownFrames } from '../frameTree/model';
import { loadFrameTree } from '../frameTree/loader';
import type { FrameTree, FrameTreePageRequest } from '../frameTree/types';

export interface FrameTreeContext {
    datasetKey: string;
    deviceId: string;
    eventId: number;
}
const Panel = styled.div`
    padding: 0 12px 12px;
    color: ${(p): string => p.theme.textColorPrimary};
    .frame-tree-summary, .frame-tree-context { display: flex; gap: 20px; flex-wrap: wrap; align-items: center; margin: 12px 0; }
    .frame-tree-summary { padding: 12px; background: ${(p): string => p.theme.bgColorLight}; font-variant-numeric: tabular-nums; }
    .frame-tree-summary strong { font-size: 18px; margin-left: 8px; }
    .frame-tree-warning { color: ${(p): string => p.theme.textColorSecondary}; padding: 8px; border-left: 3px solid #d6a319; }
`;
interface LoadState {
    key: string;
    tree?: FrameTree;
    error?: string;
    received: number;
    total: number;
}
export function FrameTreePanel({ context, resolving = false, selectionError, request = requestFrameTreePage }: {
    context: FrameTreeContext | null;
    resolving?: boolean;
    selectionError?: string;
    request?: FrameTreePageRequest;
}): JSX.Element {
    const { t } = useTranslation('leaks');
    const [includeUnknown, setIncludeUnknown] = useState(true);
    const [retry, setRetry] = useState(0);
    const [state, setState] = useState<LoadState>({ key: '', received: 0, total: 0 });
    const key = JSON.stringify([context?.datasetKey, context?.deviceId, context?.eventId, includeUnknown, retry]);
    useEffect(() => {
        if (!context || resolving) return;
        const controller = new AbortController();
        setState({ key, received: 0, total: 0 });
        void loadFrameTree({
            deviceId: context.deviceId, eventId: context.eventId, includeStatic: includeUnknown, pageSize: 1000,
        }, request, controller.signal, (received, total) => {
            if (!controller.signal.aborted) setState({ key, received, total });
        }).then(tree => {
            if (!controller.signal.aborted) setState({ key, tree, received: tree.nodes.size, total: tree.nodes.size });
        }).catch(error => {
            if (!controller.signal.aborted) setState({ key, error: error instanceof Error ? error.message : String(error), received: 0, total: 0 });
        });
        return () => controller.abort();
    }, [key, request, resolving]);
    const current = state.key === key && !resolving ? state : undefined;
    const sourceTree = current?.tree;
    const tree = useMemo(() => sourceTree ? groupUnknownFrames(sourceTree, includeUnknown) : undefined, [sourceTree, includeUnknown]);
    const root = tree?.nodes.get(tree.rootId);
    const empty = root && tree?.nodes.size === 1 && root.metrics.sizeBytes === 0n && root.metrics.blockCount === 0n;
    const missing = includeUnknown && sourceTree && [...sourceTree.nodes.values()].some(node => node.dto.kind === 'missingFrame');
    return <CollapsiblePanel title={t('frameTree.title')} testId="frameTreePanel" collapsible destroy={false}>
        <Panel>
            <div className="frame-tree-context" data-testid="frame-tree-context">
                {context ? <span>{t('frameTree.device')} {context.deviceId} · {t('frameTree.event')} {context.eventId}</span> : null}
                <Checkbox checked={includeUnknown} onChange={(event): void => setIncludeUnknown(event.target.checked)}>{t('frameTree.includeUnknown')}</Checkbox>
            </div>
            {resolving
                ? <p role="status">{t('frameTree.resolving')}</p>
                : selectionError !== undefined
                    ? <p role="alert">{selectionError.length > 0 ? selectionError : t('frameTree.failed')}</p>
                    : !context
                        ? <p>{t('frameTree.idle')}</p>
                        : current?.error !== undefined
                            ? <div role="alert"><p>{t('frameTree.failed')}: {current.error}</p>
                                <Button onClick={(): void => setRetry(value => value + 1)}>{t('frameTree.retry')}</Button></div>
                            : !root || !tree
                                ? <div role="status" aria-busy="true">
                                    <p>{t('frameTree.loading')} {current?.received ?? 0} / {current?.total ? current.total : '\u2014'}</p>
                                    <p>{t('frameTree.searchAfterLoad')}</p>
                                </div>
                                : <>
                                    <div className="frame-tree-summary">
                                        <span>{t('frameTree.size')}<strong title={`${root.dto.sizeBytes} B`}>{formatExactBytes(root.metrics.sizeBytes)}</strong></span>
                                        <span>{t('frameTree.requested')}<strong title={`${root.dto.requestedBytes} B`}>{formatExactBytes(root.metrics.requestedBytes)}</strong></span>
                                        <span>{t('frameTree.blocks')}<strong>{root.dto.blockCount}</strong></span>
                                        <span>{t('frameTree.nodes')}<strong>{tree.nodes.size}</strong></span>
                                    </div>
                                    {missing ? <p className="frame-tree-warning">{t('frameTree.missingWarning')}</p> : null}
                                    {empty ? <p role="status">{t('frameTree.empty')}</p> : <FrameTreeExplorer key={key} tree={tree} />}
                                </>}
        </Panel>
    </CollapsiblePanel>;
}
