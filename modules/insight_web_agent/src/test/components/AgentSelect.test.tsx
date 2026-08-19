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
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import '@testing-library/jest-dom';
import { AgentSelect } from '../../components/AgentSelect';

const options = [
    { value: 'native', label: 'MS Insight_Native', icon: <span>N</span> },
    { value: 'claude', label: 'Claude', icon: <span>C</span> },
];

test('opens the agent list and selects an option', async () => {
    const onChange = jest.fn();
    render(<AgentSelect onChange={onChange} options={options} title="Switch Agent" value="native" />);

    fireEvent.click(screen.getByRole('button', { name: /MS Insight_Native/i }));

    await waitFor(() => expect(screen.getByText('Switch Agent')).toBeVisible());
    expect(screen.getByRole('option', { name: /MS Insight_Native/i })).toHaveAttribute('aria-selected', 'true');

    fireEvent.click(screen.getByRole('option', { name: /Claude/i }));

    expect(onChange).toHaveBeenCalledWith('claude');
    expect(screen.queryByText('Switch Agent')).not.toBeInTheDocument();
});

test('supports keyboard selection and renders the footer', async () => {
    const onChange = jest.fn();
    render(
        <AgentSelect
            footer={<button type="button">Add Agent</button>}
            onChange={onChange}
            options={options}
            title="Switch Agent"
            value="native"
        />,
    );
    const trigger = screen.getByRole('button', { name: /MS Insight_Native/i });

    fireEvent.keyDown(trigger, { key: 'ArrowDown' });
    fireEvent.keyDown(trigger, { key: 'ArrowDown' });
    fireEvent.keyDown(trigger, { key: 'Enter' });

    expect(onChange).toHaveBeenCalledWith('claude');

    fireEvent.click(trigger);
    await waitFor(() => expect(screen.getByRole('button', { name: 'Add Agent' })).toBeVisible());
});
