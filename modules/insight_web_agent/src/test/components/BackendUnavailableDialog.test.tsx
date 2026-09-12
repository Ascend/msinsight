/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { act, render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import type { ReactNode } from 'react';
import i18n from '@insight/lib/i18n';
import { BackendUnavailableDialog } from '../../components/BackendUnavailableDialog';
import { reportBackendAvailable, reportBackendUnavailable } from '../../backendConnection';

jest.mock('antd', () => ({
    Modal: ({ open, title, children }: { open: boolean; title: ReactNode; children: ReactNode }) => (
        open ? <div role="dialog"><h1>{title}</h1>{children}</div> : null
    ),
}));

afterEach(() => {
    reportBackendAvailable();
});

beforeAll(async () => {
    await i18n.changeLanguage('zhCN');
});

test('shows install guidance only when Node.js is missing', async () => {
    render(<BackendUnavailableDialog />);
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();

    await act(async () => {
        reportBackendUnavailable({ url: 'acp', status: 'missing-node' });
    });

    expect(screen.getByRole('dialog')).toHaveTextContent('Node.js 运行环境不可用');
    expect(screen.getByRole('dialog')).toHaveTextContent('https://nodejs.org/zh-cn/download');
});

test('does not tell the user to install Node.js when the service is merely unreachable', async () => {
    render(<BackendUnavailableDialog />);

    await act(async () => {
        reportBackendUnavailable({ url: 'http://127.0.0.1:9090/api/state', status: 'unreachable' });
    });

    expect(screen.getByRole('dialog')).toHaveTextContent('无法连接 Agent 服务');
    expect(screen.queryByText(/请安装 Node.js/)).not.toBeInTheDocument();
    expect(screen.queryByText('https://nodejs.org/zh-cn/download')).not.toBeInTheDocument();
});

test('closes when the backend becomes available', async () => {
    render(<BackendUnavailableDialog />);
    await act(async () => {
        reportBackendUnavailable({ url: 'acp', status: 'start-failed' });
    });
    expect(screen.getByRole('dialog')).toBeInTheDocument();

    await act(async () => {
        reportBackendAvailable();
    });

    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
});

test('shows the detected Node.js version when it is too old', async () => {
    render(<BackendUnavailableDialog />);
    await act(async () => {
        reportBackendUnavailable({ url: 'acp', status: 'unsupported-node', nodeVersion: '18.20.0' });
    });

    expect(screen.getByRole('dialog')).toHaveTextContent('18.20.0');
    expect(screen.getByRole('dialog')).toHaveTextContent('22.14.0');
});
