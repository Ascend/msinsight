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
// React Testing Library re-exports screen/waitFor; the repository's import resolver misses these exports.
// eslint-disable-next-line import/named
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import '@testing-library/jest-dom';
import { fetchReport } from '../../api';
import { reportFromTool, reportSrcDoc, type ReportArtifact } from '../../artifacts';
import { ReportCard, ReportWorkspace } from '../../components/ReportWorkspace';
import { ReportPreview } from '../../components/ReportPreview';
import { useChatState } from '../../hooks/useChatState';
import { requestHostOpenReport } from '../../connection';
import type { ToolCallItem } from '../../types';

jest.mock('../../api', () => ({ fetchReport: jest.fn() }));
jest.mock('../../hooks/useChatState', () => ({ useChatState: jest.fn() }));
jest.mock('../../connection', () => ({ requestHostOpenReport: jest.fn() }));
const originalParent = Object.getOwnPropertyDescriptor(window, 'parent');
afterEach(() => {
    if (originalParent) Object.defineProperty(window, 'parent', originalParent);
});

const report: ReportArtifact = {
    kind: 'visualization',
    artifactId: '11111111-1111-1111-1111-111111111111',
    version: 1,
    title: 'Pipeline report',
    description: 'Seven days of sample data',
    createdAt: '2026-11-03T00:00:00Z',
};
const tool: ToolCallItem = { name: 'publish_report', toolCallId: 'call-1', status: 'completed', output: JSON.stringify(report) };
const message = { id: 'answer', role: 'assistant', content: [{ id: 'call-1', type: 'tool', toolCall: tool }] };
const document = { ...report, html: '<h1>Sample report</h1>', versions: [report, { ...report, version: 2 }] };

beforeEach(() => {
    jest.clearAllMocks();
    (fetchReport as jest.Mock).mockResolvedValue(document);
    (requestHostOpenReport as jest.Mock).mockResolvedValue(true);
});

test('embedded cards open directly in the main area, retaining chat and offering retry on failure', async () => {
    Object.defineProperty(window, 'parent', { configurable: true, value: {} });
    (useChatState as jest.Mock).mockReturnValue({ currentSessionId: 'one', messages: [message] });
    (requestHostOpenReport as jest.Mock).mockResolvedValueOnce(false);
    render(<ReportWorkspace><textarea aria-label="Chat input" defaultValue="Continue analysis" /><ReportCard tool={tool} /></ReportWorkspace>);
    expect(requestHostOpenReport).not.toHaveBeenCalled();
    fireEvent.click(screen.getByRole('button', { name: /Pipeline report/ }));
    await screen.findByRole('alert');
    expect(requestHostOpenReport).toHaveBeenCalledWith(report);
    expect(fetchReport).not.toHaveBeenCalled();
    expect(screen.queryByLabelText('REPORT PREVIEW')).not.toBeInTheDocument();
    expect(screen.getByLabelText('Chat input')).toHaveValue('Continue analysis');
    fireEvent.click(screen.getByText('Retry'));
    await waitFor(() => expect(screen.queryByRole('alert')).not.toBeInTheDocument());
    expect(requestHostOpenReport).toHaveBeenCalledTimes(2);
});

test('embedded live reports open once in the main area without opening restored history', async () => {
    Object.defineProperty(window, 'parent', { configurable: true, value: {} });
    let state = { currentSessionId: 'one', messages: [message], pendingPrompt: false };
    (useChatState as jest.Mock).mockImplementation(() => state);
    const view = render(<ReportWorkspace>Chat</ReportWorkspace>);
    expect(requestHostOpenReport).not.toHaveBeenCalled();
    state = { ...state, messages: [], pendingPrompt: true };
    view.rerender(<ReportWorkspace>Chat</ReportWorkspace>);
    state = { ...state, messages: [message], pendingPrompt: false };
    view.rerender(<ReportWorkspace>Chat</ReportWorkspace>);
    await waitFor(() => expect(requestHostOpenReport).toHaveBeenCalledWith(report));
    view.rerender(<ReportWorkspace>Chat</ReportWorkspace>);
    expect(requestHostOpenReport).toHaveBeenCalledTimes(1);
    expect(fetchReport).not.toHaveBeenCalled();
    expect(screen.queryByLabelText('REPORT PREVIEW')).not.toBeInTheDocument();
});

test('supports native and MCP results and does not render arbitrary or failed tool output', () => {
    expect(reportFromTool(tool)).toEqual(report);
    expect(reportFromTool({ ...tool, output: JSON.stringify({ content: [{ type: 'text', text: JSON.stringify(report) }] }) })).toEqual(report);
    expect(reportFromTool({ ...tool, status: 'failed' })).toBeUndefined();
    expect(reportFromTool({ ...tool, output: JSON.stringify({ ...report, artifactId: '../secret' }) })).toBeUndefined();
    expect(reportFromTool({ ...tool, output: '<script>alert(1)</script>' })).toBeUndefined();
});

test('unwraps ACP output and falls back to nested content when raw output is missing or truncated', () => {
    const wrappedOutput = JSON.stringify({ output: JSON.stringify(report), metadata: { truncated: false } });
    const progress = JSON.stringify([{ type: 'content', content: { type: 'text', text: JSON.stringify(report) } }]);
    expect(reportFromTool({ ...tool, output: wrappedOutput })).toEqual(report);
    expect(reportFromTool({ ...tool, output: undefined, progress })).toEqual(report);
    expect(reportFromTool({ ...tool, output: '{"output":"truncated…', progress })).toEqual(report);
    expect(reportFromTool({ ...tool, output: JSON.stringify({ structuredContent: {}, output: JSON.stringify(report) }) })).toEqual(report);
    expect(reportFromTool({ ...tool, status: 'failed', output: wrappedOutput, progress })).toBeUndefined();
});

test('opens a saved report from an ACP-wrapped publish_report result', async () => {
    const wrappedTool = {
        ...tool,
        name: 'msinsight-capabilities_publish_report',
        output: JSON.stringify({
            output: JSON.stringify(report), metadata: { truncated: false },
        }),
    };
    (useChatState as jest.Mock).mockReturnValue({
        currentSessionId: 'acp-session',
        isDraftSession: false,
        pendingPrompt: false,
        messages: [{ ...message, content: [{ id: wrappedTool.toolCallId, type: 'tool', toolCall: wrappedTool }] }],
    });
    render(<ReportWorkspace><ReportCard tool={wrappedTool} /></ReportWorkspace>);
    const card = screen.getByRole('button', { name: /Pipeline report/ });
    expect(card).toBeEnabled();
    fireEvent.click(card);
    await screen.findByTitle(report.title);
    expect(fetchReport).toHaveBeenCalledWith(report.artifactId, report.version, expect.any(AbortSignal));
});

test('loads authenticated HTML into an opaque sandbox, supports version selection and retry', async () => {
    const onClose = jest.fn();
    const view = render(<ReportPreview report={report} onClose={onClose} />);
    const frame = await screen.findByTitle(report.title);
    expect(frame).toHaveAttribute('sandbox', 'allow-scripts');
    expect(frame).toHaveAttribute('referrerpolicy', 'no-referrer');
    expect(frame).not.toHaveAttribute('src');
    expect(frame.getAttribute('srcdoc')).toContain('connect-src \'none\'');
    (fetchReport as jest.Mock).mockRejectedValueOnce(new Error('not found'));
    fireEvent.change(screen.getByLabelText('Report version'), { target: { value: '2' } });
    await screen.findByRole('alert');
    expect(view.container.querySelector('iframe')).toBeNull();
    (fetchReport as jest.Mock).mockResolvedValue({ ...document, version: 2 });
    fireEvent.click(screen.getByText('Retry'));
    await waitFor(() => expect(view.container.querySelector('iframe')).not.toBeNull());
    expect(fetchReport).toHaveBeenLastCalledWith(report.artifactId, 2, expect.any(AbortSignal));
    fireEvent.click(screen.getByLabelText('Close report'));
    expect(onClose).toHaveBeenCalledTimes(1);
});

test('standalone history starts closed, cards reopen it, switching session closes it and new live reports auto-open', async () => {
    let state = { currentSessionId: 'one', isDraftSession: false, messages: [message], pendingPrompt: false };
    (useChatState as jest.Mock).mockImplementation(() => state);
    const tree = <ReportWorkspace><ReportCard tool={tool} /></ReportWorkspace>;
    const view = render(tree);
    expect(screen.queryByLabelText('REPORT PREVIEW')).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: /Pipeline report/ }));
    await screen.findByTitle(report.title);
    state = { ...state, currentSessionId: 'two', messages: [], pendingPrompt: true };
    view.rerender(tree);
    // Re-render with a fresh element so React observes updated context mocks.
    view.rerender(<ReportWorkspace><ReportCard tool={tool} /></ReportWorkspace>);
    expect(screen.queryByLabelText('REPORT PREVIEW')).not.toBeInTheDocument();
    state = { ...state, messages: [message], pendingPrompt: false };
    view.rerender(<ReportWorkspace><ReportCard tool={tool} /></ReportWorkspace>);
    await screen.findByTitle(report.title);
    fireEvent.click(screen.getByLabelText('Close report'));
    view.rerender(<ReportWorkspace><ReportCard tool={tool} /></ReportWorkspace>);
    expect(screen.queryByLabelText('REPORT PREVIEW')).not.toBeInTheDocument();
});

test('puts restrictive CSP before generated markup even if the document contains another policy', () => {
    const html = '<html><head><meta http-equiv="Content-Security-Policy" content="default-src *"></head><body><script>run()</script></body></html>';
    const preview = reportSrcDoc(html);
    expect(preview.indexOf('default-src \'none\'')).toBeLessThan(preview.indexOf('default-src *'));
    expect(preview).toContain('frame-src \'none\'');
    expect(preview).toContain('form-action \'none\'');
});
