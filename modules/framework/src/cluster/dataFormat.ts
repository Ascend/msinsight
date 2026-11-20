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
import { ProjectType } from '../utils/enum';
import type { FileOrDirectory } from '../centralServer/websocket/defs';

export interface ClusterDataContext {
    activeDataSource?: { children?: FileOrDirectory[] };
    clusterPageInfo?: { selectedClusterPath: string };
    compareSet?: { baseline?: { projectName: string; rankId?: string } };
}

/** Use import metadata: parsed text data also has internal .db files and isFullDb is rank-scoped. */
export function supportsModernCluster(context: ClusterDataContext): boolean {
    const children = context.activeDataSource?.children ?? [];
    const clusters = children.filter(child => child.type === 'CLUSTER');
    const selectedPath = context.clusterPageInfo?.selectedClusterPath;
    const cluster = clusters.find(child => child.path === selectedPath) ?? (!selectedPath && clusters.length === 1 ? clusters[0] : undefined);
    if (clusters.length && !cluster) { return false; }
    const isDb = (type?: ProjectType): boolean => type === ProjectType.DB || type === ProjectType.DB_CLUSTER;
    if (cluster?.projectType !== undefined) { return isDb(cluster.projectType); }
    const ranks = (cluster?.children ?? children).filter(child => child.type === 'RANK');
    return ranks.length > 0 && ranks.every(rank => isDb(rank.projectType));
}
