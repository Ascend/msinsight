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
import React, { useState } from 'react';
// eslint-disable-next-line import/named
import { fireEvent, render, screen } from '@testing-library/react';
import { parseAgentReportReference } from '@insight/lib/AgentReport';
import { useReportTabs } from './useReportTabs';

const report = { artifactId: '11111111-1111-1111-1111-111111111111', title: 'Weekly report', version: 1 };
const other = { ...report, artifactId: '22222222-2222-2222-2222-222222222222' };
const moduleIds = ['Timeline', 'Memory'];

test('reuses report tabs, updates their revision and restores the previous business module on close', () => {
    const Harness = (): JSX.Element => {
        const [activeModule, setActiveModule] = useState('Memory');
        const { reports, openReport, closeReport } = useReportTabs({ activeModule, setActiveModule, moduleIds });
        return <>
            <output>{JSON.stringify({ activeModule, reports })}</output>
            <button onClick={() => openReport(report)}>Open</button>
            <button onClick={() => openReport({ ...report, version: 2 })}>Update</button>
            <button onClick={() => openReport(other)}>Other</button>
            <button onClick={() => closeReport(report.artifactId)}>Close first</button>
            <button onClick={() => closeReport(other.artifactId)}>Close second</button>
        </>;
    };
    render(<Harness />);
    const state = (): { activeModule: string; reports: Array<typeof report> } => JSON.parse(screen.getByRole('status').textContent ?? '{}');
    fireEvent.click(screen.getByText('Open'));
    fireEvent.click(screen.getByText('Update'));
    expect(state()).toEqual({ activeModule: `report:${report.artifactId}`, reports: [{ ...report, version: 2 }] });
    fireEvent.click(screen.getByText('Other'));
    fireEvent.click(screen.getByText('Close first'));
    expect(state()).toEqual({ activeModule: `report:${other.artifactId}`, reports: [other] });
    fireEvent.click(screen.getByText('Close second'));
    expect(state()).toEqual({ activeModule: 'Memory', reports: [] });
});

test('report references reject arbitrary paths and invalid versions and strip document payloads', () => {
    expect(parseAgentReportReference({ ...report, html: '<script>run()</script>', url: 'https://example.com' })).toEqual(report);
    expect(parseAgentReportReference({ ...report, artifactId: '../config' })).toBeUndefined();
    expect(parseAgentReportReference({ ...report, version: -1 })).toBeUndefined();
    expect(parseAgentReportReference({ ...report, version: '1' })).toBeUndefined();
});
