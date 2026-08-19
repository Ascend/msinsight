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
import { useRef } from 'react';
import { ArrowUpOutlined, StopOutlined } from '@ant-design/icons';
import type { TFunction } from 'i18next';
import { useTranslation } from 'react-i18next';
import type { AvailableCommand, AvailableSkill, ConfigOption, ConfigOptionValue } from '../types';
import { useChatState } from '../hooks/useChatState';
import { AgentSelect } from './AgentSelect';

const Container = styled.div`
    display: grid;
    gap: 8px;
    padding: 10px 16px 16px;
    background: ${(props): string => props.theme.bgColor};

    .composer-box {
        height: 124px;
        display: grid;
        grid-template-rows: minmax(0, 1fr) auto;
        gap: 10px;
        border: 1px solid transparent;
        border-radius: 22px;
        padding: 12px 14px 12px;
        background:
            linear-gradient(${(props): string => props.theme.bgColor}, ${(props): string => props.theme.bgColor}) padding-box,
            linear-gradient(90deg, rgba(46, 83, 250, 1), rgba(123, 37, 244, 1)) border-box;
        box-shadow: 0 2px 8px rgba(91, 83, 255, 0.06);
    }

    .composer-box.has-attachments {
        height: auto;
        min-height: 180px;
        grid-template-rows: minmax(40px, 1fr) auto auto;
    }

    .attachments {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
    }

    .attachment {
        position: relative;
        min-width: 0;
        display: grid;
        padding: 0;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 0;
        cursor: pointer;
    }

    .attachment:hover {
        border-color: ${(props): string => props.theme.primaryColor};
    }

    .attachment img {
        width: 44px;
        height: 44px;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        object-fit: cover;
    }

    .attachment-remove {
        position: absolute;
        top: -6px;
        right: -6px;
        width: 16px;
        height: 16px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        line-height: 1;
    }

    .queue-preview {
        display: grid;
        overflow: hidden;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        background: ${(props): string => props.theme.bgColor};
    }

    .queue-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding: 7px 10px;
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        font-weight: 700;
    }

    .queue-title {
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .queue-clear {
        border: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        font-weight: 700;
        cursor: pointer;
    }

    .queue-item {
        min-width: 0;
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 7px 10px;
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }

    .queue-item:last-child {
        border-bottom: 0;
    }

    .queue-index {
        width: 6px;
        height: 6px;
        flex: 0 0 6px;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: ${(props): string => props.theme.primaryColor};
    }

    .queue-text {
        flex: 1 1 auto;
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .queue-remove {
        flex: 0 0 auto;
        border: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorSecondary};
        cursor: pointer;
    }

    .queue-remove:hover,
    .queue-clear:hover {
        color: ${(props): string => props.theme.primaryColor};
    }

    textarea {
        width: 100%;
        height: 100%;
        min-height: 0;
        resize: none;
        border: 0;
        padding: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        font: inherit;
        font-size: 14px;
        outline: none;
    }

    textarea::placeholder {
        color: ${(props): string => props.theme.textColorPlaceholder};
    }

    .actions {
        display: flex;
        align-items: center;
        align-self: end;
        gap: 12px;
        min-width: 0;
    }

    .command-wrap {
        position: relative;
        min-height: 0;
    }

    .command-menu {
        position: absolute;
        left: 0;
        right: auto;
        bottom: calc(100% + 8px);
        z-index: 10;
        width: min(440px, 100%);
        max-height: 220px;
        overflow: auto;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        background: ${(props): string => props.theme.bgColorLight};
        box-shadow: 0 10px 30px rgba(0, 0, 0, 0.18);
    }

    .command-title {
        padding: 7px 10px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        font-weight: 700;
    }

    .command-item {
        width: 100%;
        min-width: 0;
        display: grid;
        gap: 2px;
        border: 0;
        padding: 7px 10px;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        text-align: left;
        cursor: pointer;
    }

    .command-item:hover,
    .command-item.active {
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .command-name {
        font-size: 13px;
        font-weight: 700;
    }

    .command-description {
        overflow: hidden;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .model-picker {
        flex: 0 1 auto;
        min-width: 0;
        display: flex;
        align-items: center;
        gap: 5px;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 13px;
    }

    .config-picker {
        width: fit-content;
        max-width: 38vw;
        flex: 0 1 auto;
    }

    .config-picker .agent-select-label {
        font-size: 13px;
        line-height: 20px;
    }

    .shortcut-hint {
        flex: 1 1 auto;
    }

    .send-button {
        width: 30px;
        height: 30px;
        flex: 0 0 30px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: 50%;
        cursor: pointer;
    }

    .send-button {
        background: ${(props): string => props.theme.primaryColor};
        color: #ffffff;
        font-size: 17px;
        transition: transform 0.18s ease, background 0.18s ease;
    }

    .send-button:hover {
        background: ${(props): string => props.theme.primaryColorHover};
        transform: translateY(-1px);
    }

    .send-button:disabled {
        cursor: not-allowed;
        opacity: 0.55;
        transform: none;
    }

`;

export const Composer = (): JSX.Element => {
    const isComposingRef = useRef(false);
    const { t } = useTranslation('insightWebAgent');
    const { addImages, availableCommands, availableSkills, cancelMessage, clearQueuedPrompts, configOptions, images, input, pendingPrompt, queuedCount, queuedPrompts, removeImage, removeQueuedPrompt, sendMessage, setInput, setMode, setModel } = useChatState();
    const modelConfig = getModelConfig(configOptions);
    const modelOptions = flattenConfigValues(modelConfig?.options ?? []);
    const modeConfig = getModeConfig(configOptions);
    const modeOptions = flattenConfigValues(modeConfig?.options ?? []);
    const modelPicker = createConfigPicker(modelConfig, modelOptions, pendingPrompt, setModel);
    const modePicker = createConfigPicker(modeConfig, modeOptions, pendingPrompt, setMode);
    const commandQuery = getCommandQuery(input);
    const commandMatches = getCompletionMatches(availableCommands, availableSkills, commandQuery);
    const showCommandMenu = commandQuery !== undefined && commandMatches.length > 0;
    const insertCompletion = (item: CompletionItem): void => {
        setInput(`/${item.name} `);
    };
    const submitOrCancel = (): void => {
        if (pendingPrompt) {
            cancelMessage();
            return;
        }
        sendMessage();
    };

    const handleKeyDown = (event: any) => {
        const isPlainEnter = event.key === 'Enter' && !event.shiftKey && !event.ctrlKey && !event.metaKey;
        const isComposing = isComposingRef.current || event.nativeEvent?.isComposing || event.isComposing;
        if (isComposing && event.key === 'Enter') return;
        if (showCommandMenu && (event.key === 'Tab' || isPlainEnter)) {
            event.preventDefault();
            insertCompletion(commandMatches[0]);
            return;
        }
        if (event.key === 'Escape' && pendingPrompt) {
            event.preventDefault();
            cancelMessage();
            return;
        }
        if (isPlainEnter) {
            event.preventDefault();
            sendMessage();
        }
    }

    const handlePaste = async (event: any): Promise<void> => {
        const files = getPastedImageFiles(event.clipboardData);
        if (files.length) {
            event.preventDefault();
            addImages(await readImageFiles(files));
        }
    }

    return (
        <Container>
            {queuedPrompts.length ? (
                <div className="queue-preview">
                    <div className="queue-header">
                        <span className="queue-title">{t('queuedMessage', { count: queuedCount })}</span>
                        <button className="queue-clear" onClick={clearQueuedPrompts} type="button">{t('clearAll')}</button>
                    </div>
                    {queuedPrompts.map((prompt, index) => (
                        <div className="queue-item" key={`${index}-${prompt.text}-${prompt.images.length}`}>
                            <span className="queue-index" />
                            <span className="queue-text">{getQueuedPromptPreview(prompt, t)}</span>
                            <button className="queue-remove" onClick={() => removeQueuedPrompt(index)} title={t('remove')} type="button">×</button>
                        </div>
                    ))}
                </div>
            ) : null}
            <div className={`composer-box${images.length ? ' has-attachments' : ''}`}>
                <div className="command-wrap">
                    {showCommandMenu ? (
                        <div className="command-menu">
                            <div className="command-title">{t('agentCommands')}</div>
                            {commandMatches.map((item, index) => (
                                <button className={`command-item${index === 0 ? ' active' : ''}`} key={`${item.kind}-${item.name}`} onClick={() => insertCompletion(item)} type="button">
                                    <span className="command-name">{item.name}</span>
                                    {item.description ? <span className="command-description">{item.kind === 'skill' ? t('skillDescription', { description: item.description }) : item.description}</span> : null}
                                </button>
                            ))}
                        </div>
                    ) : null}
                    <textarea
                        onChange={(event) => setInput(event.target.value)}
                        onCompositionEnd={() => { isComposingRef.current = false; }}
                        onCompositionStart={() => { isComposingRef.current = true; }}
                        onKeyDown={handleKeyDown}
                        onPaste={handlePaste}
                        placeholder={t('newMessagePlaceholder')}
                        rows={3}
                        value={input}
                    />
                </div>
                {images.length ? (
                    <div className="attachments">
                        {images.map((image) => (
                            <button className="attachment" key={image.id} onClick={() => removeImage(image.id)} type="button">
                                <img alt={image.name} src={`data:${image.mimeType};base64,${image.data}`} />
                                <span className="attachment-remove">×</span>
                            </button>
                        ))}
                    </div>
                ) : null}
                <div className="actions">
                    {modePicker ? <div className="model-picker">{modePicker}</div> : null}
                    <div className="model-picker">
                        {modelPicker}
                    </div>
                    <span className="shortcut-hint" />
                    <button
                        aria-label={pendingPrompt ? t('cancel') : t('send')}
                        className="send-button"
                        disabled={!pendingPrompt && !input.trim() && !images.length}
                        onClick={submitOrCancel}
                        title={pendingPrompt ? t('cancel') : t('send')}
                        type="button"
                    >
                        {pendingPrompt ? <StopOutlined /> : <ArrowUpOutlined />}
                    </button>
                </div>
            </div>
        </Container>
    );
};

const getQueuedPromptPreview = (prompt: { text: string; images: unknown[] }, t: TFunction): string => {
    const text = prompt.text.trim().replace(/\s+/g, ' ');
    if (text && prompt.images.length) return `${text} (${t('imageAttachment', { count: prompt.images.length })})`;
    if (text) return text;
    return prompt.images.length === 1 ? t('image') : t('images', { count: prompt.images.length });
};

interface CompletionItem {
    kind: 'command' | 'skill';
    name: string;
    description?: string;
}

const getCommandQuery = (input: string): string | undefined => {
    const match = input.match(/^\/(\S*)$/);
    return match ? match[1].toLowerCase() : undefined;
};

const getCompletionMatches = (commands: AvailableCommand[], skills: AvailableSkill[], query: string | undefined): CompletionItem[] => {
    if (query === undefined) return [];
    return [
        ...skills.map((skill) => ({ kind: 'skill' as const, ...skill })),
        ...commands.map((command) => ({ kind: 'command' as const, ...command })),
    ]
        .filter((item) => item.name.toLowerCase().includes(query))
        .slice(0, 8);
};

const readImageFiles = async (files: File[]): Promise<Array<{ id: string; name: string; mimeType: string; data: string }>> => {
    const images = files.filter((file) => file.type.startsWith('image/'));
    return Promise.all(images.map(readImageFile));
};

const getPastedImageFiles = (clipboardData: DataTransfer): File[] => {
    const itemFiles = Array.from(clipboardData.items)
        .filter((item) => item.kind === 'file' && item.type.startsWith('image/'))
        .map((item) => item.getAsFile())
        .filter((file): file is File => Boolean(file));
    if (itemFiles.length) return itemFiles;
    return Array.from(clipboardData.files).filter((file) => file.type.startsWith('image/'));
};

const readImageFile = (file: File): Promise<{ id: string; name: string; mimeType: string; data: string }> => {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onerror = () => reject(reader.error ?? new Error('failed to read image'));
        reader.onload = () => {
            const result = String(reader.result ?? '');
            resolve({
                id: crypto.randomUUID(),
                name: file.name || 'image',
                mimeType: file.type || 'image/png',
                data: result.includes(',') ? result.slice(result.indexOf(',') + 1) : result,
            });
        };
        reader.readAsDataURL(file);
    });
};

const createConfigPicker = (
    config: ConfigOption | undefined,
    options: ConfigOptionValue[],
    disabled: boolean,
    onChange: (value: string) => Promise<void>,
): JSX.Element | null => {
    if (!config?.currentValue || !options.length) return null;
    return (
        <AgentSelect
            className="config-picker"
            compact
            disabled={disabled}
            onChange={(value) => { void onChange(value); }}
            options={options.map((option) => ({ value: option.value, label: option.name || option.value }))}
            placement="top"
            value={config.currentValue}
        />
    );
};

const getModelConfig = (configOptions: ConfigOption[]): ConfigOption | undefined => {
    return configOptions.find((option) => option.category === 'model')
        ?? configOptions.find((option) => option.id.toLowerCase().includes('model'));
};

const getModeConfig = (configOptions: ConfigOption[]): ConfigOption | undefined => {
    return configOptions.find((option) => option.category === 'mode')
        ?? configOptions.find((option) => option.id.toLowerCase().includes('mode'));
};

const flattenConfigValues = (options: ConfigOptionValue[]): ConfigOptionValue[] => {
    return options.flatMap((option) => {
        if (Array.isArray(option.options)) return flattenConfigValues(option.options);
        return option.value ? [option] : [];
    });
};
