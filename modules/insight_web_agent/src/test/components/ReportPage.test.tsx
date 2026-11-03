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
import React from 'react';
// eslint-disable-next-line import/named
import { render, screen, fireEvent } from '@testing-library/react';
import '@testing-library/jest-dom';
import { ReportPage } from '../../components/ReportPage';
import { fetchReport } from '../../api';
import { requestHostCloseReport } from '../../connection';

jest.mock('../../api', () => ({ fetchReport: jest.fn() }));
jest.mock('../../connection', () => ({ requestHostCloseReport: jest.fn() }));

afterEach(() => { window.history.replaceState({}, '', '/'); });

test('loads a standalone report without a chat provider and closes its host tab', async () => {
    const id = '11111111-1111-1111-1111-111111111111';
    window.history.replaceState({}, '', `/?view=report&reportId=${id}&reportVersion=2`);
    (fetchReport as jest.Mock).mockResolvedValue({ artifactId: id, title: 'Main area report', version: 2, html: '<h1>Report</h1>', versions: [{ version: 2 }] });
    render(<ReportPage />);
    expect(await screen.findByTitle('Main area report')).toHaveAttribute('sandbox', 'allow-scripts');
    expect(screen.queryByText('Open in main area')).not.toBeInTheDocument();
    fireEvent.click(screen.getByLabelText('Close report'));
    expect(requestHostCloseReport).toHaveBeenCalledTimes(1);
});
