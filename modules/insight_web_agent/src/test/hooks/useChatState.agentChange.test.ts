/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { resolveAppliedAgentChange } from '../../hooks/useChatState';

test('detects an agent change from the fetched runtime state when the snapshot is stale', () => {
    expect(resolveAppliedAgentChange('OpenCode', 'OpenCode', 'Claude')).toEqual({
        effectiveAgentName: 'Claude',
        changed: true,
    });
});

test('does not report an agent change when the fetched runtime state keeps the current agent', () => {
    expect(resolveAppliedAgentChange('OpenCode', 'Claude', 'OpenCode')).toEqual({
        effectiveAgentName: 'OpenCode',
        changed: false,
    });
});
