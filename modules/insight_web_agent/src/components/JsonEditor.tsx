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
import CodeMirror from '@uiw/react-codemirror';
import { json } from '@codemirror/lang-json';
import styled from '@emotion/styled';
import { useTheme } from '@emotion/react';
import React from 'react';

const EditorContainer = styled.div`
    min-width: 0;
    overflow: hidden;
    background: ${(props): string => props.theme.bgColor};

    .cm-editor,
    .cm-gutters {
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .cm-editor {
        min-height: 320px;
        font-size: 13px;
    }

    .cm-gutters {
        border-right-color: ${(props): string => props.theme.borderColor};
        color: ${(props): string => props.theme.textColorSecondary};
    }

    .cm-activeLine,
    .cm-activeLineGutter,
    .cm-selectionBackground {
        background: ${(props): string => props.theme.bgColorDark} !important;
    }

    .cm-focused {
        outline: none;
    }

    .cm-scroller {
        max-height: 480px;
        font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
        line-height: 1.6;
    }
`;

interface JsonEditorProps {
    value: string;
    onChange: (value: string) => void;
    ariaLabel: string;
}

export const JsonEditor = ({ value, onChange, ariaLabel }: JsonEditorProps): JSX.Element => {
    const theme = useTheme();
    return <EditorContainer>
        <CodeMirror
            aria-label={ariaLabel}
            basicSetup={{
                bracketMatching: true,
                closeBrackets: true,
                foldGutter: true,
                highlightActiveLine: true,
                highlightActiveLineGutter: true,
                lineNumbers: true,
            }}
            extensions={[json()]}
            height="320px"
            onChange={onChange}
            theme={theme.mode}
            value={value}
        />
    </EditorContainer>;
};
