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
const TAIL_ALIGNABLE_OPERATOR_NAMES = ['allreduce', 'alltoall', 'allgather'];

function normalizeOperatorName(operatorName: string): string {
    return operatorName.toLowerCase().replace(/[^a-z]/g, '');
}

export function shouldShowTailAlignTip(operatorName: string): boolean {
    const normalizedName = normalizeOperatorName(operatorName);
    return TAIL_ALIGNABLE_OPERATOR_NAMES.some(name => normalizedName.includes(name));
}
