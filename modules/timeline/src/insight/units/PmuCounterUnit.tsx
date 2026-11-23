/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { chart, unit, UnitHeight } from '../../entity/insight';
import { PMU_GROUP_NAME, type PmuLaneMetadata } from '../../entity/pmuCounter';
import { isPinned, isSonPinned } from '../../components/ChartContainer/unitPin';
import { fetchPmuCounterData } from '../../components/charts/usePmuCounterData';
import { getPmuLaneName } from '../../components/charts/pmuCounterUtils';

export const PmuGroupUnit = unit<PmuLaneMetadata>({
    name: 'Label',
    height: () => UnitHeight.UPPER,
    pinType: 'copied',
    configBar: () => null,
    renderInfo: (_session, metadata) => metadata.pmu.kind === 'group' ? PMU_GROUP_NAME : `CPU ${metadata.pmu.cpu}`,
});

export const PmuCounterUnit = unit<PmuLaneMetadata>({
    name: 'Counter',
    pinType: 'copied',
    collapsible: false,
    configBar: () => null,
    renderInfo: (_session, metadata, thisUnit) => {
        const name = getPmuLaneName(metadata);
        return isPinned(thisUnit) && !isSonPinned(thisUnit)
            ? `${metadata.cardId} · CPU ${metadata.pmu.cpu} · ${name}`
            : name;
    },
    chart: chart({
        type: 'pmuCounter',
        height: UnitHeight.SUPER_UPPER,
        mapFunc: (session, metadata, thisUnit) => fetchPmuCounterData(session, metadata as PmuLaneMetadata, thisUnit?.counterRenderWidth ?? 0),
        config: {},
    }),
});
