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
import { fireEvent, render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import { useChatState } from '../../hooks/useChatState';
import { WelcomePanel } from '../../components/WelcomePanel';

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

const mockUseChatState = useChatState as jest.Mock;

test('fills the composer with the guide description when a welcome card is clicked', () => {
    const setInput = jest.fn();
    mockUseChatState.mockReturnValue({ setInput });
    render(<WelcomePanel />);

    fireEvent.click(screen.getByRole('button', { name: /Analyze reuse strategies and tiling solutions/ }));

    expect(setInput).toHaveBeenCalledWith('Analyze reuse strategies and tiling solutions');
});
