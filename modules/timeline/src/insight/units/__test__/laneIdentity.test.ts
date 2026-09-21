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
import { getLaneIdentity } from '../../../entity/data';

describe('lane identity', () => {
    const metadata = {
        cardId: 'rank0',
        dbPath: 'thread-1.db',
        processId: 'Ascend Hardware',
        metaType: 'Ascend Hardware',
        threadId: '7',
    };

    it('separates equal-named streams from different source databases', () => {
        expect(getLaneIdentity(metadata)).not.toBe(getLaneIdentity({
            ...metadata,
            dbPath: 'thread-2.db',
        }));
    });

    it('is stable for the same physical lane', () => {
        expect(getLaneIdentity(metadata)).toBe(getLaneIdentity({ ...metadata }));
    });
});
