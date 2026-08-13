/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useDeferredValue, useEffect, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import {
    CloseCircleFilled,
    DownOutlined,
    SearchOutlined,
    SortAscendingOutlined,
    SortDescendingOutlined,
    UpOutlined,
} from '@ant-design/icons';
import { useTranslation } from 'react-i18next';

export interface CallStackGroupData {
    key: string;
    label: string;
    lines: string[];
}

export interface CallStackDisplayLine {
    text: string;
    originalLineNumber: number;
    displayLineNumber: number;
}

export interface CallStackDisplayGroup extends Omit<CallStackGroupData, 'lines'> {
    lines: CallStackDisplayLine[];
}

export type CallStackOrder = 'forward' | 'reverse';

export const getCallStackDisplayGroups = (
    groups: CallStackGroupData[],
    order: CallStackOrder,
): CallStackDisplayGroup[] => {
    return groups.map(group => {
        const numberedLines = group.lines.map((text, index) => ({ text, originalLineNumber: index + 1 }));
        const orderedLines = order === 'reverse' ? numberedLines.reverse() : numberedLines;
        return {
            key: group.key,
            label: group.label,
            lines: orderedLines.map((line, index) => ({ ...line, displayLineNumber: index + 1 })),
        };
    });
};

export interface CallStackHighlightPart {
    text: string;
    matched: boolean;
}

export const getCallStackHighlightParts = (text: string, keyword: string): CallStackHighlightPart[] => {
    const normalizedKeyword = keyword.trim();
    if (normalizedKeyword.length < 1) {
        return [{ text, matched: false }];
    }
    const parts: CallStackHighlightPart[] = [];
    const lowerText = text.toLocaleLowerCase();
    const lowerKeyword = normalizedKeyword.toLocaleLowerCase();
    let start = 0;
    let matchIndex = lowerText.indexOf(lowerKeyword, start);
    while (matchIndex >= 0) {
        if (matchIndex > start) {
            parts.push({ text: text.slice(start, matchIndex), matched: false });
        }
        const matchEnd = matchIndex + normalizedKeyword.length;
        parts.push({ text: text.slice(matchIndex, matchEnd), matched: true });
        start = matchEnd;
        matchIndex = lowerText.indexOf(lowerKeyword, start);
    }
    if (start < text.length) {
        parts.push({ text: text.slice(start), matched: false });
    }
    return parts.length > 0 ? parts : [{ text, matched: false }];
};

const Viewer = styled.div`
    box-sizing: border-box;
    container-name: call-stack-viewer;
    container-type: inline-size;
    display: flex;
    flex: 1;
    flex-direction: column;
    min-height: 0;
    height: 100%;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 4px;
    background: ${(props): string => props.theme.bgColor};
`;

const Toolbar = styled.div`
    box-sizing: border-box;
    display: grid;
    flex: 0 0 auto;
    grid-template-columns: max-content minmax(180px, 520px) max-content;
    align-items: center;
    gap: 8px;
    min-height: 38px;
    padding: 5px 8px;
    border-bottom: 1px solid ${(props): string => props.theme.borderColorLight};

    @container call-stack-viewer (max-width: 440px) {
        grid-template-columns: minmax(0, 1fr) max-content;
    }
`;

const ViewerTitle = styled.div`
    flex: 0 0 auto;
    min-width: 58px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;
    font-weight: 600;
    line-height: 26px;
    white-space: nowrap;

    @container call-stack-viewer (max-width: 440px) {
        grid-column: 1;
        grid-row: 1;
    }
`;

const SearchBox = styled.div`
    box-sizing: border-box;
    display: flex;
    min-width: 180px;
    width: 100%;
    height: 26px;
    align-items: center;
    gap: 5px;
    padding: 0 7px;
    color: ${(props): string => props.theme.textColorSecondary};
    background: ${(props): string => props.theme.bgColorCommon};
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 3px;

    @container call-stack-viewer (max-width: 440px) {
        grid-column: 1 / -1;
        grid-row: 2;
        min-width: 0;
    }

    &:focus-within {
        border-color: ${(props): string => props.theme.primaryColor};
        box-shadow: 0 0 0 1px ${(props): string => props.theme.primaryColorLight4};
    }

    input {
        min-width: 0;
        width: 100%;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        background: transparent;
        border: none;
        outline: none;
    }

    input::-webkit-search-cancel-button {
        display: none;
    }
`;

const ClearSearchButton = styled.button`
    display: inline-flex;
    padding: 0;
    color: ${(props): string => props.theme.textColorSecondary};
    background: transparent;
    border: none;
    cursor: pointer;

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }
`;

const SearchActionButton = styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 20px;
    height: 20px;
    padding: 0;
    color: ${(props): string => props.theme.textColorSecondary};
    background: transparent;
    border: none;
    border-radius: 2px;
    cursor: pointer;

    &:hover:not(:disabled) {
        color: ${(props): string => props.theme.textColorPrimary};
        background: ${(props): string => props.theme.bgColorLight};
    }

    &:disabled {
        opacity: 0.45;
        cursor: default;
    }

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }
`;

const OrderButton = styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 4px;
    height: 26px;
    padding: 0 8px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    white-space: nowrap;
    background: transparent;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 3px;
    cursor: pointer;
    transition: color 150ms ease, background-color 150ms ease, border-color 150ms ease;

    &:hover {
        color: ${(props): string => props.theme.textColorPrimary};
        background: ${(props): string => props.theme.bgColorLight};
        border-color: ${(props): string => props.theme.primaryColor};
    }

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }

    @container call-stack-viewer (max-width: 440px) {
        grid-column: 2;
        grid-row: 1;
    }
`;

const MatchCount = styled.span`
    flex: 0 0 auto;
    min-width: 34px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    font-variant-numeric: tabular-nums;
    text-align: right;
`;

const Groups = styled.div`
    flex: 1 1 0;
    min-height: 0;
    overflow: auto;
`;

const Group = styled.div`
    &:not(:first-of-type) {
        border-top: 1px solid ${(props): string => props.theme.borderColorLight};
    }
`;

const GroupTitle = styled.div`
    padding: 5px 8px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    font-weight: 600;
`;

const Line = styled.div`
    display: grid;
    grid-template-columns: 38px minmax(0, 1fr);
    min-width: 0;
    padding: 1px 8px 1px 0;
    font-family: Consolas, "Courier New", monospace;
    font-size: 12px;
    line-height: 17px;
`;

const LineNumber = styled.div`
    color: ${(props): string => props.theme.textColorSecondary};
    text-align: right;
    user-select: none;
`;

const LineText = styled.div`
    min-width: 0;
    padding-left: 8px;
    overflow-wrap: anywhere;
    white-space: pre-wrap;

    mark {
        padding: 0;
        color: inherit;
        background: #ffd666;
    }

    mark[data-current-match='true'] {
        background: #ff9c2a;
        box-shadow: 0 0 0 1px #d46b08;
    }
`;

const NoResult = styled.div`
    padding: 18px 10px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    text-align: center;
`;

const HighlightedLine = ({
    text,
    keyword,
    firstMatchIndex,
    activeMatchIndex,
}: {
    text: string;
    keyword: string;
    firstMatchIndex: number;
    activeMatchIndex: number;
}): JSX.Element => {
    let lineMatchIndex = firstMatchIndex;
    return <>
        {getCallStackHighlightParts(text, keyword).map((part, index) => {
            if (!part.matched) {
                return <React.Fragment key={`${index}_${part.text}`}>{part.text}</React.Fragment>;
            }
            const matchIndex = lineMatchIndex;
            lineMatchIndex += 1;
            return <mark
                key={`${index}_${part.text}`}
                data-current-match={String(matchIndex === activeMatchIndex)}
            >{part.text}</mark>;
        })}
    </>;
};

interface LineMatchInfo {
    firstMatchIndex: number;
    count: number;
}

interface CallStackMatchInfo {
    id: string;
    lineKey: string;
}

const getLineKey = (groupKey: string, originalLineNumber: number): string =>
    JSON.stringify([groupKey, originalLineNumber]);

const getMatchId = (lineKey: string, keyword: string, occurrenceIndex: number): string =>
    JSON.stringify([lineKey, keyword.trim().toLocaleLowerCase(), occurrenceIndex]);

export const CallStackViewer = ({ groups }: { groups: CallStackGroupData[] }): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [order, setOrder] = useState<CallStackOrder>('forward');
    const [keyword, setKeyword] = useState('');
    const [activeMatchId, setActiveMatchId] = useState<string | null>(null);
    const lineRefs = useRef(new Map<string, HTMLDivElement>());
    const isComposingRef = useRef(false);
    const deferredKeyword = useDeferredValue(keyword);
    const displayGroups = useMemo(
        () => getCallStackDisplayGroups(groups, order),
        [groups, order],
    );
    const hasKeyword = deferredKeyword.trim().length > 0;
    const matchState = useMemo(() => {
        const byLine = new Map<string, LineMatchInfo>();
        const matches: CallStackMatchInfo[] = [];
        displayGroups.forEach(group => {
            group.lines.forEach(line => {
                const lineKey = getLineKey(group.key, line.originalLineNumber);
                const count = getCallStackHighlightParts(line.text, deferredKeyword)
                    .filter(part => part.matched).length;
                byLine.set(lineKey, { firstMatchIndex: matches.length, count });
                for (let index = 0; index < count; index += 1) {
                    matches.push({
                        id: getMatchId(lineKey, deferredKeyword, index),
                        lineKey,
                    });
                }
            });
        });
        return { byLine, matches, matchCount: matches.length };
    }, [deferredKeyword, displayGroups]);
    const activeMatchIndex = useMemo(() => {
        if (matchState.matchCount < 1) {
            return -1;
        }
        const index = matchState.matches.findIndex(match => match.id === activeMatchId);
        return index >= 0 ? index : 0;
    }, [activeMatchId, matchState]);
    const activeLineKey = activeMatchIndex >= 0
        ? matchState.matches[activeMatchIndex].lineKey
        : null;

    useEffect(() => {
        setActiveMatchId(null);
    }, [deferredKeyword]);

    useEffect(() => {
        if (activeLineKey === null) {
            return;
        }
        lineRefs.current.get(activeLineKey)?.scrollIntoView?.({ block: 'nearest' });
    }, [activeLineKey, deferredKeyword, order]);

    const moveMatch = (offset: number): void => {
        if (matchState.matchCount < 1) {
            return;
        }
        const nextIndex = (activeMatchIndex + offset + matchState.matchCount) % matchState.matchCount;
        setActiveMatchId(matchState.matches[nextIndex].id);
    };

    const handleSearchKeyDown = (event: React.KeyboardEvent<HTMLInputElement>): void => {
        event.stopPropagation();
        if (isComposingRef.current || event.nativeEvent.isComposing) {
            return;
        }
        if (event.key === 'Enter') {
            event.preventDefault();
            moveMatch(event.shiftKey ? -1 : 1);
        } else if (event.key === 'Escape') {
            event.preventDefault();
            setKeyword('');
        }
    };
    const nextOrder = order === 'forward' ? 'reverse' : 'forward';
    const currentOrderLabel = t(order === 'forward' ? 'forwardOrder' : 'reverseOrder');
    const switchOrderLabel = t(nextOrder === 'forward' ? 'switchToForwardOrder' : 'switchToReverseOrder');

    return <Viewer>
        <Toolbar onClick={(event): void => event.stopPropagation()}>
            <ViewerTitle>{t('callStackTitle')}</ViewerTitle>
            <SearchBox role="search" aria-label={t('findCallStack')}>
                <SearchOutlined aria-hidden="true" />
                <input
                    type="search"
                    value={keyword}
                    aria-label={t('findCallStack')}
                    placeholder={t('findCallStackPlaceholder')}
                    onChange={(event): void => setKeyword(event.target.value)}
                    onKeyDown={handleSearchKeyDown}
                    onKeyUp={(event): void => event.stopPropagation()}
                    onCompositionStart={(event): void => {
                        isComposingRef.current = true;
                        event.stopPropagation();
                    }}
                    onCompositionEnd={(event): void => {
                        isComposingRef.current = false;
                        event.stopPropagation();
                    }}
                />
                {hasKeyword
                    ? <MatchCount aria-live="polite">
                        {t('callStackMatchPosition', {
                            current: activeMatchIndex >= 0 ? activeMatchIndex + 1 : 0,
                            total: matchState.matchCount,
                        })}
                    </MatchCount>
                    : <></>}
                <SearchActionButton
                    type="button"
                    disabled={matchState.matchCount < 1}
                    aria-label={t('previousCallStackMatch')}
                    onClick={(): void => moveMatch(-1)}
                ><UpOutlined /></SearchActionButton>
                <SearchActionButton
                    type="button"
                    disabled={matchState.matchCount < 1}
                    aria-label={t('nextCallStackMatch')}
                    onClick={(): void => moveMatch(1)}
                ><DownOutlined /></SearchActionButton>
                {keyword.length > 0
                    ? <ClearSearchButton
                        type="button"
                        aria-label={t('clearCallStackSearch')}
                        onClick={(): void => setKeyword('')}
                    ><CloseCircleFilled /></ClearSearchButton>
                    : <></>}
            </SearchBox>
            <OrderButton
                type="button"
                aria-label={switchOrderLabel}
                aria-pressed={order === 'reverse'}
                title={switchOrderLabel}
                data-order={order}
                onClick={(): void => setOrder(nextOrder)}
            >
                {order === 'forward'
                    ? <SortAscendingOutlined aria-hidden="true" />
                    : <SortDescendingOutlined aria-hidden="true" />}
                {currentOrderLabel}
            </OrderButton>
        </Toolbar>
        <Groups>
            {displayGroups.length > 0
                ? displayGroups.map(group => <Group key={group.key}>
                    {displayGroups.length > 1 ? <GroupTitle>{group.label}</GroupTitle> : <></>}
                    {group.lines.map(line => {
                        const lineKey = getLineKey(group.key, line.originalLineNumber);
                        const matchInfo = matchState.byLine.get(lineKey) ?? { firstMatchIndex: 0, count: 0 };
                        return <Line
                            key={lineKey}
                            ref={(element): void => {
                                if (element === null) {
                                    lineRefs.current.delete(lineKey);
                                } else {
                                    lineRefs.current.set(lineKey, element);
                                }
                            }}
                            data-testid="callStackLine"
                            data-line-number={line.displayLineNumber}
                            data-original-line-number={line.originalLineNumber}
                            data-match-count={matchInfo.count}
                        >
                            <LineNumber>{line.displayLineNumber}</LineNumber>
                            <LineText><HighlightedLine
                                text={line.text}
                                keyword={deferredKeyword}
                                firstMatchIndex={matchInfo.firstMatchIndex}
                                activeMatchIndex={activeMatchIndex}
                            /></LineText>
                        </Line>;
                    })}
                </Group>)
                : <NoResult role="status">{t('noCallStackData')}</NoResult>}
        </Groups>
    </Viewer>;
};
