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
import type { PiecewiseVisualMapOption } from 'echarts/types/dist/shared';
import type { TFunction } from 'i18next';
import { COLOR } from '../../Common';
import { MatrixType, MatrixTypeValues } from './Filter';
import { baseSeries, getTooltip, type HeatmapData, HeatmapDataIndex } from '../CommunicationMatrix';

enum CompareRes {
    SAME = 0,
    DIFFERENT = 1,
}

export const allTransporType = ['HCCS', 'PCIE', 'RDMA', 'LOCAL', 'SIO', 'UB'];
export const getTransporTypeNumber = (item: HeatmapData, matrixType: MatrixTypeValues, isCompare: boolean): number => {
    if (isCompare) {
        const compareData = item[HeatmapDataIndex.DATA];
        return compareData.compare[matrixType][0] === compareData.baseline[matrixType][0] ? CompareRes.SAME : CompareRes.DIFFERENT;
    }
    const type = item[HeatmapDataIndex.VALUE] as string;
    return allTransporType.indexOf(type);
};

export const transportTypeVisualMap: PiecewiseVisualMapOption = {
    type: 'piecewise',
    orient: 'horizontal',
    left: 'center',
    bottom: '0',
    splitNumber: 1,
    textStyle: { color: COLOR.GREY_40 },
    dimension: 2,
};

export const getTransportTypeVisualMap = (isCompare: boolean, t: TFunction): PiecewiseVisualMapOption => {
    return {
        ...transportTypeVisualMap,
        pieces: isCompare
            ? [
                { value: 0, label: t('Same'), color: COLOR.BAND_4 },
                { value: 1, label: t('Different'), color: COLOR.BAND_0 },
            ]
            : [
                { value: 0, label: allTransporType[0], color: COLOR.BAND_0 },
                { value: 1, label: allTransporType[1], color: COLOR.BAND_1 },
                { value: 2, label: allTransporType[2], color: COLOR.BAND_2 },
                { value: 3, label: allTransporType[3], color: COLOR.BAND_3 },
                { value: 4, label: allTransporType[4], color: COLOR.BAND_4 },
                { value: 5, label: allTransporType[5], color: COLOR.BAND_5 },
            ],
    };
};

export const getTransportTypeSerie = ({ data, rankIds, type, isCompare, t }: {
    data: HeatmapData[];
    rankIds: number[];
    type: MatrixType;
    isCompare: boolean;
    t: TFunction;
}): any => {
    const numData = data.map(item => [...item.slice(0, HeatmapDataIndex.VALUE),
        getTransporTypeNumber(item, type, isCompare),
        ...item.slice(HeatmapDataIndex.VALUE + 1)]);
    return {
        ...baseSeries,
        data: numData,
        label: {
            show: rankIds.length <= 16,
            formatter: (params: any): string => {
                const num = params.value[HeatmapDataIndex.VALUE] as number;
                if (isCompare) {
                    const compareData = params.value[HeatmapDataIndex.DATA];
                    return num === CompareRes.DIFFERENT ? `${compareData.baseline[type][0]}->${compareData.compare[type][0]}` : `${compareData.compare[type][0]}`;
                }
                return allTransporType[num] ?? '';
            },
        },
        tooltip: getTooltip({ t, type, isCompare }),
    };
};
