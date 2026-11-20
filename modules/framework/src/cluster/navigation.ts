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
interface Intent { context: string; event: string; body: Record<string, unknown> }
/** At most the latest navigation per page is buffered, and only for its originating data context. */
export class ClusterNavigation {
    private readonly ready = new Set<string>();
    private readonly pending = new Map<string, Intent>();
    begin(name: string): void { this.ready.delete(name); }
    defer(name: string, intent: Intent): boolean {
        if (this.ready.has(name)) { return false; }
        this.pending.set(name, intent); return true;
    }

    mounted(name: string, context: string): Intent | undefined {
        this.ready.add(name); const intent = this.pending.get(name); this.pending.delete(name);
        return intent?.context === context ? intent : undefined;
    }

    remove(name: string): void { this.ready.delete(name); this.pending.delete(name); }
    reset(): void { this.ready.clear(); this.pending.clear(); }
}
export const clusterNavigation = new ClusterNavigation();
export const navigationContext = (project: string, clusterPath: string): string => JSON.stringify([project, clusterPath]);
