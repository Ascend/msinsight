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
import { act, fireEvent, render } from '@testing-library/react';
import * as testing from '@testing-library/react';
import { ClusterApplicationFrame } from './ClusterApplicationFrame';
import { clusterView } from '../../cluster/viewVersion';

const initialEnv = process.env;
beforeEach(() => { process.env = { ...initialEnv, REACT_APP_IS_VSCODE: 'false' }; });
afterEach(() => { process.env = initialEnv; });

const { screen } = testing;
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
it('shows an explicit missing-entry state with unified legacy fallback', () => {
    clusterView.select('modern'); const view = render(<ClusterApplicationFrame name="Summary" visible modern onReady={jest.fn()} />);
    expect(view.container.querySelector('iframe')).toBeNull();
    fireEvent.click(screen.getByText('clusterView.legacy')); expect(clusterView.version).toBe('legacy');
});
it('requires readiness from the actual application frame and expires unavailable modern entries', () => {
    jest.useFakeTimers(); const callback = jest.fn();
    render(<ClusterApplicationFrame name="Communication" src="./plugins/Communication/index.html" visible modern onReady={callback} />);
    const frame = screen.getByTitle('Communication') as HTMLIFrameElement;
    act(() => window.dispatchEvent(new MessageEvent('message', { source: null, origin: window.location.origin, data: { event: 'communication/ready' } })));
    expect(callback).not.toHaveBeenCalled();
    act(() => window.dispatchEvent(new MessageEvent('message', { source: frame.contentWindow, origin: window.location.origin, data: { event: 'communication/ready' } })));
    expect(callback).toHaveBeenCalledTimes(1);
    act(() => jest.advanceTimersByTime(16000)); expect(screen.queryByText('clusterView.unavailable')).not.toBeInTheDocument();
    jest.useRealTimers();
});
it('remounts a failed application when retry is selected', () => {
    jest.useFakeTimers(); render(<ClusterApplicationFrame name="Summary" src="./plugins/Summary/index.html" visible modern onReady={jest.fn()} />);
    const before = screen.getByTitle('Summary'); act(() => jest.advanceTimersByTime(16000));
    fireEvent.click(screen.getByText('clusterView.retry')); expect(screen.getByTitle('Summary')).not.toBe(before);
    jest.useRealTimers();
});
it('initializes Summary through the shared ready event only from its own frame and origin', () => {
    const callback = jest.fn();
    render(<ClusterApplicationFrame name="Summary" src="./plugins/Summary/index.html" visible modern onReady={callback} />);
    const frame = screen.getByTitle('Summary') as HTMLIFrameElement;
    act(() => window.dispatchEvent(new MessageEvent('message', {
        source: frame.contentWindow, origin: 'https://unrelated.example', data: { event: 'clusterPage/ready' },
    })));
    expect(callback).not.toHaveBeenCalled();
    act(() => window.dispatchEvent(new MessageEvent('message', {
        source: frame.contentWindow,
        origin: window.location.origin,
        data: JSON.stringify({ event: 'clusterPage/ready', from: 'Summary', body: { version: 'modern' } }),
    })));
    expect(callback).toHaveBeenCalledWith(true);
});
