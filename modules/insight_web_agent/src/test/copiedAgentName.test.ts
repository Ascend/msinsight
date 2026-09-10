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
import { uniqueCopiedAgentName } from '../copiedAgentName';

test('uses the catalog name without the auto suffix when it is free', () => {
    expect(uniqueCopiedAgentName('OpenCode(auto)', ['OpenCode(auto)'])).toBe('OpenCode');
});

test('appends Copy when the stripped catalog name is already taken', () => {
    expect(uniqueCopiedAgentName('OpenCode(auto)', ['OpenCode(auto)', 'OpenCode'])).toBe('OpenCode Copy');
    expect(uniqueCopiedAgentName('OpenCode(auto)', ['OpenCode(auto)', 'OpenCode', 'OpenCode Copy'])).toBe('OpenCode Copy 2');
});
