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
import { ThemeProvider } from '@emotion/react';
import { render } from '@testing-library/react';
import '@testing-library/jest-dom';
import { themeInstance } from '@insight/lib/theme';
import type { ChatMessage, ConversationNotice } from '../../types';
import { MessageList } from '../../components/MessageList';

const mockRender = jest.fn();

jest.mock('../../components/markdown/MarkdownRenderer', () => ({
    MarkdownRenderer: ({ text }: { text: string }) => {
        mockRender(text);
        return <div>{text}</div>;
    },
}));

const messages: ChatMessage[] = [{
    id: 'memo-message',
    role: 'assistant',
    content: [
        { id: 'memo-thinking', type: 'thinking', text: 'thinking text' },
        { id: 'memo-block', type: 'text', text: 'hello' },
    ],
}];
const notices: ConversationNotice[] = [];
const onPermissionDecision = jest.fn().mockResolvedValue(undefined);

const renderList = (): JSX.Element => (
    <ThemeProvider theme={themeInstance.getTheme().dark}>
        <MessageList
            messages={messages}
            notices={notices}
            onPermissionDecision={onPermissionDecision}
            pendingPrompt={false}
        />
    </ThemeProvider>
);

describe('MessageList memoization', () => {
    test('skips thinking and message rendering when the parent re-renders with the same props', () => {
        const { rerender } = render(renderList());
        // Thinking timeline renders MarkdownRenderer directly, so it would re-render
        // whenever the list re-renders even if ContentBlock is memoized.
        expect(mockRender).toHaveBeenCalledTimes(2);

        rerender(renderList());
        expect(mockRender).toHaveBeenCalledTimes(2);
    });
});
