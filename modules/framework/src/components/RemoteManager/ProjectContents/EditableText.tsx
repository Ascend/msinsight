/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import React, { useState, useRef, useEffect } from 'react';
import { Input } from '@insight/lib/components';
import styled from '@emotion/styled';
import type { InputRef } from 'antd';
import { updateProjectName } from '@/utils/Project';
import { HandleSingleDoubleClick } from '@insight/lib/utils';
import type { Session } from '@/entity/session';
import { message } from 'antd';
import { useTranslation } from 'react-i18next';

const Container = styled.div`
  display: flex;
  align-items: center;

  .show {
    display: flex;
    align-items: center;
    overflow: hidden;
    max-width: 100%;
  }

  .text-front {
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
    flex-shrink: 1;
    min-width: 0;
  }

  .text-back {
    white-space: nowrap;
    flex-shrink: 0;
  }

  .hide {
    display: none;
  }
  input {
    width: 100% ;
  }
`;

interface IProps {
    text: string;
    session?: Session;
    projectName?: string;
}

/**
 * 根据容器宽度计算前后分割点
 */
const calculateSplit = (
    text: string,
    containerWidth: number,
    container: HTMLElement,
): { front: string; back: string } => {
    if (!text) return { front: '', back: '' };

    const measureSpan = document.createElement('span');
    measureSpan.style.cssText = 'position:absolute;visibility:hidden;white-space:nowrap;padding:0;margin:0;border:0;';
    const computed = window.getComputedStyle(container);
    measureSpan.style.font = computed.font;
    container.appendChild(measureSpan);

    const measure = (str: string): number => {
        measureSpan.textContent = str;
        return measureSpan.offsetWidth;
    };

    const fullWidth = measure(text);
    if (fullWidth <= containerWidth) {
        container.removeChild(measureSpan);
        return { front: text, back: '' };
    }

    // 后缀约占40%宽度
    const targetBackWidth = containerWidth * 0.4;
    let backLen = 0;
    let low = 0;
    let high = text.length;

    while (low < high) {
        const mid = Math.floor((low + high + 1) / 2);
        const width = measure(text.slice(-mid));
        if (width <= targetBackWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    backLen = low || 1;

    container.removeChild(measureSpan);
    return { front: text.slice(0, text.length - backLen), back: text.slice(-backLen) };
};

function EditableText({ text = '', session, projectName }: IProps): JSX.Element {
    const { t } = useTranslation('framework');
    const inputRef = useRef<InputRef>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const [editing, setEditing] = useState(false);
    const [editText, setEditText] = useState(text);
    const [split, setSplit] = useState({ front: text, back: '' });

    // 监听容器宽度变化
    useEffect(() => {
        const container = containerRef.current;
        if (!container || !text) return;

        let rafId: number | null = null;
        const updateSplit = (): void => {
            if (containerRef.current) {
                setSplit(calculateSplit(text, containerRef.current.offsetWidth, containerRef.current));
            }
        };

        updateSplit();

        const resizeObserver = new ResizeObserver(() => {
            if (rafId) return;
            rafId = requestAnimationFrame(() => {
                rafId = null;
                updateSplit();
            });
        });
        resizeObserver.observe(container);

        return () => {
            resizeObserver.disconnect();
            if (rafId) cancelAnimationFrame(rafId);
        };
    }, [text]);
    // 双击进入编辑
    const handleDoubleClick = (): void => {
        let isSelectBaseline = false;
        if (session?.compareSet) {
            const { compareSet: { baseline, comparison } } = session;
            isSelectBaseline = baseline.filePath.startsWith(projectName as string) || comparison.filePath.startsWith(projectName as string);
        }
        if (!isSelectBaseline) {
            // React不区分单击、双击,为避免单击事件运行，增加额外控制
            HandleSingleDoubleClick.doubleClick(() => {
                enterEditMode();
            }, 'projectName');
        } else {
            HandleSingleDoubleClick.doubleClick(() => {
                message.warning(t('BaselineDataComparisonDataCannotRenamed'));
            }, 'projectName');
            // "基线数据和对比数据不允许重命名",
        }
    };
    const enterEditMode = (): void => {
        setEditText(text);
        setEditing(true);
    };

    // 退出编辑
    const exitEdit = async (): Promise<void> => {
        const trimmedContent = editText.trim();
        if (trimmedContent !== '' && trimmedContent !== text) {
            const success = await updateProjectName(text, editText);
            if (success) {
                setEditing(false);
            }
        } else {
            setEditing(false);
        }
    };

    const blurInput = (): void => {
        inputRef.current?.blur();
    };

    useEffect(() => {
        if (editing) {
            inputRef.current?.focus();
        }
    }, [editing]);

    return <Container ref={containerRef}>
        <div className={`can-right-click ${editing ? 'hide' : 'show'}`} onDoubleClick={handleDoubleClick} title={text}>
            <span className="text-front">{split.front}</span>
            <span className="text-back">{split.back}</span>
        </div>
        <Input className={editing ? 'show' : 'hide'}
            ref={inputRef}
            value={editText}
            onChange={(e): void => { setEditText(e.target.value); }}
            onPressEnter={blurInput}
            onBlur={exitEdit}
            onClick={(e): void => { e.stopPropagation(); }}
        />
    </Container>;
};

export default EditableText;
