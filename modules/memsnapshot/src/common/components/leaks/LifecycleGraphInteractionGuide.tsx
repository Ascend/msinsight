/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React from 'react';
import { createPortal } from 'react-dom';
import styled from '@emotion/styled';
import {
    CloseOutlined,
    DragOutlined,
    ExpandAltOutlined,
    FlagOutlined,
    GroupOutlined,
    OneToOneOutlined,
} from '@ant-design/icons';
import { useTranslation } from 'react-i18next';
import { TimelineFlagIcon } from './TimelineFlagIcon';

const Panel = styled.div`
    position: fixed;
    z-index: 13;
    display: flex;
    flex-direction: column;
    width: min(374px, calc(100vw - 32px));
    color: ${(props): string => props.theme.textColorPrimary};
    background: ${(props): string => props.theme.bgColorLight};
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 4px;
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.12);
    overflow: hidden;
`;

const Header = styled.div`
    z-index: 2;
    display: flex;
    flex: 0 0 auto;
    align-items: center;
    justify-content: space-between;
    min-height: 48px;
    padding: 0 12px 0 16px;
    background: ${(props): string => props.theme.bgColorLight};
    font-size: 14px;
    font-weight: 600;
`;

const CloseButton = styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 24px;
    height: 24px;
    padding: 0;
    color: ${(props): string => props.theme.iconColor};
    background: transparent;
    border: none;
    border-radius: 4px;
    cursor: pointer;

    &:hover { color: ${(props): string => props.theme.primaryColor}; background: ${(props): string => props.theme.bgColorLight}; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; outline-offset: 1px; }
`;

const Body = styled.div`
    min-height: 0;
    padding: 0 16px 14px;
    overflow-y: auto;
`;

const Section = styled.section`
    margin-bottom: 14px;
`;

const Title = styled.div`
    display: flex;
    align-items: center;
    gap: 7px;
    margin-bottom: 4px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 13px;
    font-weight: 600;

    svg { width: 16px; height: 16px; flex: 0 0 auto; color: ${(props): string => props.theme.iconColor}; }
`;
const Description = styled.div`
    margin: 0 0 8px 23px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    line-height: 18px;
`;

const Artwork = styled.div`
    width: calc(100% - 23px);
    height: 96px;
    margin-left: 23px;
    overflow: hidden;
    color: ${(props): string => props.theme.iconColor};
    background: ${(props): string => props.theme.contentBackgroundColor};
    border-radius: 4px;

    svg { display: block; width: 100%; height: 100%; }
    .frame { fill: ${(props): string => props.theme.contentBackgroundColor}; stroke: ${(props): string => props.theme.borderColorLight}; }
    .muted { fill: none; stroke: ${(props): string => props.theme.borderColor}; }
    .primary { fill: none; stroke: ${(props): string => props.theme.primaryColor}; }
    .primary-fill { fill: ${(props): string => props.theme.primaryColor}; color: ${(props): string => props.theme.primaryColor}; }
    .block-a { fill: #57b36a; }
    .block-b { fill: #64a0dd; }
    .block-c { fill: #e9a23b; }
    .surface { fill: ${(props): string => props.theme.contentBackgroundColor}; }
    .secondary-text { fill: ${(props): string => props.theme.textColorSecondary}; font-size: 9px; }
    .primary-text { fill: ${(props): string => props.theme.textColorPrimary}; font-size: 10px; font-weight: 600; }
`;

const ChartContents = ({ x, y, width, height }: { x: number; y: number; width: number; height: number }): JSX.Element => <>
    <path className="block-b" opacity=".9" d={`M${x + 8} ${y + height - 10}H${x + width - 8}V${y + height - 5}H${x + 8}Z`} />
    <path className="block-a" opacity=".9" d={`M${x + 8} ${y + height - 17}H${x + width * 0.72}L${x + width - 8} ${y + height - 10}H${x + 8}Z`} />
    <path className="block-c" opacity=".82" d={`M${x + 8} ${y + height - 23}H${x + width * 0.46}L${x + width * 0.72} ${y + height - 17}H${x + 8}Z`} />
    <path className="primary" d={`M${x + 7} ${y + 15} C${x + width * 0.32} ${y + 9}, ${x + width * 0.58} ${y + 23}, ${x + width - 7} ${y + 13}`} strokeWidth="1.4" />
</>;

const ZoomArtwork = (): JSX.Element => <Artwork data-testid="lifecycleGuideZoomVisual">
    <svg viewBox="0 0 312 96" aria-hidden="true">
        <rect className="frame" x="18" y="12" width="122" height="72" rx="3" />
        <ChartContents x={18} y={12} width={122} height={72} />
        <path className="primary" d="M24 19 L35 30 M24 19 H32 M24 19 V27 M134 77 L123 66 M134 77 H126 M134 77 V69" strokeWidth="1.4" />
        <path className="muted" d="M153 15 V81" strokeDasharray="3 3" />
        <rect className="frame" x="169" y="22" width="125" height="52" rx="3" />
        <ChartContents x={169} y={22} width={125} height={52} />
        <path className="primary" d="M176 48 H190 M176 48 L182 43 M176 48 L182 53 M287 48 H273 M287 48 L281 43 M287 48 L281 53" strokeWidth="1.4" />
    </svg>
</Artwork>;

const PanArtwork = (): JSX.Element => <Artwork data-testid="lifecycleGuidePanVisual">
    <svg viewBox="0 0 312 96" aria-hidden="true">
        <rect className="frame" x="28" y="14" width="256" height="68" rx="3" />
        <ChartContents x={28} y={14} width={256} height={68} />
        <rect className="muted" x="79" y="23" width="104" height="50" rx="2" strokeDasharray="3 3" />
        <rect className="primary" x="123" y="23" width="104" height="50" rx="2" strokeWidth="1.6" />
        <path className="primary" d="M92 47 H213 M92 47 L99 41 M92 47 L99 53 M213 47 L206 41 M213 47 L206 53" strokeWidth="1.5" />
    </svg>
</Artwork>;

const ResetArtwork = (): JSX.Element => <Artwork data-testid="lifecycleGuideResetVisual">
    <svg viewBox="0 0 312 96" aria-hidden="true">
        <defs><clipPath id="lifecycle-reset-preview"><rect x="25" y="24" width="102" height="48" rx="3" /></clipPath></defs>
        <rect className="frame" x="25" y="24" width="102" height="48" rx="3" />
        <g clipPath="url(#lifecycle-reset-preview)"><ChartContents x={2} y={7} width={158} height={76} /></g>
        <path className="muted" d="M139 48 H165 M160 43 L166 48 L160 53" strokeWidth="1.5" />
        <rect className="frame" x="178" y="14" width="110" height="68" rx="3" />
        <ChartContents x={178} y={14} width={110} height={68} />
        <path className="primary" d="M266 24 A12 12 0 1 1 262 48 M266 24 L259 24 M266 24 L266 31" strokeWidth="1.5" />
    </svg>
</Artwork>;

const LayerArtwork = (): JSX.Element => <Artwork data-testid="lifecycleGuideLayerVisual">
    <svg viewBox="0 0 312 96" aria-hidden="true">
        <rect className="frame" x="30" y="13" width="252" height="70" rx="3" />
        <rect className="frame" x="42" y="23" width="148" height="49" rx="2" />
        <ChartContents x={42} y={23} width={148} height={49} />
        <path className="muted" d="M50 50 H183" strokeDasharray="4 3" />
        <g transform="translate(216 22)">
            <rect className="surface muted" width="50" height="15" rx="3" />
            <rect className="surface muted" y="20" width="50" height="15" rx="3" />
            <rect className="surface muted" y="40" width="50" height="15" rx="3" />
            <path className="primary" d="M34 7.5 C38 3.5 44 3.5 48 7.5 C44 11.5 38 11.5 34 7.5 Z M34 27.5 C38 23.5 44 23.5 48 27.5 C44 31.5 38 31.5 34 27.5 Z M34 47.5 C38 43.5 44 43.5 48 47.5 C44 51.5 38 51.5 34 47.5 Z" strokeWidth="1.2" />
            <rect className="block-b" x="6" y="5" width="12" height="5" rx="1" />
            <path className="primary" d="M6 29 H18" strokeWidth="1.5" />
            <g transform="translate(5 42) scale(.8)"><TimelineFlagIcon width="17" height="16" /></g>
        </g>
    </svg>
</Artwork>;

const DifferenceMarkerArtwork = (): JSX.Element => <Artwork data-testid="lifecycleGuideDifferenceMarkerVisual">
    <svg viewBox="0 0 312 96" aria-hidden="true">
        <rect className="frame" x="28" y="12" width="256" height="72" rx="3" />
        <rect className="block-a" x="42" y="58" width="118" height="10" rx="1" opacity="0.82" />
        <rect className="block-b" x="42" y="35" width="151" height="10" rx="1" opacity="0.82" />
        <path className="muted" d="M42 35 H247 M42 68 H247" strokeDasharray="4 3" />
        <rect className="surface" x="244" y="17" width="24" height="62" rx="3" />
        <path className="muted" d="M244 17 V79 M268 17 V79" />
        <path className="primary" d="M250 35 V68" strokeWidth="2" />
        <g className="primary-fill"><g transform="translate(250 27)"><TimelineFlagIcon /></g><g transform="translate(250 60)"><TimelineFlagIcon /></g></g>
        <rect className="surface primary" x="194" y="45" width="51" height="17" rx="8.5" />
        <text className="primary-text" x="219.5" y="56.5" textAnchor="middle">8.2 GB</text>
        <rect className="surface muted" x="70" y="17" width="34" height="15" rx="3" />
        <text className="secondary-text" x="87" y="27.5" textAnchor="middle">K</text>
    </svg>
</Artwork>;

const MarkerManagementArtwork = (): JSX.Element => <Artwork data-testid="lifecycleGuideMarkerManagementVisual">
    <svg viewBox="0 0 312 96" aria-hidden="true">
        <path className="muted" d="M52 14 V82" strokeWidth="1.5" />
        {[22, 48, 74].map((y, index) => <g key={y}>
            <g className="primary-fill" transform={`translate(47 ${y - 8})`}><TimelineFlagIcon /></g>
            <rect className="frame" x="69" y={y - 10} width="214" height="21" rx="3" />
            <text className="primary-text" x="80" y={y + 3}>Flag {index + 1}</text>
            <rect className={index === 1 ? 'block-a' : index === 2 ? 'block-c' : 'block-b'} x="224" y={y - 4} width="13" height="8" rx="1" />
            <path className="muted" d={`M249 ${y} C253 ${y - 4} 259 ${y - 4} 263 ${y} C259 ${y + 4} 253 ${y + 4} 249 ${y} Z`} />
            <path className="muted" d={`M273 ${y - 4} H281 M275 ${y - 4} V${y + 5} H279 V${y - 4}`} />
        </g>)}
    </svg>
</Artwork>;

const useGuidePosition = (anchorElement: HTMLElement | null): { top: number; right: number; maxHeight: number } => {
    const [position, setPosition] = React.useState({ top: 8, right: 48, maxHeight: 620 });
    React.useLayoutEffect(() => {
        const update = (): void => {
            if (anchorElement === null) return;
            const anchor = anchorElement.getBoundingClientRect();
            const panelWidth = Math.min(374, window.innerWidth - 32);
            const maxRight = Math.max(8, window.innerWidth - panelWidth - 8);
            const right = Math.max(8, Math.min(window.innerWidth - anchor.left + 10, maxRight));
            const top = Math.max(8, Math.min(anchor.top, window.innerHeight - 180));
            setPosition({ top, right, maxHeight: Math.max(160, window.innerHeight - top - 8) });
        };
        update();
        window.addEventListener('resize', update);
        window.addEventListener('scroll', update, true);
        return () => {
            window.removeEventListener('resize', update);
            window.removeEventListener('scroll', update, true);
        };
    }, [anchorElement]);
    return position;
};

export const LifecycleGraphInteractionGuide = ({
    anchorElement,
    onClose,
}: {
    anchorElement: HTMLElement | null;
    onClose: () => void;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    const position = useGuidePosition(anchorElement);
    return createPortal(<Panel
        role="dialog"
        aria-label={t('lifecycleGraphGuideTitle')}
        data-testid="lifecycleGraphInteractionGuide"
        data-lifecycle-floating-panel="guide"
        style={position}
    >
        <Header>
            <span>{t('lifecycleGraphGuideTitle')}</span>
            <CloseButton type="button" aria-label={t('closeLifecycleGraphGuide')} onClick={onClose}><CloseOutlined /></CloseButton>
        </Header>
        <Body>
            <Section data-testid="lifecycleGuideSection">
                <Title><ExpandAltOutlined /><span>{t('lifecycleGuideZoomTitle')}</span></Title>
                <Description>{t('lifecycleGuideZoomDescription')}</Description>
                <ZoomArtwork />
            </Section>
            <Section data-testid="lifecycleGuideSection">
                <Title><DragOutlined /><span>{t('lifecycleGuidePanTitle')}</span></Title>
                <Description>{t('lifecycleGuidePanDescription')}</Description>
                <PanArtwork />
            </Section>
            <Section data-testid="lifecycleGuideSection">
                <Title><OneToOneOutlined /><span>{t('lifecycleGuideResetTitle')}</span></Title>
                <Description>{t('lifecycleGuideResetDescription')}</Description>
                <ResetArtwork />
            </Section>
            <Section data-testid="lifecycleGuideSection">
                <Title><GroupOutlined /><span>{t('lifecycleGuideLayerManagementTitle')}</span></Title>
                <Description>{t('lifecycleGuideLayerManagementDescription')}</Description>
                <LayerArtwork />
            </Section>
            <Section data-testid="lifecycleGuideSection">
                <Title><TimelineFlagIcon aria-hidden="true" /><span>{t('lifecycleGuideDifferenceMarkerTitle')}</span></Title>
                <Description>{t('lifecycleGuideDifferenceMarkerDescription')}</Description>
                <DifferenceMarkerArtwork />
            </Section>
            <Section data-testid="lifecycleGuideSection">
                <Title><FlagOutlined /><span>{t('lifecycleGuideMarkerManagementTitle')}</span></Title>
                <Description>{t('lifecycleGuideMarkerManagementDescription')}</Description>
                <MarkerManagementArtwork />
            </Section>
        </Body>
    </Panel>, document.body);
};
