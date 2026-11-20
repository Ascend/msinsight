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
/** Only selections belonging to the active project may become the context used by a remounted version. */
export function selectedClusterContext<T extends { clusterList: Array<{ path: string }>; selectedClusterPath: string }>(
    current: T, activeProject: string, body: Record<string, unknown>,
): T | undefined {
    if (body.project !== activeProject || typeof body.clusterPath !== 'string' || !current.clusterList.some(cluster => cluster.path === body.clusterPath)) { return undefined; }
    return { ...current, selectedClusterPath: body.clusterPath };
}
