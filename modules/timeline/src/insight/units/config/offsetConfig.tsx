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
import type { InputRef } from 'antd';
import React, { useEffect, useRef, useState } from 'react';
import type { ChangeEvent } from 'react';
import { observer } from 'mobx-react-lite';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { Tooltip } from '@insight/lib/components';
import { StyledInput } from '../../../components/base/StyledInput';
import { CustomButton } from '../../../components/base/StyledButton';
import type { Session } from '../../../entity/session';
import type { CardMetaData } from '../../../entity/data';
import { getCardSideOffset, type OffsetSide } from '../offset';
import type { SvgType } from '../../../components/base/rc-table/types';
import { ReactComponent as AlignStartIcon } from '../../../assets/images/timeline/ic_align_start.svg';

const AlignIcon = AlignStartIcon as SvgType;
const MAX_OFFSET_TIME = 30 * 24 * 60 * 60 * 1000_000_000;
const defaultBorderColor = '#838383FF';
const inputBorderColor = '#1890ff';
const invalidBorderColor = '#C61E37FF';
const inputBorderShadow = '0 0 0 2px rgba(24, 144, 255, 0.2)';
const invalidBorderShadow = '0 0 0 2px rgba(255, 0, 0, 0.2)';

const OffsetButton = styled.div`
    color: ${(props): string => props.theme.primaryColor};
    cursor: pointer;
    min-width: 44px;
    text-align: center;
    line-height: 20px;
`;

const OffsetIndicatorWrapper = styled.div`
    min-width: 44px;
    padding-right: 40px;
    height: 20px;
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
`;

const OffsetIndicatorDot = styled.div`
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background-color: ${(props): string => props.theme.primaryColor || '#1890ff'};
`;

const InputContainer = styled.div`
    padding: 5px 10px;
    color: ${(props): string => props.theme.fontColor};
`;

const InputRow = styled.div`
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
    min-width: 330px;
    & + & {
        margin-top: 8px;
    }
    > label {
        width: 110px;
        flex-shrink: 0;
    }
    .ant-input-disabled {
        background-color: ${(props): string => props.theme.templateBackgroundColor};
        border: none;
    }
`;

interface OffsetInputRowProps {
    session: Session;
    cardId: string;
    side: OffsetSide;
    alignStartTimestamp?: number;
}

function isValidOffset(value: string): boolean {
    const offset = Number(value);
    return value.trim() !== '' && Number.isFinite(offset) && offset >= -MAX_OFFSET_TIME && offset <= MAX_OFFSET_TIME;
}

const OffsetInputRow = observer(({ session, cardId, side, alignStartTimestamp }: OffsetInputRowProps): JSX.Element => {
    const currentOffset = getCardSideOffset(session, cardId, side);
    const [value, setValue] = useState(String(currentOffset));
    const [invalid, setInvalid] = useState(false);
    const inputRef = useRef<InputRef>(null);
    const { t } = useTranslation();
    const label = side === 'host' ? 'Host Offset' : 'Device Offset';

    useEffect(() => {
        setValue(String(currentOffset));
    }, [currentOffset]);

    const resetStyle = (input: HTMLInputElement): void => {
        input.style.borderColor = defaultBorderColor;
        input.style.boxShadow = 'none';
    };

    const commit = (input: HTMLInputElement): void => {
        if (!isValidOffset(input.value)) {
            setValue(String(currentOffset));
            setInvalid(false);
            resetStyle(input);
            return;
        }
        const nextOffset = Number(input.value);
        if (nextOffset !== currentOffset) {
            session.setCardSideOffset(cardId, side, nextOffset);
            session.setDomainWithoutHistory({
                domainStart: 0,
                domainEnd: session.endTimeAll ?? session.domain.defaultDuration,
            });
            session.renderTrigger = !session.renderTrigger;
        }
        setInvalid(false);
        resetStyle(input);
    };

    const onChange = (event: ChangeEvent<HTMLInputElement>): void => {
        setValue(event.target.value);
        const valid = isValidOffset(event.target.value);
        setInvalid(!valid);
        event.target.style.borderColor = valid ? inputBorderColor : invalidBorderColor;
        event.target.style.boxShadow = valid ? inputBorderShadow : invalidBorderShadow;
    };

    const alignToStart = (): void => {
        if (alignStartTimestamp === undefined) {
            return;
        }
        setValue(String(alignStartTimestamp));
        if (alignStartTimestamp !== currentOffset) {
            session.setCardSideOffset(cardId, side, alignStartTimestamp);
            session.setDomainWithoutHistory({
                domainStart: 0,
                domainEnd: session.endTimeAll ?? session.domain.defaultDuration,
            });
            session.renderTrigger = !session.renderTrigger;
        }
        inputRef.current?.focus();
    };

    return <InputRow>
        <label htmlFor={`${cardId}-${side}-offset`}>{t(label, { ns: 'timeline' })}(ns):</label>
        <div>
            <StyledInput
                id={`${cardId}-${side}-offset`}
                aria-label={label}
                minwidth={20}
                height={18}
                width={155}
                isshow={1}
                value={value}
                disabled={session.phase === 'analyzing'}
                ref={inputRef}
                maxLength={500}
                onChange={onChange}
                onBlur={(event): void => commit(event.target)}
                onFocus={(event): void => {
                    event.target.style.borderColor = inputBorderColor;
                    event.target.style.boxShadow = inputBorderShadow;
                }}
                onPressEnter={(event): void => commit(event.target as HTMLInputElement)}
                allowClear
            />
            {invalid && <div>{t('headerButtonTooltip:TimeStampOffset')}</div>}
        </div>
        <CustomButton
            aria-label={`Align ${side === 'host' ? 'Host' : 'Device'} to Start`}
            tooltip={t('Align to Start', { ns: 'timeline' })}
            icon={AlignIcon}
            type="primary"
            disabled={session.phase === 'analyzing'}
            onClick={alignToStart}
        />
    </InputRow>;
});

interface CardOffsetConfigProps {
    session: Session;
    metadata: CardMetaData;
    onClick?: () => void;
    isHovered?: boolean;
    isSelected?: boolean;
}

export const CardOffsetConfig = observer(({
    session,
    metadata,
    onClick,
    isHovered,
    isSelected,
}: CardOffsetConfigProps): JSX.Element | null => {
    const hostOffset = getCardSideOffset(session, metadata.cardId, 'host');
    const deviceOffset = getCardSideOffset(session, metadata.cardId, 'device');
    const hasOffset = hostOffset !== 0 || deviceOffset !== 0;
    const { t } = useTranslation();
    const cardUnit = session.units.find(unit => (unit.metadata as CardMetaData)?.cardId === metadata.cardId);
    const alignStartTimestamp = cardUnit?.alignStartTimestamp;

    if (!hasOffset && !isHovered && !isSelected) {
        return null;
    }

    return <Tooltip
        trigger="click"
        placement="bottom"
        title={<InputContainer>
            <OffsetInputRow session={session} cardId={metadata.cardId} side="host" alignStartTimestamp={alignStartTimestamp} />
            <OffsetInputRow session={session} cardId={metadata.cardId} side="device" alignStartTimestamp={alignStartTimestamp} />
        </InputContainer>}
        overlayInnerStyle={{ borderRadius: 2 }}
    >
        {!isHovered && !isSelected && hasOffset
            ? <OffsetIndicatorWrapper data-testid="offset-btn" onClick={onClick}>
                <OffsetIndicatorDot title={t('Offset set', { ns: 'timeline' })} />
            </OffsetIndicatorWrapper>
            : <OffsetButton data-testid="offset-btn" onClick={onClick}>{t('Offset', { ns: 'timeline' })}</OffsetButton>}
    </Tooltip>;
});

export const cardOffsetConfig = (
    session: Session,
    metadata: CardMetaData,
    onClick?: () => void,
    isHovered?: boolean,
    isSelected?: boolean,
): JSX.Element | null => <CardOffsetConfig
    session={session}
    metadata={metadata}
    onClick={onClick}
    isHovered={isHovered}
    isSelected={isSelected}
/>;
