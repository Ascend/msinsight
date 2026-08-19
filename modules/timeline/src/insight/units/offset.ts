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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import type { CardMetaData } from '../../entity/data';
import type { InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';

export const OFFSET_SIDE = {
    HOST: 'host',
    DEVICE: 'device',
} as const;

export type OffsetSide = typeof OFFSET_SIDE[keyof typeof OFFSET_SIDE];

export interface OffsetTarget {
    cardId: string;
    side: OffsetSide;
}

export type CardIdIndex = Map<string, string>;

const DEVICE_META_TYPES = new Set([
    'Ascend Hardware',
    'HCCL',
    'OVERLAP_ANALYSIS',
    'NPU_METRICS',
    'CCU',
]);

function containCardId(unit: InsightUnit, cardId: string): boolean {
    if ((unit.metadata as CardMetaData)?.cardId === cardId) {
        return true;
    }
    return unit.children?.some(childUnit => containCardId(childUnit, cardId)) ?? false;
}

export function buildCardIdIndex(units: InsightUnit[]): CardIdIndex {
    const cardIdIndex: CardIdIndex = new Map();
    const visit = (unit: InsightUnit, rootCardId: string): void => {
        const cardId = (unit.metadata as CardMetaData)?.cardId;
        if (cardId !== undefined && !cardIdIndex.has(cardId)) {
            cardIdIndex.set(cardId, rootCardId);
        }
        unit.children?.forEach(child => visit(child, rootCardId));
    };
    units.forEach(unit => {
        const rootCardId = (unit.metadata as CardMetaData)?.cardId;
        if (rootCardId !== undefined) {
            visit(unit, rootCardId);
        }
    });
    return cardIdIndex;
}

export function getOffsetSide(metaType?: string): OffsetSide {
    return DEVICE_META_TYPES.has(metaType ?? '') ? OFFSET_SIDE.DEVICE : OFFSET_SIDE.HOST;
}

export function getCardOffsetKey(
    session: Session,
    target: OffsetTarget,
    units: InsightUnit[] = [],
    cardIdIndex?: CardIdIndex,
): string {
    const targetUnits = units.length === 0 ? session.units : units;
    const unit = cardIdIndex === undefined
        ? targetUnits.find(value => containCardId(value, target.cardId))
        : undefined;
    const realCardId = cardIdIndex?.get(target.cardId) ?? (unit ? (unit.metadata as CardMetaData).cardId : 'Host');
    return `${realCardId}__${target.side}`;
}

export function getCardSideOffset(session: Session, cardId: string, side: OffsetSide): number {
    const key = getCardOffsetKey(session, { cardId, side });
    return session.unitsConfig.offsetConfig.timestampOffset[key] ?? 0;
}

export function getTimeOffset(
    session: Session,
    metadata: { cardId?: string; processId?: string; metaType?: string },
    units: InsightUnit[] = [],
    timestampOffset?: Record<string, number>,
    cardIdIndex?: CardIdIndex,
): number {
    if (metadata.cardId === undefined) {
        return 0;
    }
    const key = getCardOffsetKey(session, {
        cardId: metadata.cardId,
        side: getOffsetSide(metadata.metaType),
    }, units, cardIdIndex);
    return (timestampOffset ?? session.unitsConfig.offsetConfig.timestampOffset)[key] ?? 0;
}

export function initializeCardOffsets(
    current: Record<string, number>,
    cardId: string,
    defaultOffset: number,
): Record<string, number> {
    const next = { ...current };
    const hostKey = `${cardId}__${OFFSET_SIDE.HOST}`;
    const deviceKey = `${cardId}__${OFFSET_SIDE.DEVICE}`;
    if (!Object.prototype.hasOwnProperty.call(next, hostKey)) {
        next[hostKey] = defaultOffset;
    }
    if (!Object.prototype.hasOwnProperty.call(next, deviceKey)) {
        next[deviceKey] = defaultOffset;
    }
    return next;
}
