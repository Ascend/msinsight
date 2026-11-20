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
import { selectedClusterContext } from './selection';
const current = { selectedClusterPath: 'a', clusterList: [{ path: 'a', parsed: true }, { path: 'b', parsed: false }] };
it('retains the chosen cluster and parse metadata for either version', () => {
    expect(selectedClusterContext(current, 'p', { project: 'p', clusterPath: 'b' })).toEqual({ ...current, selectedClusterPath: 'b' });
    expect(current.selectedClusterPath).toBe('a');
});
it('ignores stale project messages and unknown cluster paths', () => {
    expect(selectedClusterContext(current, 'new', { project: 'old', clusterPath: 'b' })).toBeUndefined();
    expect(selectedClusterContext(current, 'p', { project: 'p', clusterPath: 'unknown' })).toBeUndefined();
});
