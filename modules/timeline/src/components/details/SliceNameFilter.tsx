/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React, { useEffect, useId, useState } from 'react';
import styled from '@emotion/styled';
import { message, Segmented } from 'antd';
import { Select } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import type { FilterDropdownProps } from 'antd/lib/table/interface';
import { ColumnFilterPanel } from '../base/ColumnFilterPanel';
import { nameKeywordsWithinLimit, normalizeNameKeywords } from './nameKeywords';

const ModeSwitch = styled(Segmented)`
    &&, &&:not(.ant-segmented-disabled):hover, &&:not(.ant-segmented-disabled):focus {
        background-color: ${(props): string => props.theme.bgColorLight};
        box-shadow: inset 0 0 0 1px ${(props): string => props.theme.borderColorLight};
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }
    && .ant-segmented-item-selected,
    && .ant-segmented-item-selected:hover,
    && .ant-segmented-thumb {
        color: ${(props): string => props.theme.primaryColor};
        background-color: ${(props): string => props.theme.bgColor};
        outline: 1px solid ${(props): string => props.theme.borderColorLight};
        outline-offset: -1px;
    }
    && .ant-segmented-item:not(.ant-segmented-item-selected),
    && .ant-segmented-item:not(.ant-segmented-item-selected):hover {
        color: ${(props): string => props.theme.textColorSecondary};
        background-color: transparent;
    }
    && .ant-segmented-item:hover::after {
        background-color: transparent;
    }
`;

const KeywordInput = styled(Select)`
    && .ant-select-selector {
        background-color: ${(props): string => props.theme.bgColor};
        border-color: ${(props): string => props.theme.borderColorLighter};
        max-height: 128px;
        overflow-y: auto;
        padding-left: 11px;
    }
    &&:hover .ant-select-selector {
        background-color: ${(props): string => props.theme.bgColor};
    }
    .ant-select-selection-search {
        margin-inline-start: 0;
    }
    .ant-select-selection-placeholder {
        color: ${(props): string => props.theme.textColorTertiary};
        font-family: inherit;
        font-size: 12px;
        font-weight: 400;
        opacity: 1;
    }
    .ant-select-selection-item {
        max-width: 100%;
        min-width: 0;
        padding: 0 4px;
        margin-inline-end: 3px;
    }
    && .ant-select-selection-item,
    && .ant-select-selection-item:hover {
        background-color: ${(props): string => props.theme.borderColorLight};
    }
    .ant-select-selection-item-content {
        margin-right: 2px;
    }
    .ant-select-selection-item-remove > div {
        background-size: 14px 14px;
        background-position: center;
        background-repeat: no-repeat;
    }
`;

interface SliceNameFilterProps extends FilterDropdownProps {
    includedKeywords: string[];
    excludedKeywords: string[];
    activeMode: 'include' | 'exclude';
    onApply: (mode: 'include' | 'exclude', include: string[], exclude: string[]) => void;
}

export const SliceNameFilter = ({
    confirm, setSelectedKeys, visible, includedKeywords, excludedKeywords, activeMode, onApply,
}: SliceNameFilterProps): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'sliceList' });
    const inputId = `slice-name-${useId().replace(/:/g, '')}`;
    const [drafts, setDrafts] = useState({ include: includedKeywords, exclude: excludedKeywords });
    const [mode, setMode] = useState<'include' | 'exclude'>('include');
    const [searchValues, setSearchValues] = useState({ include: '', exclude: '' });
    const keywords = drafts[mode];
    const searchValue = searchValues[mode];
    useEffect(() => {
        if (visible) {
            setDrafts({ include: includedKeywords, exclude: excludedKeywords });
            setSearchValues({ include: '', exclude: '' });
            setMode(activeMode);
        }
    }, [visible, includedKeywords, excludedKeywords, activeMode]);
    const validate = (values: string[]): boolean => {
        if (nameKeywordsWithinLimit(values)) {
            return true;
        }
        message.warning({ key: 'slice-name-keyword-limit', content: t('Keyword limit exceeded') });
        return false;
    };
    const setSearchValue = (value: string): void => {
        if (validate([...keywords, value])) {
            setSearchValues(previous => ({ ...previous, [mode]: value }));
        }
    };
    const updateKeywords = (values: string[]): boolean => {
        if (!validate(values)) {
            return false;
        }
        setDrafts(previous => ({ ...previous, [mode]: normalizeNameKeywords(values) }));
        return true;
    };
    const notifyDuplicates = (text: string): void => {
        const submitted = text.split(/[,，\r\n]/).map(value => value.trim()).filter(Boolean);
        const values = [...keywords, ...submitted];
        if (nameKeywordsWithinLimit(values) && normalizeNameKeywords(values).length < keywords.length + submitted.length) {
            message.info({ key: 'slice-name-keyword-duplicate', content: t('Duplicate keyword ignored'), duration: 2 });
        }
    };
    const apply = (): void => {
        const include = normalizeNameKeywords([...drafts.include, searchValues.include]);
        const exclude = normalizeNameKeywords([...drafts.exclude, searchValues.exclude]);
        if (!validate(include) || !validate(exclude)) {
            return;
        }
        onApply(mode, include, exclude);
        // 表格只记录当前侧关键词，用于图标状态；实际名称匹配统一由选中列表计算。
        setSelectedKeys(mode === 'include' ? include : exclude);
        confirm();
    };
    const reset = (): void => {
        const include = mode === 'include' ? [] : normalizeNameKeywords([...drafts.include, searchValues.include]);
        const exclude = mode === 'exclude' ? [] : normalizeNameKeywords([...drafts.exclude, searchValues.exclude]);
        setDrafts({ include, exclude });
        setSearchValues(previous => ({ ...previous, [mode]: '' }));
        onApply(mode, include, exclude);
        setSelectedKeys([]);
        confirm();
    };
    const pasteKeywords = (event: React.ClipboardEvent<HTMLDivElement>): void => {
        const text = event.clipboardData.getData('text');
        // Validate the entire paste atomically, including newlines that a text input would strip.
        event.preventDefault();
        event.stopPropagation();
        const input = event.target as HTMLInputElement;
        const start = input.selectionStart ?? searchValue.length;
        const end = input.selectionEnd ?? start;
        const textToCommit = searchValue.slice(0, start) + text + searchValue.slice(end);
        if (updateKeywords([...keywords, textToCommit])) {
            notifyDuplicates(textToCommit);
            setSearchValues(previous => ({ ...previous, [mode]: '' }));
        }
    };
    return <ColumnFilterPanel
        onSearch={apply}
        onReset={reset}
        header={<ModeSwitch
            block
            aria-label={t('Name filter')}
            value={mode}
            onChange={(value): void => setMode(value as 'include' | 'exclude')}
            options={[
                { value: 'include', label: <span title={t('Include mode help')}>{t('Include name keyword')}</span> },
                { value: 'exclude', label: <span title={t('Exclude mode help')}>{t('Exclude name keywords')}</span> },
            ]}
        />}
        inputControl={<div
            title={t(mode === 'include' ? 'Include keyword placeholder' : 'Exclude keywords placeholder')}
            onPasteCapture={pasteKeywords}
            onChangeCapture={(event): void => {
                if (event.target instanceof HTMLInputElement && /[,，\r\n]/.test(event.target.value) && !(event.nativeEvent as InputEvent).isComposing) {
                    notifyDuplicates(event.target.value);
                }
            }}
        >
            <KeywordInput
                id={inputId}
                aria-label={t(mode === 'include' ? 'Include name keyword' : 'Exclude name keywords')}
                mode="tags"
                width={168}
                placeholder={t(mode === 'include' ? 'Include keyword placeholder' : 'Exclude keywords placeholder')}
                value={keywords}
                searchValue={searchValue}
                onSearch={setSearchValue}
                onSelect={(): void => setSearchValues(previous => ({ ...previous, [mode]: '' }))}
                onChange={(values: string[]): void => { updateKeywords(values); }}
                tokenSeparators={[',', '，', '\n', '\r']}
                open={false}
                onInputKeyDown={(event): void => {
                    if (event.key === 'Enter' && !event.nativeEvent.isComposing) {
                        event.preventDefault();
                        event.stopPropagation();
                        if (searchValue.trim() === '') {
                            apply();
                        } else {
                            notifyDuplicates(searchValue);
                        }
                    }
                }}
            />
        </div>}
    />;
};
