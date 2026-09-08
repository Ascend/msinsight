/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useState } from 'react';
import { Modal } from 'antd';
import { ArrowDownOutlined, ArrowUpOutlined, DeleteOutlined } from '@ant-design/icons';
import styled from '@emotion/styled';
import { useTheme } from '@emotion/react';
import { LocalStorageKey, localStorageService } from '@insight/lib';
import { Button, Input, Select, Tooltip, message } from '@insight/lib/components';
import { SetIcon } from '@insight/lib/icon';
import { useTranslation } from 'react-i18next';
import type { CustomClassificationRule } from '../../api/interface';
const MAX_RULES = 50;
const MAX_KEYWORDS = 20;
const MAX_CATEGORY_LENGTH = 50;
const MAX_KEYWORDS_TEXT_LENGTH = 200;
const RULE_GRID_COLUMNS = '150px minmax(260px, 1fr) 210px auto';
const BUILT_IN_CATEGORY_NAMES = new Set([
    'Computing Time', 'Paged Attention', 'SDMA', 'Flash Attention', 'Matmul', 'Conv',
    'Other Cube', 'Other Vector', 'Other',
]);
const RuleInput = styled(Input)`width: 100%;`;
const RuleActionButton = styled(Button)`display: inline-flex; align-items: center; justify-content: center;
    width: 32px; min-width: 32px; padding: 0;`;
const ActionTooltipTarget = styled.span`display: inline-flex;`;
interface StoredRule { category: string; keywords: string[]; splitByDirection?: boolean }
const getTextLength = (value: string): number => Array.from(value).length;
const getKeywordsTextLength = (keywords: string[]): number => getTextLength(keywords.join(','));
const normalizeKeywords = (value: string): string[] => value.split(',').map(keyword => keyword.trim()).filter(Boolean);
const isRule = (value: unknown): value is StoredRule => {
    if (typeof value !== 'object' || value === null) {
        return false;
    }
    const rule = value as Partial<CustomClassificationRule>;
    return typeof rule.category === 'string' && rule.category.trim().length > 0 &&
        !BUILT_IN_CATEGORY_NAMES.has(rule.category.trim()) &&
        getTextLength(rule.category) <= MAX_CATEGORY_LENGTH && Array.isArray(rule.keywords) &&
        rule.keywords.length > 0 && rule.keywords.length <= MAX_KEYWORDS &&
        rule.keywords.every(keyword => typeof keyword === 'string' && keyword.trim().length > 0) &&
        getKeywordsTextLength(rule.keywords.map(keyword => keyword.trim())) <= MAX_KEYWORDS_TEXT_LENGTH &&
        (rule.splitByDirection === undefined || typeof rule.splitByDirection === 'boolean');
};
export const loadClassificationRules = (): CustomClassificationRule[] => {
    const value = localStorageService.getItem(LocalStorageKey.OVERALL_METRICS_CLASSIFICATION_RULES);
    if (!Array.isArray(value)) {
        return [];
    }
    const categories = new Set<string>();
    return value.filter(isRule).map(rule => ({
        category: rule.category.trim(),
        keywords: rule.keywords.map(keyword => keyword.trim()),
        splitByDirection: rule.splitByDirection ?? true,
    })).filter(rule => !categories.has(rule.category) && Boolean(categories.add(rule.category))).slice(0, MAX_RULES);
};
interface ClassificationRulesProps { rules: CustomClassificationRule[]; onApply: (rules: CustomClassificationRule[]) => void }
interface RuleEditorProps extends ClassificationRulesProps { onClose: () => void }
interface DraftRule { category: string; keywords: string; splitByDirection: boolean }
const RuleEditor = ({ rules, onApply, onClose }: RuleEditorProps): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'classificationRules' });
    const [draft, setDraft] = useState<DraftRule[]>(rules.map(rule => ({ category: rule.category, keywords: rule.keywords.join(', '), splitByDirection: rule.splitByDirection })));
    const updateRule = (index: number, value: Partial<DraftRule>): void => {
        setDraft(current => current.map((rule, ruleIndex) => ruleIndex === index ? { ...rule, ...value } : rule));
    };
    const updateText = (index: number, field: 'category' | 'keywords', value: string, maxLength: number, error: string): void => {
        if (getTextLength(value) > maxLength) { message.error(t(error)); updateRule(index, { [field]: Array.from(value).slice(0, maxLength).join('') }); return; }
        updateRule(index, { [field]: value });
    };
    const updateKeywords = (index: number, value: string): void => {
        const keywords = normalizeKeywords(value);
        if (keywords.length > MAX_KEYWORDS || getKeywordsTextLength(keywords) > MAX_KEYWORDS_TEXT_LENGTH) { message.error(t('Keywords Too Long')); return; }
        updateRule(index, { keywords: value });
    };
    const moveRule = (index: number, offset: number): void => {
        const target = index + offset;
        if (target < 0 || target >= draft.length) { return; }
        const next = [...draft];
        [next[index], next[target]] = [next[target], next[index]];
        setDraft(next);
    };
    const apply = (): void => {
        const normalized = draft.map(rule => ({ category: rule.category.trim(), keywords: normalizeKeywords(rule.keywords), splitByDirection: rule.splitByDirection }));
        const conflicts = [...new Set(normalized.filter(rule => BUILT_IN_CATEGORY_NAMES.has(rule.category)).map(rule => rule.category))];
        if (conflicts.length > 0) {
            message.error(t('Reserved Category', { categories: conflicts.join(t('Keyword Separator')) }));
            return;
        }
        const validations: Array<[boolean, string]> = [
            [normalized.some(rule => !rule.category || rule.keywords.length === 0), 'Required Rule'],
            [new Set(normalized.map(rule => rule.category)).size !== normalized.length, 'Duplicate Category'],
            [normalized.some(rule => getTextLength(rule.category) > MAX_CATEGORY_LENGTH), 'Category Too Long'],
            [normalized.some(rule => rule.keywords.length > MAX_KEYWORDS ||
                getKeywordsTextLength(rule.keywords) > MAX_KEYWORDS_TEXT_LENGTH), 'Keywords Too Long'],
        ];
        const error = validations.find(([invalid]) => invalid)?.[1];
        if (error) { message.error(t(error)); return; }
        if (!localStorageService.setItem(LocalStorageKey.OVERALL_METRICS_CLASSIFICATION_RULES, normalized)) {
            message.error(t('Storage Error'));
            return;
        }
        onApply(normalized);
        onClose();
    };
    return <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 2, padding: '8px 12px', fontSize: 13, lineHeight: '20px' }}>
            {['Usage Tip Category', 'Usage Tip Keywords', 'Usage Tip Hierarchy', 'Usage Tip Priority'].map(key => <div key={key}>{t(key)}</div>)}
        </div>
        <div style={{ maxHeight: 'min(400px, 40vh)', overflowY: 'auto', paddingRight: 4, display: 'flex', flexDirection: 'column', gap: 8 }}>
            {draft.map((rule, index) => <div key={index} style={{ display: 'grid', gridTemplateColumns: RULE_GRID_COLUMNS, gap: 8 }}>
                <RuleInput aria-label={t('Category')} placeholder={t('Category')} value={rule.category} maxLength={undefined}
                    onChange={(event): void => updateText(index, 'category', event.target.value,
                        MAX_CATEGORY_LENGTH, 'Category Too Long')}
                />
                <RuleInput aria-label={t('Keywords')} placeholder={t('Keywords Placeholder')} value={rule.keywords}
                    maxLength={undefined}
                    onChange={(event): void => updateKeywords(index, event.target.value)}/>
                <Select style={{ width: '100%' }} aria-label={t('Hierarchy')}
                    value={rule.splitByDirection ? 'DIRECTION_AND_CORE' : 'CORE_ONLY'}
                    onChange={(value): void => updateRule(index, { splitByDirection: value === 'DIRECTION_AND_CORE' })}
                    options={[
                        { value: 'DIRECTION_AND_CORE', label: t('Direction And Core') },
                        { value: 'CORE_ONLY', label: t('Core Only') },
                    ]}/>
                <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                    {[
                        { label: 'Up', disabled: index === 0, icon: <ArrowUpOutlined/>, action: () => moveRule(index, -1) },
                        { label: 'Down', disabled: index === draft.length - 1, icon: <ArrowDownOutlined/>, action: () => moveRule(index, 1) },
                        { label: 'Delete', disabled: false, icon: <DeleteOutlined/>, action: () => setDraft(current => current.filter((_, itemIndex) => itemIndex !== index)) },
                    ].map(item => <Tooltip key={item.label} title={t(item.label)}>
                        <ActionTooltipTarget>
                            <RuleActionButton aria-label={t(item.label)} danger={item.label === 'Delete'}
                                disabled={item.disabled} icon={item.icon} onClick={item.action}/>
                        </ActionTooltipTarget>
                    </Tooltip>)}
                </div>
            </div>)}
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <Button disabled={draft.length >= MAX_RULES} onClick={(): void => setDraft(current =>
                [...current, { category: '', keywords: '', splitByDirection: true }])}>
                {t('Add Rule')}
            </Button>
            <div style={{ display: 'flex', gap: 8 }}>
                <Button onClick={onClose}>{t('Cancel')}</Button>
                <Button type="primary" onClick={apply}>{t('Apply')}</Button>
            </div>
        </div>
    </div>;
};
export const ClassificationRules = ({ rules, onApply }: ClassificationRulesProps): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'classificationRules' });
    const theme = useTheme();
    const [open, setOpen] = useState(false);
    const hideRules = (): void => setOpen(false);
    return <>
        <Tooltip title={t('Classification Rules Tips')} placement={'topLeft'}>
            <ActionTooltipTarget>
                <Button type="text" htmlType="button" aria-label={t('Classification Rules')}
                    style={{ width: 20, minWidth: 20, height: 20, padding: 0, border: 0 }}
                    onClick={(event): void => {
                        event.stopPropagation();
                        setOpen(true);
                    }}>
                    <SetIcon width={16} height={16} color={theme.iconColor}/>
                </Button>
            </ActionTooltipTarget>
        </Tooltip>
        <Modal title={t('Computing Classification Rules')} width={1050} open={open} footer={null}
            destroyOnClose onCancel={hideRules}>
            <RuleEditor key={open ? 'open' : 'closed'} rules={open ? loadClassificationRules() : rules}
                onApply={onApply} onClose={hideRules} />
        </Modal>
    </>;
};
