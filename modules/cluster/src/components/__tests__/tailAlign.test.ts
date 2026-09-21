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
import { shouldShowTailAlignTip } from '../communication/tailAlign';

describe('shouldShowTailAlignTip', () => {
    it('returns true for tail-alignable communication operators', () => {
        expect(shouldShowTailAlignTip('AllReduce')).toBe(true);
        expect(shouldShowTailAlignTip('HcomAllReduce')).toBe(true);
        expect(shouldShowTailAlignTip('all_reduce')).toBe(true);

        expect(shouldShowTailAlignTip('AllGather')).toBe(true);
        expect(shouldShowTailAlignTip('HcomAllGather')).toBe(true);
        expect(shouldShowTailAlignTip('all_gather')).toBe(true);

        expect(shouldShowTailAlignTip('AllToAll')).toBe(true);
        expect(shouldShowTailAlignTip('AlltoAll')).toBe(true);
        expect(shouldShowTailAlignTip('HcomAllToAll')).toBe(true);
        expect(shouldShowTailAlignTip('all_to_all')).toBe(true);
    });

    it('returns false for operators that should not show tail alignment tip', () => {
        expect(shouldShowTailAlignTip('Send')).toBe(false);
        expect(shouldShowTailAlignTip('HcomSend')).toBe(false);
        expect(shouldShowTailAlignTip('Recv')).toBe(false);
        expect(shouldShowTailAlignTip('Receive')).toBe(false);
        expect(shouldShowTailAlignTip('HcomReceive')).toBe(false);
        expect(shouldShowTailAlignTip('Broadcast')).toBe(false);
    });
});
