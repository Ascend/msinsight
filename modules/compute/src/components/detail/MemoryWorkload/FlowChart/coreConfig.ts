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

import type { Inode } from './flowType';

export const cubeCoreBaseA5: Inode = {
    name: '',
    left: -25,
    top: 20,
    container: [
        {
            x: 125,
            y: -20,
            width: 850,
            height: 330,
        },
    ],
    rect: [
        {
            name: 'L1',
            x: 140,
            y: 25,
            width: 170,
            height: 170,
            label: 'L1',
        },
        {
            left: 100,
            top: 20,
            width: 100,
            height: 50,
            label: 'L0A',
        },
        {
            top: 90,
            left: -100,
            width: 100,
            height: 50,
            label: 'L0B',
        },
        {
            top: -80,
            left: 100,
            width: 150,
            height: 120,
            labels: [
                { value: 'Cube' },
                { value: 'Ratio:', x: 665, y: 135 },
                { id: 'cubeRatio', value: '0%', x: 695, y: 135, position: 'right' },
                { id: 'cubeRatioBaseline', value: '' },
            ],
        },
        {
            top: -30,
            left: 100,
            width: 80,
            height: 180,
            label: 'L0C',
        },
        {
            top: 230,
            left: -800,
            width: 800,
            height: 40,
            label: 'FIXP',
        },
    ],
    line: [
        {
            id: 'L2_TO_L1',
            label: 'L2_TO_L1',
            x: 0,
            y: 75,
            orient: 'right',
            length: 138,
        },
        {
            id: 'L0A_TO_CUBE',
            label: 'L0A_TO_CUBE',
            x: 510,
            y: 70,
            orient: 'right',
            length: 97,
            labelPosition: 'bottom',
        },
        {
            id: 'L0B_TO_CUBE',
            label: 'L0B_TO_CUBE',
            x: 510,
            y: 160,
            orient: 'right',
            length: 97,
        },
        {
            id: 'CUBE_TO_L0C',
            label: 'CUBE_TO_L0C',
            x: 760,
            y: 120,
            length: 97,
            orient: 'right',
        },
        {
            id: 'L0C_TO_FIXP',
            label: 'L0C_TO_FIXP',
            x: 900,
            y: 205,
            length: 47,
            orient: 'bottom',
            labelPosition: 'left',
        },
        {
            id: 'FIXP_TO_L1',
            label: 'FIXP_TO_L1',
            x: 210,
            y: 255,
            length: 57,
            orient: 'top',
            labelPosition: 'bottom',
        },
        {
            id: 'FIXP_TO_L2',
            label: 'FIXP_TO_L2',
            x: 140,
            y: 275,
            orient: 'left',
            length: 138,
        },
    ],
};

// UB0
export const vectorCoreModule1A5Common: Inode = {
    name: '',
    left: -15,
    top: 0,
    container: [
        {
            left: 125,
            top: 0,
            width: 850,
            height: 160,
        },
    ],
    rect: [
        {
            x: 140,
            y: 20,
            width: 320,
            height: 50,
            label: 'UB',
            name: 'UB',
        },
        {
            top: 0,
            left: 120,
            width: 320,
            height: 125,
            name: 'Vector',
            labels: [
                { value: 'Vector' },
                { value: 'Ratio:', x: 720, y: 95 },
                { id: 'vectorRatio', value: '0%', x: 750, y: 95, position: 'right' },
                { id: 'vectorRatioBaseline', value: '' },
            ],
        },
        {
            top: 70,
            left: -760,
            width: 320,
            height: 50,
            label: 'Dcache(SIMT)',
            name: 'Dcache(SIMT)',
        },
    ],
    line: [
        // L1 -> UB0,44
        {
            id: 'L1_TO_UB',
            label: 'L1_TO_UB',
            x: 250,
            top: '46%-205',
            length: 145,
            orient: 'bottom',
            labelPosition: 'bottom',
        },
        //  FIXP -> UB0,42
        {
            id: 'FIXP_TO_UB',
            label: 'FIXP_TO_UB',
            x: 350,
            top: '46%-105',
            length: 45,
            orient: 'bottom',
        },
        // L2_TO_UB0 -> 12
        {
            id: 'L2_TO_UB',
            label: 'L2_TO_UB0',
            x: 0,
            top: '46%-40',
            length: 138,
            orient: 'right',
        },
        // L2_TO_Dcache 48
        {
            id: 'L2_TO_Dcache',
            label: 'L2_TO_Dcache',
            x: 0,
            top: '46%+35',
            length: 138,
            orient: 'right',
        },
        // UB_TO_VEC 14
        {
            id: 'UB_TO_VEC',
            label: 'UB_TO_VEC',
            x: 460,
            top: '46%-40',
            length: 118,
            orient: 'right',
        },
        // VEC_TO_UB 15
        {
            id: 'VEC_TO_UB',
            label: 'VEC_TO_UB',
            x: 580,
            top: '46%-20',
            length: 118,
            orient: 'left',
            labelPosition: 'bottom',
        },
        //  Dcache_TO_VEC 54
        {
            id: 'Dcache_TO_VEC',
            label: 'DCACHE_TO_VEC',
            x: 460,
            top: '46%+35',
            length: 118,
            orient: 'right',
        },
        //  VEC_TO_Dcache 52
        {
            id: 'VEC_TO_Dcache',
            label: 'VEC_TO_DCACHE',
            x: 580,
            top: '46%+55',
            length: 118,
            orient: 'left',
            labelPosition: 'bottom',
        },
    ],
};

export const vectorCoreModule1A5: Inode = {
    ...vectorCoreModule1A5Common,
    line: [
        ...(vectorCoreModule1A5Common.line ?? []),
        // UB0 -> L1,46
        {
            id: 'UB_TO_L1',
            label: 'UB_TO_L1',
            x: 150,
            y: 20,
            top: '46%-60',
            length: 104,
            orient: 'top',
            labelXy: {
                x: 166,
                y: -100,
            },
        },
        // UB_TO_L1 到 UB_TO_L1_2
        {
            id: 'UB_TO_L1__UB_TO_L1_2',
            label: 'UB_TO_L1__UB_TO_L1_2',
            x: 150,
            y: -84,
            top: '46%-60',
            length: 30,
            orient: 'right',
        },
        //  UB0_TO_L2 -> 13
        {
            id: 'UB_TO_L2',
            label: 'UB0_TO_L2',
            x: 126,
            top: '46%-2',
            length: 124,
            orient: 'left',
            labelPosition: 'top',
        },
    ],
};

export const vectorCoreModule1A5V2: Inode = {
    ...vectorCoreModule1A5Common,
    line: [
        ...(vectorCoreModule1A5Common.line ?? []),
        // UB0 -> L1,46
        {
            id: 'UB_TO_L1',
            label: 'UB_TO_L1',
            x: 150,
            y: 20,
            top: '46%-60',
            length: 146,
            orient: 'top',
            labelXy: {
                x: 150,
                y: -82,
            },
        },
        //  UB0_TO_L2 -> 13
        {
            id: 'UB_TO_L2',
            label: 'UB0_TO_L2',
            x: 140,
            top: '46%-20',
            length: 138,
            orient: 'left',
            labelPosition: 'bottom',
        },
        // Dcache_TO_L2 50
        {
            id: 'Dcache_TO_L2',
            label: 'Dcache_TO_L2',
            x: 140,
            top: '46%+55',
            length: 138,
            orient: 'left',
            labelPosition: 'bottom',
        },
    ],
};

// UB1
export const vectorCoreModule2A5Common: Inode = {
    name: '',
    top: 0,
    left: -15,
    container: [
        {
            left: 125,
            top: 0,
            width: 850,
            height: 160,
        },
    ],
    rect: [
        {
            x: 140,
            y: 20,
            width: 320,
            height: 50,
            label: 'UB',
            name: 'UB',
        },
        {
            top: 0,
            left: 120,
            width: 320,
            height: 125,
            name: 'Vector',
            labels: [
                { value: 'Vector' },
                { value: 'Ratio:', x: 720, y: 95 },
                { id: 'vectorRatio', value: '0%', x: 750, y: 95, position: 'right' },
                { id: 'vectorRatioBaseline', value: '' },
            ],
        },
        {
            top: 70,
            left: -760,
            width: 320,
            height: 50,
            label: 'Dcache(SIMT)',
            name: 'Dcache(SIMT)',
        },
    ],
    line: [
        // UB0 -> L1
        {
            id: 'UB_TO_L1_2',
            label: 'UB_TO_L1_2',
            x: 180,
            y: 20,
            top: '46%-55',
            length: 311,
            orient: 'top',
        },
        // FIXP -> UB1
        {
            id: 'FIXP_TO_UB_2',
            label: 'FIXP_TO_UB_2',
            x: 400,
            top: '46%-270',
            length: 210,
            orient: 'bottom',
        },
        // L2_TO_UB_2 16
        {
            id: 'L2_TO_UB_2',
            label: 'L2_TO_UB_2',
            x: 0,
            top: '46%-40',
            length: 138,
            orient: 'right',
        },
        // L2_TO_Dcache_2 49
        {
            id: 'L2_TO_Dcache_2',
            label: 'L2_TO_Dcache_2',
            x: 0,
            top: '46%+35',
            length: 138,
            orient: 'right',
        },
        // UB_TO_VEC_2 18
        {
            id: 'UB_TO_VEC_2',
            label: 'UB_TO_VEC_2',
            x: 460,
            top: '46%-40',
            length: 118,
            orient: 'right',
        },
        // VEC_TO_UB_2 19
        {
            id: 'VEC_TO_UB_2',
            label: 'VEC_TO_UB_2',
            x: 580,
            top: '46%-20',
            length: 118,
            orient: 'left',
            labelPosition: 'bottom',
        },
        // Dcache_TO_VEC_2 55
        {
            id: 'Dcache_TO_VEC_2',
            label: 'Dcache_TO_VEC_2',
            x: 460,
            top: '46%+35',
            length: 118,
            orient: 'right',
        },
        // VEC_TO_Dcache_2 53
        {
            id: 'VEC_TO_Dcache_2',
            label: 'VEC_TO_Dcache_2',
            x: 580,
            top: '46%+55',
            length: 118,
            orient: 'left',
            labelPosition: 'bottom',
        },
    ],
};

export const vectorCoreModule2A5: Inode = {
    ...vectorCoreModule2A5Common,
    line: [
        ...(vectorCoreModule2A5Common.line ?? []),
        // UB_TO_L2_2 17
        {
            id: 'UB_TO_L2_2',
            label: 'UB_TO_L2_2',
            x: 126,

            top: '46%-2',
            length: 124,
            orient: 'left',
            labelPosition: 'top',
        },
        // UB1 -> UB2
        {
            id: 'L1_TO_UB_UB',
            label: 'L1_TO_UB_UB',
            x: 250,
            top: '46%-327',
            length: 40,
            orient: 'right',
        },
        // L1 -> UB1
        {
            id: 'L1_TO_UB_2',
            label: 'L1_TO_UB_2',
            x: 290,
            top: '46%-327',
            length: 267,
            orient: 'bottom',
        },
    ],
};

export const vectorCoreModule2A5V2: Inode = {
    ...vectorCoreModule2A5Common,
    line: [
        ...(vectorCoreModule2A5Common.line ?? []),
        // UB_TO_L2_2 17
        {
            id: 'UB_TO_L2_2',
            label: 'UB_TO_L2_2',
            x: 140,

            top: '46%-20',
            length: 138,
            orient: 'left',
            labelPosition: 'bottom',
        },
        // Dcache_TO_L2_2 51
        {
            id: 'Dcache_TO_L2_2',
            label: 'Dcache_TO_L2_2',
            x: 140,
            top: '46%+55',
            length: 138,
            orient: 'left',
            labelPosition: 'bottom',
        },
        // L1 -> UB1
        {
            id: 'L1_TO_UB_2',
            label: 'L1_TO_UB_2',
            x: 290,
            top: '46%-370',
            length: 310,
            orient: 'bottom',
        },
    ],
};
