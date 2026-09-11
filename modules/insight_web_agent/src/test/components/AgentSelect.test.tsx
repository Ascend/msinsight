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

test('does not select a disabled option', async () => {
    const onChange = jest.fn();
    render(
        <AgentSelect
            onChange={onChange}
            options={[
                { value: 'native', label: 'MS Insight_Native' },
                { value: 'missing', label: 'OpenCode(auto) (Unavailable)', disabled: true },
            ]}
            title="Switch Agent"
            value="native"
        />,
    );

    fireEvent.click(screen.getByRole('button', { name: /MS Insight_Native/i }));
    fireEvent.click(await screen.findByRole('option', { name: /OpenCode\(auto\)/i }));

    expect(onChange).not.toHaveBeenCalled();
    expect(screen.getByRole('option', { name: /OpenCode\(auto\)/i })).toBeDisabled();
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

const modelOptions = [
    { value: 'provider/alpha', label: 'Alpha' },
    { value: 'provider/beta-disabled', label: 'Beta unavailable', disabled: true },
    { value: 'provider/beta-fast', label: 'Beta Fast' },
    { value: 'other/beta-pro', label: 'Beta Pro' },
];
const modelSearch = { placeholder: 'Search models', noResultsText: 'No matching models' };

test('searches model labels and identifiers and selects from filtered enabled options', async () => {
    const onChange = jest.fn();
    render(<AgentSelect dropdownWidth={320} onChange={onChange} options={modelOptions} search={modelSearch} value="provider/alpha" />);
    const trigger = screen.getByRole('button', { name: 'Alpha' });
    fireEvent.click(trigger);
    const input = await screen.findByRole('combobox', { name: 'Search models' });
    await waitFor(() => expect(input).toHaveFocus());
    expect(input.closest('.agent-select-dropdown')).toHaveStyle({ width: '320px' });

    fireEvent.change(input, { target: { value: ' PROVIDER/ ' } });
    expect(screen.getAllByRole('option')).toHaveLength(3);
    fireEvent.change(input, { target: { value: 'bEtA' } });
    expect(screen.queryByRole('option', { name: 'Alpha' })).not.toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Beta Fast' })).toHaveClass('focused');
    expect(onChange).not.toHaveBeenCalled();

    fireEvent.keyDown(input, { key: 'ArrowDown' });
    fireEvent.keyDown(input, { key: 'Enter', isComposing: true });
    expect(onChange).not.toHaveBeenCalled();
    fireEvent.keyDown(input, { key: 'Enter' });
    expect(onChange).toHaveBeenCalledWith('other/beta-pro');
    expect(screen.queryByRole('combobox')).not.toBeInTheDocument();
    expect(trigger).toHaveFocus();
});

test('shows an empty search result without selecting and resets search after closing', async () => {
    const onChange = jest.fn();
    render(<AgentSelect onChange={onChange} options={modelOptions} search={modelSearch} value="provider/alpha" />);
    const trigger = screen.getByRole('button', { name: 'Alpha' });
    fireEvent.click(trigger);
    const input = await screen.findByRole('combobox');
    await waitFor(() => expect(input).toHaveFocus());
    fireEvent.change(input, { target: { value: 'missing-model' } });
    expect(screen.getByRole('status')).toHaveTextContent('No matching models');
    fireEvent.keyDown(input, { key: 'ArrowDown' });
    fireEvent.keyDown(input, { key: 'Enter' });
    expect(onChange).not.toHaveBeenCalled();
    expect(input).not.toHaveAttribute('aria-activedescendant');

    fireEvent.keyDown(input, { key: 'Escape' });
    expect(trigger).toHaveFocus();
    fireEvent.click(trigger);
    expect(await screen.findByRole('combobox')).toHaveValue('');
    expect(screen.getAllByRole('option')).toHaveLength(4);
});

test('clearing search scrolls back to the selected model even when its index is unchanged', async () => {
    const onChange = jest.fn();
    const models = Array.from({ length: 12 }, (_, index) => ({ value: `model-${index}`, label: `Model ${index}` }));
    render(<AgentSelect onChange={onChange} options={models} search={modelSearch} value="model-10" />);
    fireEvent.click(screen.getByRole('button', { name: 'Model 10' }));
    const input = await screen.findByRole('combobox');
    await waitFor(() => expect(input).toHaveFocus());
    const list = screen.getByRole('listbox');
    const selected = screen.getByRole('option', { name: 'Model 10' });
    Object.defineProperty(list, 'clientHeight', { configurable: true, value: 56 });
    Object.defineProperty(selected, 'offsetHeight', { configurable: true, value: 28 });
    jest.spyOn(list, 'getBoundingClientRect').mockImplementation(() => ({ top: 100 } as DOMRect));
    jest.spyOn(selected, 'getBoundingClientRect').mockImplementation(() => ({ top: 380 - list.scrollTop } as DOMRect));

    // All models match, so clearing the query changes neither the count nor the selected index.
    fireEvent.change(input, { target: { value: 'model' } });
    await waitFor(() => expect(list.scrollTop).toBe(252));
    list.scrollTop = 0;
    fireEvent.change(input, { target: { value: '' } });

    await waitFor(() => expect(list.scrollTop).toBe(252));
    expect(selected).toHaveClass('focused');
    expect(selected).toHaveAttribute('aria-selected', 'true');
    expect(input).toHaveFocus();
    expect(onChange).not.toHaveBeenCalled();
});
