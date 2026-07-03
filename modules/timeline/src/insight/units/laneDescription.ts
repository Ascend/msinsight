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
import type { InsightUnit } from '../../entity/insight';

interface LaneDescriptionRule {
    keywords: string[];
    descriptionKey: string;
    patterns?: RegExp[];
}

// NPU Metrics 泳道说明来自 docs/zh/user_guide/system_tuning.md 表 2。
// 带 patterns 的规则用于匹配三级泳道，需要放在二级泳道兜底规则之前。
const NPU_METRICS_LANE_DESCRIPTION_RULES: LaneDescriptionRule[] = [
    {
        keywords: ['Biu Perf'],
        patterns: [/^Group\s*\d+[-_]?aiv\d+/i],
        descriptionKey: 'laneDescriptions.npuMetrics.biuPerfGroupAiv',
    },
    {
        keywords: ['UB'],
        patterns: [/^UDMA-?Ports?\d+/i],
        descriptionKey: 'laneDescriptions.npuMetrics.ubUdmaPorts',
    },
    {
        keywords: ['UB'],
        patterns: [/^UNIC-?Ports?\d+/i],
        descriptionKey: 'laneDescriptions.npuMetrics.ubUnicPorts',
    },
    {
        keywords: ['Block Detail'],
        patterns: [/^AIC\s+Earliest/i],
        descriptionKey: 'laneDescriptions.npuMetrics.blockDetailAicEarliest',
    },
    {
        keywords: ['Block Detail'],
        patterns: [/^AIC\s+Latest/i],
        descriptionKey: 'laneDescriptions.npuMetrics.blockDetailAicLatest',
    },
    {
        keywords: ['Block Detail'],
        patterns: [/^AIV\s+Earliest/i],
        descriptionKey: 'laneDescriptions.npuMetrics.blockDetailAivEarliest',
    },
    {
        keywords: ['Block Detail'],
        patterns: [/^AIV\s+Latest/i],
        descriptionKey: 'laneDescriptions.npuMetrics.blockDetailAivLatest',
    },
    {
        keywords: ['HBM'],
        patterns: [/^HBM\s+\d+\s+read\/Bandwidth$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.hbmRead',
    },
    {
        keywords: ['HBM'],
        patterns: [/^HBM\s+\d+\s+write\/Bandwidth$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.hbmWrite',
    },
    {
        keywords: ['DDR'],
        patterns: [/^Read/i],
        descriptionKey: 'laneDescriptions.npuMetrics.ddrRead',
    },
    {
        keywords: ['DDR'],
        patterns: [/^Write/i],
        descriptionKey: 'laneDescriptions.npuMetrics.ddrWrite',
    },
    {
        keywords: ['LLC'],
        patterns: [/^LLC\s+\d+\s+read\/Hit Rate$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.llcReadHitRate',
    },
    {
        keywords: ['LLC'],
        patterns: [/^LLC\s+\d+\s+write\/Hit Rate$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.llcWriteHitRate',
    },
    {
        keywords: ['LLC'],
        patterns: [/^LLC\s+\d+\s+read\/Throughput$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.llcReadThroughput',
    },
    {
        keywords: ['LLC'],
        patterns: [/^LLC\s+\d+\s+write\/Throughput$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.llcWriteThroughput',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        patterns: [/^APP\/DDR/i],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMemAppDdr',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        patterns: [/^APP\/HBM/i],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMemAppHbm',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        patterns: [/^APP\/MEMORY/i],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMemAppMemory',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        patterns: [/^Device\/DDR/i],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMemDeviceDdr',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        patterns: [/^Device\/HBM/i],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMemDeviceHbm',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        patterns: [/^Device\/MEMORY/i],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMemDeviceMemory',
    },
    {
        keywords: ['Stars Soc', 'Stars Soc Info'],
        patterns: [/^L2 Buffer Bw Level$/i, /(^|\/)l2BufferBwLevel$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.starsSocL2BufferBwLevel',
    },
    {
        keywords: ['Stars Soc', 'Stars Soc Info'],
        patterns: [/^M[ae]ta Bw Level$/i, /(^|\/)mataBwLevel$/i],
        descriptionKey: 'laneDescriptions.npuMetrics.starsSocMetaBwLevel',
    },
    {
        keywords: ['acc_pmu'],
        patterns: [/^Accelerator\s+\d+\/readBwLevel/i],
        descriptionKey: 'laneDescriptions.npuMetrics.accPmuReadBwLevel',
    },
    {
        keywords: ['acc_pmu'],
        patterns: [/^Accelerator\s+\d+\/readOstLevel/i],
        descriptionKey: 'laneDescriptions.npuMetrics.accPmuReadOstLevel',
    },
    {
        keywords: ['acc_pmu'],
        patterns: [/^Accelerator\s+\d+\/writeBwLevel/i],
        descriptionKey: 'laneDescriptions.npuMetrics.accPmuWriteBwLevel',
    },
    {
        keywords: ['acc_pmu'],
        patterns: [/^Accelerator\s+\d+\/writeOstLevel/i],
        descriptionKey: 'laneDescriptions.npuMetrics.accPmuWriteOstLevel',
    },
    {
        keywords: ['AI Core Utilization'],
        patterns: [/^Average/i],
        descriptionKey: 'laneDescriptions.npuMetrics.aiCoreUtilizationAverage',
    },
    {
        keywords: ['AI Core Utilization'],
        patterns: [/^Core\s+\d+/i],
        descriptionKey: 'laneDescriptions.npuMetrics.aiCoreUtilizationCore',
    },
    {
        keywords: ['SIO'],
        patterns: [/^dat_rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioDatRx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^dat_tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioDatTx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^req_rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioReqRx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^req_tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioReqTx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^rsp_rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioRspRx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^rsp_tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioRspTx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^snp_rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioSnpRx',
    },
    {
        keywords: ['SIO'],
        patterns: [/^snp_tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.sioSnpTx',
    },
    {
        keywords: ['QoS', 'QOS'],
        patterns: [/^QoS\s+\d+:OTHERS/i],
        descriptionKey: 'laneDescriptions.npuMetrics.qosOthers',
    },
    {
        keywords: ['NIC'],
        patterns: [/^Port\s+\d+\/Rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.nicPortRx',
    },
    {
        keywords: ['NIC'],
        patterns: [/^Port\s+\d+\/Tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.nicPortTx',
    },
    {
        keywords: ['RoCE'],
        patterns: [/^Port\s+\d+\/Rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.rocePortRx',
    },
    {
        keywords: ['RoCE'],
        patterns: [/^Port\s+\d+\/Tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.rocePortTx',
    },
    {
        keywords: ['PCIe', 'PCIE'],
        patterns: [/^PCIe?_cpl/i, /^PCIe?\/.+cpl/i],
        descriptionKey: 'laneDescriptions.npuMetrics.pcieCpl',
    },
    {
        keywords: ['PCIe', 'PCIE'],
        patterns: [/^PCIe?_nonpost(?!_latency)/i, /^PCIe?\/.+nonpost(?!.*latency)/i],
        descriptionKey: 'laneDescriptions.npuMetrics.pcieNonpost',
    },
    {
        keywords: ['PCIe', 'PCIE'],
        patterns: [/^PCIe?_nonpost_latency/i, /^PCIe?\/.+nonpost.*latency/i],
        descriptionKey: 'laneDescriptions.npuMetrics.pcieNonpostLatency',
    },
    {
        keywords: ['PCIe', 'PCIE'],
        patterns: [/^PCIe?_post/i, /^PCIe?\/.+post/i],
        descriptionKey: 'laneDescriptions.npuMetrics.pciePost',
    },
    {
        keywords: ['HCCS'],
        patterns: [/^(HCCS\/)?txThroughput/i],
        descriptionKey: 'laneDescriptions.npuMetrics.hccsTxThroughput',
    },
    {
        keywords: ['HCCS'],
        patterns: [/^(HCCS\/)?rxThroughput/i],
        descriptionKey: 'laneDescriptions.npuMetrics.hccsRxThroughput',
    },
    {
        keywords: ['biu_group'],
        patterns: [/^Bandwidth\s+Read/i],
        descriptionKey: 'laneDescriptions.npuMetrics.biuGroupBandwidthRead',
    },
    {
        keywords: ['biu_group'],
        patterns: [/^Bandwidth\s+Write/i],
        descriptionKey: 'laneDescriptions.npuMetrics.biuGroupBandwidthWrite',
    },
    {
        keywords: ['biu_group'],
        patterns: [/^Latency\s+Read/i],
        descriptionKey: 'laneDescriptions.npuMetrics.biuGroupLatencyRead',
    },
    {
        keywords: ['biu_group'],
        patterns: [/^Latency\s+Write/i],
        descriptionKey: 'laneDescriptions.npuMetrics.biuGroupLatencyWrite',
    },
    {
        keywords: ['aic_core_group'],
        patterns: [/^Cube/i],
        descriptionKey: 'laneDescriptions.npuMetrics.aicCoreGroupCube',
    },
    {
        keywords: ['aic_core_group'],
        patterns: [/^Mte1/i],
        descriptionKey: 'laneDescriptions.npuMetrics.coreGroupMte1',
    },
    {
        keywords: ['aic_core_group'],
        patterns: [/^Mte2/i],
        descriptionKey: 'laneDescriptions.npuMetrics.coreGroupMte2',
    },
    {
        keywords: ['aic_core_group'],
        patterns: [/^Mte3/i],
        descriptionKey: 'laneDescriptions.npuMetrics.coreGroupMte3',
    },
    {
        keywords: ['aiv_core_group'],
        patterns: [/^Mte1/i],
        descriptionKey: 'laneDescriptions.npuMetrics.coreGroupMte1',
    },
    {
        keywords: ['aiv_core_group'],
        patterns: [/^Mte2/i],
        descriptionKey: 'laneDescriptions.npuMetrics.coreGroupMte2',
    },
    {
        keywords: ['aiv_core_group'],
        patterns: [/^Mte3/i],
        descriptionKey: 'laneDescriptions.npuMetrics.coreGroupMte3',
    },
    {
        keywords: ['aiv_core_group'],
        patterns: [/^Scalar/i],
        descriptionKey: 'laneDescriptions.npuMetrics.aivCoreGroupScalar',
    },
    {
        keywords: ['aiv_core_group'],
        patterns: [/^Vector/i],
        descriptionKey: 'laneDescriptions.npuMetrics.aivCoreGroupVector',
    },
    {
        keywords: ['Stars Chip Trans'],
        patterns: [/^PA Link Rx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.starsChipTransPaLinkRx',
    },
    {
        keywords: ['Stars Chip Trans'],
        patterns: [/^PA Link Tx/i],
        descriptionKey: 'laneDescriptions.npuMetrics.starsChipTransPaLinkTx',
    },
    {
        keywords: ['Stars Chip Trans'],
        patterns: [/^PCIE Read Bandwidth/i],
        descriptionKey: 'laneDescriptions.npuMetrics.starsChipTransPcieReadBandwidth',
    },
    {
        keywords: ['Stars Chip Trans'],
        patterns: [/^PCIE Write Bandwidth/i],
        descriptionKey: 'laneDescriptions.npuMetrics.starsChipTransPcieWriteBandwidth',
    },
    {
        keywords: ['Low Power'],
        descriptionKey: 'laneDescriptions.npuMetrics.lowPower',
    },
    {
        keywords: ['Biu Perf'],
        descriptionKey: 'laneDescriptions.npuMetrics.biuPerf',
    },
    {
        keywords: ['UB'],
        descriptionKey: 'laneDescriptions.npuMetrics.ub',
    },
    {
        keywords: ['Block Detail'],
        descriptionKey: 'laneDescriptions.npuMetrics.blockDetail',
    },
    {
        keywords: ['HBM'],
        descriptionKey: 'laneDescriptions.npuMetrics.hbm',
    },
    {
        keywords: ['DDR'],
        descriptionKey: 'laneDescriptions.npuMetrics.ddr',
    },
    {
        keywords: ['LLC'],
        descriptionKey: 'laneDescriptions.npuMetrics.llc',
    },
    {
        keywords: ['NPU_MEM', 'NPU MEM'],
        descriptionKey: 'laneDescriptions.npuMetrics.npuMem',
    },
    {
        keywords: ['Stars Soc', 'Stars Soc Info'],
        descriptionKey: 'laneDescriptions.npuMetrics.starsSoc',
    },
    {
        keywords: ['acc_pmu'],
        descriptionKey: 'laneDescriptions.npuMetrics.accPmu',
    },
    {
        keywords: ['AI Core Utilization'],
        descriptionKey: 'laneDescriptions.npuMetrics.aiCoreUtilization',
    },
    {
        keywords: ['AI Core Freq'],
        descriptionKey: 'laneDescriptions.npuMetrics.aiCoreFreq',
    },
    {
        keywords: ['SIO'],
        descriptionKey: 'laneDescriptions.npuMetrics.sio',
    },
    {
        keywords: ['QoS', 'QOS'],
        descriptionKey: 'laneDescriptions.npuMetrics.qos',
    },
    {
        keywords: ['NIC'],
        descriptionKey: 'laneDescriptions.npuMetrics.nic',
    },
    {
        keywords: ['RoCE'],
        descriptionKey: 'laneDescriptions.npuMetrics.roce',
    },
    {
        keywords: ['PCIe', 'PCIE'],
        descriptionKey: 'laneDescriptions.npuMetrics.pcie',
    },
    {
        keywords: ['HCCS'],
        descriptionKey: 'laneDescriptions.npuMetrics.hccs',
    },
    {
        keywords: ['biu_group'],
        descriptionKey: 'laneDescriptions.npuMetrics.biuGroup',
    },
    {
        keywords: ['aic_core_group'],
        descriptionKey: 'laneDescriptions.npuMetrics.aicCoreGroup',
    },
    {
        keywords: ['aiv_core_group'],
        descriptionKey: 'laneDescriptions.npuMetrics.aivCoreGroup',
    },
    {
        keywords: ['Stars Chip Trans'],
        descriptionKey: 'laneDescriptions.npuMetrics.starsChipTrans',
    },
];

const normalizeLaneName = (name: string): string => {
    return name
        .toLowerCase()
        .replace(/_/g, ' ')
        .replace(/-/g, ' ')
        .replace(/\s+/g, ' ')
        .trim();
};

// Counter 展示名可能会追加单位，例如 "HBM 0 read/Bandwidth (Byte/s)"。
// 匹配时优先使用原始泳道名，仅在展示名兜底匹配时去掉末尾单位。
const stripUnitSuffix = (name: string): string => name.replace(/\s+\([^()]+\)\s*$/, '').trim();

const getCandidateNames = (unit: InsightUnit, displayName?: string): string[] => {
    const metadata = unit.metadata as {
        processId?: string;
        processName?: string;
        threadName?: string;
        metaType?: string;
    };
    if (unit.name === 'Thread') {
        // 部分指标泳道可能会被构造成 Thread unit，只允许它们参与三级泳道规则匹配。
        return [metadata.processId, metadata.processName, metadata.threadName, metadata.metaType]
            .filter((candidate): candidate is string => Boolean(candidate))
            .map(stripUnitSuffix);
    }
    if (unit.name === 'Process' || unit.name === 'Label') {
        // NPU Metrics 二级泳道是 Label unit，展示名来自 processName，processId/metaType 可作为别名辅助匹配。
        return [displayName, metadata.processId, metadata.processName, metadata.metaType]
            .filter((candidate): candidate is string => Boolean(candidate))
            .map(stripUnitSuffix);
    }
    if (unit.name === 'Counter') {
        // Counter 子泳道经常把父级泳道名存放在 processId 中，此时 processName 可能为空。
        return [
            metadata.processId,
            metadata.processName,
            metadata.threadName,
            metadata.metaType,
            displayName === undefined ? undefined : stripUnitSuffix(displayName),
        ].filter((candidate): candidate is string => Boolean(candidate));
    }
    return [displayName].filter((candidate): candidate is string => Boolean(candidate)).map(stripUnitSuffix);
};

const isKeywordMatched = (rule: LaneDescriptionRule, candidates: string[]): boolean => {
    const normalizedKeywords = rule.keywords.map(normalizeLaneName);
    // 使用精确关键字匹配，避免普通 Process/Thread 名称因包含指标关键字而误命中。
    return candidates.some(candidate => {
        const normalizedCandidate = normalizeLaneName(candidate);
        return normalizedKeywords.some(keyword => normalizedCandidate === keyword || normalizedCandidate.startsWith(`${keyword}/`));
    });
};

const matchRule = (rule: LaneDescriptionRule, candidates: string[], onlyPatternRule = false): boolean => {
    // Thread unit 不能命中二级兜底规则，否则 RoCE/NIC 等泳道下的普通线程会出现错误说明。
    if (onlyPatternRule && !rule.patterns) {
        return false;
    }
    if (!isKeywordMatched(rule, candidates)) {
        return false;
    }
    if (!rule.patterns) {
        return true;
    }
    return candidates.some(candidate => rule.patterns?.some(pattern => pattern.test(candidate)) ?? false);
};

export const getLaneDescriptionKey = (unit: InsightUnit, displayName?: string): string | undefined => {
    const candidates = getCandidateNames(unit, displayName);
    return NPU_METRICS_LANE_DESCRIPTION_RULES.find(rule => matchRule(rule, candidates, unit.name === 'Thread'))?.descriptionKey;
};
