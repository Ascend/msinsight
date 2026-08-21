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
import styled from '@emotion/styled';
import { useEffect, useId, useLayoutEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import type { CSSProperties, KeyboardEvent, ReactNode } from 'react';
import arrowDownIcon from '../icons/arrow-down.svg';
import checkIcon from '../icons/check.svg';

export interface AgentSelectOption {
    value: string;
    label: ReactNode;
    title?: string;
    icon?: ReactNode;
    disabled?: boolean;
}

interface AgentSelectProps {
    value?: string;
    options: AgentSelectOption[];
    onChange: (value: string) => void;
    title?: ReactNode;
    footer?: ReactNode;
    placeholder?: ReactNode;
    className?: string;
    disabled?: boolean;
    compact?: boolean;
    placement?: 'top' | 'bottom';
}

interface DropdownPosition {
    top: number;
    left: number;
    maxOptionsHeight: number;
}

const DROPDOWN_GAP = 4;
const VIEWPORT_PADDING = 8;
const DEFAULT_OPTIONS_MAX_HEIGHT = 240;
const MIN_OPTIONS_HEIGHT = 56;

const Container = styled.div`
    position: relative;
    min-width: 0;

    .agent-select-trigger {
        width: 100%;
        height: 30px;
        min-width: 0;
        display: flex;
        align-items: center;
        gap: 8px;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 4px;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        text-align: left;
        cursor: pointer;
    }

    .agent-select-trigger:hover,
    .agent-select-trigger.expanded {
        background: ${(props): string => props.theme.mode === 'dark'
            ? props.theme.agentWelcomeCardBackgroundColor
            : props.theme.bgColorLight};
    }

    .agent-select-trigger:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }

    .agent-select-trigger:disabled {
        color: ${(props): string => props.theme.textColorDisabled};
        cursor: not-allowed;
    }

    .agent-select-trigger.compact {
        height: 28px;
        gap: 4px;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 2px 4px;
    }

    .agent-select-trigger-icon,
    .agent-select-option-icon {
        width: 20px;
        height: 20px;
        flex: 0 0 20px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
    }

    .agent-select-option-icon {
        width: 16px;
        height: 16px;
        flex-basis: 16px;
    }

    .agent-select-trigger-icon img,
    .agent-select-option-icon img {
        width: 100%;
        height: 100%;
        object-fit: contain;
    }

    .agent-select-label {
        min-width: 0;
        flex: 1 1 auto;
        overflow: hidden;
        color: inherit;
        font-size: 14px;
        line-height: 22px;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .agent-select-arrow {
        width: 16px;
        height: 16px;
        flex: 0 0 16px;
        opacity: 1;
        transform: rotate(0deg);
        transition: transform 0.15s ease;
        filter: ${(props): string => props.theme.mode === 'dark' ? 'invert(1)' : 'none'};
    }

    .agent-select-trigger.expanded .agent-select-arrow {
        transform: rotate(180deg);
    }

    .agent-select-dropdown {
        width: 240px;
        position: fixed;
        z-index: 1000;
        display: grid;
        gap: 0;
        border: 0;
        border-radius: 12px;
        padding: 8px 4px 12px;
        background: ${(props): string => props.theme.mode === 'dark' ? props.theme.bgColorLight : props.theme.bgColor};
        box-shadow: ${(props): string => props.theme.mode === 'dark'
            ? props.theme.boxShadow
            : '0 12px 32px 4px rgba(0, 0, 0, 0.06), 0 8px 20px rgba(0, 0, 0, 0.12)'};
    }

    .agent-select-title {
        margin-bottom: 2px;
        padding: 4px 8px;
        border-radius: 4px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        line-height: 20px;
    }

    .agent-select-options {
        display: grid;
        gap: 2px;
        max-height: min(240px, calc(100vh - 120px));
        overflow-x: hidden;
        overflow-y: auto;
        overscroll-behavior: contain;
        scrollbar-width: thin;
        scrollbar-color: ${(props): string => props.theme.scrollbarColor} transparent;
    }

    .agent-select-options::-webkit-scrollbar {
        width: 6px;
    }

    .agent-select-options::-webkit-scrollbar-thumb {
        border-radius: 3px;
        background: ${(props): string => props.theme.scrollbarColor};
    }

    .agent-select-options::-webkit-scrollbar-track {
        background: transparent;
    }

    .agent-select-option {
        width: 100%;
        height: 28px;
        min-width: 0;
        display: flex;
        align-items: center;
        gap: 4px;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 4px 8px;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        text-align: left;
        cursor: pointer;
    }

    .agent-select-option:hover,
    .agent-select-option.focused {
        background: ${(props): string => props.theme.agentWelcomeCardBackgroundColor};
    }

    .agent-select-option.selected {
        background: ${(props): string => props.theme.mode === 'dark'
            ? 'rgba(82, 145, 255, 0.16)'
            : 'rgba(46, 83, 250, 0.12)'};
    }

    .agent-select-option:disabled {
        color: ${(props): string => props.theme.textColorDisabled};
        cursor: not-allowed;
    }

    .agent-select-option-check {
        width: 16px;
        height: 16px;
        flex: 0 0 16px;
        margin-left: auto;
        filter: ${(props): string => props.theme.mode === 'dark'
            ? 'invert(49%) sepia(98%) saturate(3390%) hue-rotate(199deg) brightness(103%) contrast(101%)'
            : 'invert(42%) sepia(96%) saturate(4111%) hue-rotate(213deg) brightness(101%) contrast(102%)'};
    }

    .agent-select-footer {
        margin-top: 8px;
        padding: 0 8px;
    }

    .agent-select-footer > *,
    .agent-select-footer > * > *,
    .agent-select-footer > * > * > * {
        width: 100%;
    }
`;

export const AgentSelect = ({
    value,
    options,
    onChange,
    title,
    footer,
    placeholder,
    className,
    disabled = false,
    compact = false,
    placement = 'bottom',
}: AgentSelectProps): JSX.Element => {
    const [open, setOpen] = useState(false);
    const [focusedIndex, setFocusedIndex] = useState(-1);
    const [dropdownPosition, setDropdownPosition] = useState<DropdownPosition | null>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const triggerRef = useRef<HTMLButtonElement>(null);
    const dropdownRef = useRef<HTMLDivElement>(null);
    const optionsRef = useRef<HTMLDivElement>(null);
    const listboxId = useId();
    const selectedOption = options.find((option) => option.value === value);
    const selectedTitle = selectedOption?.title
        ?? (typeof selectedOption?.label === 'string' ? selectedOption.label : undefined);

    useEffect(() => {
        if (!open) return;
        const handlePointerDown = (event: PointerEvent): void => {
            const target = event.target as Node;
            if (!containerRef.current?.contains(target) && !dropdownRef.current?.contains(target)) setOpen(false);
        };
        document.addEventListener('pointerdown', handlePointerDown);
        return () => document.removeEventListener('pointerdown', handlePointerDown);
    }, [open]);

    useLayoutEffect(() => {
        if (!open) return;
        let frameId = 0;
        const updatePosition = (): void => {
            cancelAnimationFrame(frameId);
            frameId = requestAnimationFrame(() => {
                const triggerElement = triggerRef.current;
                const dropdownElement = dropdownRef.current;
                const optionsElement = optionsRef.current;
                if (!triggerElement || !dropdownElement || !optionsElement) return;

                const triggerRect = triggerElement.getBoundingClientRect();
                const dropdownWidth = dropdownElement.offsetWidth;
                const chromeHeight = dropdownElement.offsetHeight - optionsElement.offsetHeight;
                const spaceAbove = triggerRect.top - DROPDOWN_GAP - VIEWPORT_PADDING;
                const spaceBelow = window.innerHeight - triggerRect.bottom - DROPDOWN_GAP - VIEWPORT_PADDING;
                const preferredSpace = placement === 'top' ? spaceAbove : spaceBelow;
                const alternateSpace = placement === 'top' ? spaceBelow : spaceAbove;
                const opensOnTop = preferredSpace >= MIN_OPTIONS_HEIGHT + chromeHeight || preferredSpace >= alternateSpace
                    ? placement === 'top'
                    : placement !== 'top';
                const availableHeight = opensOnTop ? spaceAbove : spaceBelow;
                const maxOptionsHeight = Math.max(
                    MIN_OPTIONS_HEIGHT,
                    Math.min(DEFAULT_OPTIONS_MAX_HEIGHT, availableHeight - chromeHeight),
                );
                const dropdownHeight = chromeHeight + Math.min(optionsElement.scrollHeight, maxOptionsHeight);
                const idealLeft = triggerRect.left;
                const left = Math.min(
                    Math.max(VIEWPORT_PADDING, idealLeft),
                    Math.max(VIEWPORT_PADDING, window.innerWidth - dropdownWidth - VIEWPORT_PADDING),
                );
                const top = opensOnTop
                    ? Math.max(VIEWPORT_PADDING, triggerRect.top - DROPDOWN_GAP - dropdownHeight)
                    : Math.min(window.innerHeight - VIEWPORT_PADDING - dropdownHeight, triggerRect.bottom + DROPDOWN_GAP);

                setDropdownPosition((current) => current
                    && current.top === top
                    && current.left === left
                    && current.maxOptionsHeight === maxOptionsHeight
                    ? current
                    : { top, left, maxOptionsHeight });
            });
        };

        updatePosition();
        window.addEventListener('resize', updatePosition);
        window.addEventListener('scroll', updatePosition, true);
        const resizeObserver = typeof ResizeObserver === 'undefined'
            ? undefined
            : new ResizeObserver(updatePosition);
        if (triggerRef.current) resizeObserver?.observe(triggerRef.current);
        if (dropdownRef.current) resizeObserver?.observe(dropdownRef.current);

        return () => {
            cancelAnimationFrame(frameId);
            window.removeEventListener('resize', updatePosition);
            window.removeEventListener('scroll', updatePosition, true);
            resizeObserver?.disconnect();
        };
    }, [footer, open, options.length, placement, title]);

    useEffect(() => {
        if (!open || !dropdownPosition) return;
        const frameId = requestAnimationFrame(() => {
            const optionsElement = optionsRef.current;
            const selectedElement = optionsRef.current?.querySelector<HTMLElement>('[aria-selected="true"]');
            if (!optionsElement || !selectedElement) return;
            const optionTop = selectedElement.offsetTop;
            const optionBottom = optionTop + selectedElement.offsetHeight;
            if (optionTop < optionsElement.scrollTop) optionsElement.scrollTop = optionTop;
            if (optionBottom > optionsElement.scrollTop + optionsElement.clientHeight) {
                optionsElement.scrollTop = optionBottom - optionsElement.clientHeight;
            }
        });
        return () => cancelAnimationFrame(frameId);
    }, [dropdownPosition, open, value]);

    const closeAndSelect = (option: AgentSelectOption): void => {
        if (option.disabled) return;
        onChange(option.value);
        setOpen(false);
    };

    const moveFocus = (offset: number): void => {
        if (!options.length) return;
        let nextIndex = focusedIndex;
        for (let count = 0; count < options.length; count += 1) {
            nextIndex = (nextIndex + offset + options.length) % options.length;
            if (!options[nextIndex].disabled) {
                setFocusedIndex(nextIndex);
                return;
            }
        }
    };

    const handleKeyDown = (event: KeyboardEvent<HTMLButtonElement>): void => {
        if (event.key === 'Escape') {
            setOpen(false);
            return;
        }
        if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            event.preventDefault();
            if (!open) setOpen(true);
            moveFocus(event.key === 'ArrowDown' ? 1 : -1);
            return;
        }
        if (event.key === 'Enter' && open && focusedIndex >= 0) {
            event.preventDefault();
            closeAndSelect(options[focusedIndex]);
        }
    };

    const dropdownStyle: CSSProperties = dropdownPosition
        ? { top: dropdownPosition.top, left: dropdownPosition.left }
        : { top: 0, left: 0, visibility: 'hidden' };

    const dropdown = open ? createPortal(
        <Container>
            <div className="agent-select-dropdown" ref={dropdownRef} style={dropdownStyle}>
                {title ? <div className="agent-select-title">{title}</div> : null}
                <div
                    aria-activedescendant={focusedIndex >= 0 ? `${listboxId}-${focusedIndex}` : undefined}
                    className="agent-select-options"
                    id={listboxId}
                    ref={optionsRef}
                    role="listbox"
                    style={dropdownPosition ? { maxHeight: dropdownPosition.maxOptionsHeight } : undefined}
                >
                    {options.map((option, index) => {
                        const selected = option.value === value;
                        return (
                            <button
                                aria-selected={selected}
                                className={`agent-select-option ${selected ? 'selected' : ''} ${focusedIndex === index ? 'focused' : ''}`}
                                disabled={option.disabled}
                                id={`${listboxId}-${index}`}
                                key={option.value}
                                onClick={() => closeAndSelect(option)}
                                onMouseEnter={() => setFocusedIndex(index)}
                                role="option"
                                type="button"
                            >
                                {option.icon ? <span className="agent-select-option-icon">{option.icon}</span> : null}
                                <span
                                    className="agent-select-label"
                                    title={option.title ?? (typeof option.label === 'string' ? option.label : undefined)}
                                >
                                    {option.label}
                                </span>
                                {selected ? <img aria-hidden="true" className="agent-select-option-check" src={checkIcon} /> : null}
                            </button>
                        );
                    })}
                </div>
                {footer ? <div className="agent-select-footer" onClick={() => setOpen(false)}>{footer}</div> : null}
            </div>
        </Container>,
        document.body,
    ) : null;

    return (
        <Container className={className} ref={containerRef}>
            <button
                aria-controls={open ? listboxId : undefined}
                aria-expanded={open}
                aria-haspopup="listbox"
                className={`agent-select-trigger ${compact ? 'compact' : ''} ${open ? 'expanded' : ''}`}
                disabled={disabled}
                onClick={() => {
                    setDropdownPosition(null);
                    setOpen((current) => !current);
                    setFocusedIndex(Math.max(0, options.findIndex((option) => option.value === value)));
                }}
                onKeyDown={handleKeyDown}
                ref={triggerRef}
                type="button"
            >
                {selectedOption?.icon ? <span className="agent-select-trigger-icon">{selectedOption.icon}</span> : null}
                <span className="agent-select-label" title={selectedTitle}>{selectedOption?.label ?? placeholder}</span>
                <img aria-hidden="true" className="agent-select-arrow" src={arrowDownIcon} />
            </button>
            {dropdown}
        </Container>
    );
};
