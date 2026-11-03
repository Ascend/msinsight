/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import React from 'react';
import { fireEvent, render } from '@testing-library/react';
import { MemoryApplicationFrame } from './MemoryApplicationFrame';

const getFrame = (container: HTMLElement): HTMLIFrameElement => {
    const frame = container.querySelector('iframe');
    if (!frame) throw new Error('Expected memory application iframe');
    return frame;
};

it('loads the fixed application entry with its own identity and forwards initialization on load', () => {
    const onLoad = jest.fn();
    const view = render(<MemoryApplicationFrame application="MemScope" visible onLoad={onLoad} />);
    const frame = getFrame(view.container);
    expect(frame.getAttribute('src')).toBe('./plugins/MemScope/index.html');
    expect(frame.id).toBe('MemScope');
    expect(frame.name).toBe('MemScope');
    expect(frame.hasAttribute('srcdoc')).toBe(false);
    fireEvent.load(frame);
    expect(onLoad).toHaveBeenCalledTimes(1);
});

it('destroys the old frame when the application changes and preserves visibility', () => {
    const onLoad = jest.fn();
    const view = render(<MemoryApplicationFrame application="MemScope" visible onLoad={onLoad} />);
    const scopeFrame = getFrame(view.container);
    view.rerender(<MemoryApplicationFrame application="MemSnapshot" visible={false} onLoad={onLoad} />);
    const snapshotFrame = getFrame(view.container);
    expect(snapshotFrame).not.toBe(scopeFrame);
    expect(scopeFrame.isConnected).toBe(false);
    expect(snapshotFrame.getAttribute('src')).toBe('./plugins/MemSnapshot/index.html');
    expect(snapshotFrame.id).toBe('MemSnapshot');
    expect(snapshotFrame.name).toBe('MemSnapshot');
    expect(snapshotFrame.style.display).toBe('none');
});
