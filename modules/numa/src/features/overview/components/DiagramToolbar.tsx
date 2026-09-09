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

import React from 'react';
import type { TFunction } from 'i18next';
import { DEFAULT_ZOOM, ZOOM_STEP } from '../model/NumaStore';

interface DiagramToolbarProps {
    zoom: number;
    showConnectionMetrics: boolean;
    onChangeZoom: (zoom: number) => void;
    onToggleConnectionMetrics: () => void;
    t: TFunction<'numa'>;
}

export const DiagramToolbar: React.FC<DiagramToolbarProps> = ({
    zoom,
    showConnectionMetrics,
    onChangeZoom,
    onToggleConnectionMetrics,
    t,
}) => (
    <div className="diagram-toolbar">
        <span>{t('zoom.gesture')}</span>
        <button
            type="button"
            className={`connection-metrics-toggle ${showConnectionMetrics ? 'active' : ''}`}
            title={t('showConnectionMetrics')}
            aria-label={t('showConnectionMetrics')}
            aria-pressed={showConnectionMetrics}
            onClick={onToggleConnectionMetrics}
        >
            <span className="toggle-track" aria-hidden="true"><span /></span>
            <span>{t('showConnectionMetrics')}</span>
        </button>
        <button
            type="button"
            title={t('zoom.out')}
            onClick={() => onChangeZoom(zoom - ZOOM_STEP)}
        >-</button>
        <button
            type="button"
            className="zoom-value"
            title={t('zoom.reset')}
            onClick={() => onChangeZoom(DEFAULT_ZOOM)}
        >
            {Math.round(zoom * 100)}%
        </button>
        <button
            type="button"
            title={t('zoom.in')}
            onClick={() => onChangeZoom(zoom + ZOOM_STEP)}
        >+</button>
    </div>
);
