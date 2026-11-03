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
import { useCallback, useEffect, useRef, useState, type Dispatch, type SetStateAction } from 'react';
import { reportTabKey, type AgentReportReference } from '@insight/lib/AgentReport';

export const useReportTabs = ({ activeModule, setActiveModule, moduleIds }: {
    activeModule: string; setActiveModule: Dispatch<SetStateAction<string>>; moduleIds: string[];
}): { reports: AgentReportReference[]; openReport: (report: AgentReportReference) => void; closeReport: (id: string) => void } => {
    const [reports, setReports] = useState<AgentReportReference[]>([]);
    const previousModule = useRef(activeModule);
    useEffect(() => {
        if (moduleIds.includes(activeModule)) previousModule.current = activeModule;
    }, [activeModule, moduleIds]);
    const openReport = useCallback((report: AgentReportReference): void => {
        setReports((current) => current.some((item) => item.artifactId === report.artifactId)
            ? current.map((item) => item.artifactId === report.artifactId ? report : item)
            : [...current, report]);
        setActiveModule(reportTabKey(report));
    }, [setActiveModule]);
    const closeReport = useCallback((id: string): void => {
        setReports((current) => current.filter((item) => item.artifactId !== id));
        setActiveModule((current) => current === `report:${id}`
            ? moduleIds.includes(previousModule.current) ? previousModule.current : moduleIds[0] ?? 'Timeline'
            : current);
    }, [setActiveModule, moduleIds]);
    return { reports, openReport, closeReport };
};
