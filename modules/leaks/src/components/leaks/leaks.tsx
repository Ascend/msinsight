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
import React from 'react';
import { observer } from 'mobx-react';
import { Session } from '../../entity/session';
import MemoryStack from '../MemoryStack';
import { BottomTab } from './BottomTab';
import useWorkerMessage from '@/leaksWorker/useWorkerMessage';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import OpfsFallbackNotice from '../OpfsFallbackNotice';
import { Select } from '@insight/lib/components';
import { runInAction } from 'mobx';

const LeaksPage = styled.div`
    position: relative;
    display: flex;
    flex-direction: column;
    height: 100vh;
    padding: 16px;
`;

const ContentArea = styled.div`
    flex: 1;
    overflow: auto;
    background: var(--mi-bg-color);
    margin-bottom: 16px;
`;

const WindowToolbar = styled.div`
    position: relative;
    display: flex;
    flex: 0 0 auto;
    flex-direction: column;
    align-items: stretch;
    gap: 8px;
    min-height: 58px;
    margin-bottom: 12px;
    padding: 9px 12px 9px 15px;
    overflow: hidden;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 8px;
    background: ${(props): string => props.theme.bgColorCommon};
    box-shadow: inset 3px 0 0 ${(props): string => props.theme.primaryColor}, 0 2px 8px rgba(0, 0, 0, 0.06);
    color: ${(props): string => props.theme.textColorSecondary};
`;

const WindowToolbarRow = styled.div`
    display: flex;
    min-width: 0;
    align-items: center;
    justify-content: space-between;
    gap: 20px;

    @media (max-width: 760px) {
        align-items: stretch;
        flex-direction: column;
        gap: 8px;
    }
`;

const WindowIdentity = styled.div`
    display: flex;
    min-width: 0;
    align-items: center;
    gap: 10px;
`;

const WindowGlyph = styled.span`
    display: grid;
    flex: 0 0 auto;
    grid-template-columns: repeat(2, 8px);
    gap: 2px;
    width: 24px;
    height: 24px;
    padding: 3px;
    border: 1px solid ${(props): string => props.theme.primaryColor};
    border-radius: 6px;
    background: ${(props): string => props.theme.bgColorLight};

    &::before,
    &::after {
        content: '';
        border-radius: 2px;
        background: ${(props): string => props.theme.primaryColor};
    }

    &::after {
        opacity: 0.45;
    }
`;

const WindowHeading = styled.div`
    display: flex;
    min-width: 0;
    flex-direction: column;
    gap: 2px;
`;

const WindowTitle = styled.strong`
    overflow: hidden;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 13px;
    font-weight: 600;
    line-height: 16px;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const WindowPosition = styled.span`
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 11px;
    font-variant-numeric: tabular-nums;
    line-height: 14px;
`;

const WindowControls = styled.div`
    display: flex;
    min-width: 0;
    align-items: center;
    justify-content: flex-end;
    gap: 10px;

    @media (max-width: 760px) {
        justify-content: flex-start;
    }
`;

const WindowSelectFrame = styled.div`
    display: flex;
    min-width: 180px;
    padding: 2px;
    border-radius: 6px;
    background: ${(props): string => props.theme.bgColorLight};

    .ant-select {
        width: 100%;
    }

    .ant-select-selector {
        border-radius: 4px !important;
    }
`;

const OverallParseStatus = styled.div`
    display: flex;
    min-width: 0;
    align-items: center;
    gap: 8px;
    padding: 6px 9px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 8px;
    background: ${(props): string => props.theme.bgColorLight};
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 11px;
`;

const StandaloneOverallParse = styled.div`
    margin-bottom: 12px;
`;

const WindowProgressTrack = styled.span`
    flex: 1;
    min-width: 52px;
    height: 3px;
    overflow: hidden;
    border-radius: 999px;
    background: ${(props): string => props.theme.borderColor};
`;

const WindowProgressValue = styled.span<{ percent: number }>`
    display: block;
    width: ${(props): number => props.percent}%;
    height: 100%;
    border-radius: inherit;
    background: ${(props): string => props.theme.primaryColor};
    transition: width 0.2s ease;
`;

const WindowProgressPercent = styled.span`
    min-width: 28px;
    color: ${(props): string => props.theme.primaryColor};
    font-variant-numeric: tabular-nums;
    text-align: right;
`;

const ParseLoadingMask = styled.div`
    position: absolute;
    inset: 16px;
    z-index: 10;
    display: flex;
    align-items: center;
    justify-content: center;
    background: var(--mi-bg-color);
`;

const ParseLoadingPanel = styled.div`
    width: min(520px, calc(100% - 48px));
    padding: 24px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 12px;
    background: ${(props): string => props.theme.bgColorCommon};
    box-shadow: ${(props): string => props.theme.boxShadow};
    color: ${(props): string => props.theme.textColorPrimary};
`;

const ParseLoadingHead = styled.div`
    margin-bottom: 12px;
`;

const ParseLoadingTitle = styled.div`
    display: inline-flex;
    align-items: center;
    gap: 8px;
    font-size: 14px;
    font-weight: 500;
`;

const ParseLoadingSpinner = styled.span`
    width: 14px;
    height: 14px;
    border: 2px solid ${(props): string => props.theme.borderColor};
    border-top-color: ${(props): string => props.theme.primaryColor};
    border-radius: 50%;
    animation: parse-loading-spin 0.8s linear infinite;

    @keyframes parse-loading-spin {
        to {
            transform: rotate(360deg);
        }
    }
`;

const ParseLoadingFile = styled.div`
    margin-top: 4px;
    color: ${(props): string => props.theme.textColorSecondary};
    line-height: 1.5;
    word-break: break-all;
`;

const ParseLoadingProgress = styled.div`
    flex: 1;
    width: 100%;
    height: 4px;
    overflow: hidden;
    border-radius: 999px;
    background: ${(props): string => props.theme.borderColor};
`;

const ParseLoadingProgressRow = styled.div`
    display: flex;
    align-items: center;
    gap: 10px;
    width: 100%;
`;

const ParseLoadingProgressInner = styled.div<{ percent: number }>`
    width: ${(props): number => props.percent}%;
    height: 100%;
    border-radius: inherit;
    background: ${(props): string => props.theme.primaryColor};
    transition: width 0.2s ease;
`;

const ParseLoadingPercent = styled.div`
    min-width: 36px;
    color: ${(props): string => props.theme.primaryColor};
    font-variant-numeric: tabular-nums;
    line-height: 1;
    text-align: right;
`;

const getFileName = (filePath: string): string => {
    const normalizedPath = filePath.replace(/\\/g, '/');
    return normalizedPath.split('/').pop() ?? filePath;
};

const index = observer((props: { session: Session }) => {
    const { session } = props;
    const { t } = useTranslation('leaks');

    useWorkerMessage();

    const activeDeviceId = session.deviceId || Object.keys(session.snapshotSlices)[0] || '';
    const deviceSlices = session.snapshotSlices[activeDeviceId];
    const windowOptions = deviceSlices?.slices.map(slice => ({
        value: slice.index,
        label: `${t('snapshotWindow')} ${slice.index + 1}${slice.ready ? '' : ` (${t('pending')})`}`,
        disabled: !slice.ready,
    })) ?? [];
    const selectedWindowPosition = session.selectedSliceIndex >= 0
        ? session.selectedSliceIndex + 1
        : '—';
    const hasReadyWindow = Object.values(session.snapshotSlices).some(
        device => (device?.readySlices?.length ?? 0) > 0,
    );
    const parseOverlayVisible = session.memSnapshotParseLoading && !hasReadyWindow;
    const showWindowToolbar = session.module === 'memsnapshot' && windowOptions.length > 0;
    const overallParseProgress = session.memSnapshotParseLoading
        ? <OverallParseStatus>
            <ParseLoadingSpinner />
            <span>{t('overallParsing')}</span>
            <WindowProgressTrack>
                <WindowProgressValue percent={session.memSnapshotParseProgress} />
            </WindowProgressTrack>
            <WindowProgressPercent>{`${session.memSnapshotParseProgress}%`}</WindowProgressPercent>
        </OverallParseStatus>
        : <></>;

    return <LeaksPage>
        {showWindowToolbar
            ? <WindowToolbar>
                <WindowToolbarRow>
                    <WindowIdentity>
                        <WindowGlyph aria-hidden="true" />
                        <WindowHeading>
                            <WindowTitle>{t('snapshotWindow')}</WindowTitle>
                            <WindowPosition>{`${selectedWindowPosition} / ${windowOptions.length}`}</WindowPosition>
                        </WindowHeading>
                    </WindowIdentity>
                    <WindowControls>
                        <WindowSelectFrame>
                            <Select
                                id="select-snapshot-window"
                                aria-label={t('snapshotWindow')}
                                value={session.selectedSliceIndex}
                                options={windowOptions}
                                width="100%"
                                onChange={(value): void => {
                                    const sliceIndex = Number(value);
                                    if (!deviceSlices?.slices[sliceIndex]?.ready) {
                                        return;
                                    }
                                    runInAction(() => {
                                        session.selectedSliceIndex = sliceIndex;
                                    });
                                }}
                            />
                        </WindowSelectFrame>
                    </WindowControls>
                </WindowToolbarRow>
                {session.memSnapshotParseLoading ? overallParseProgress : <></>}
            </WindowToolbar>
            : session.memSnapshotParseLoading && hasReadyWindow
                ? <StandaloneOverallParse>{overallParseProgress}</StandaloneOverallParse>
                : <></>}
        <ContentArea>
            <MemoryStack session={session} />
        </ContentArea>
        <BottomTab session={session} />
        <OpfsFallbackNotice />
        {parseOverlayVisible
            ? <ParseLoadingMask>
                <ParseLoadingPanel>
                    <ParseLoadingHead>
                        <div>
                            <ParseLoadingTitle>
                                <ParseLoadingSpinner />
                                {t('overallParsing')}
                            </ParseLoadingTitle>
                            <ParseLoadingFile>{getFileName(session.memSnapshotParseFileId)}</ParseLoadingFile>
                        </div>
                    </ParseLoadingHead>
                    <ParseLoadingProgressRow>
                        <ParseLoadingProgress>
                            <ParseLoadingProgressInner percent={session.memSnapshotParseProgress} />
                        </ParseLoadingProgress>
                        <ParseLoadingPercent>{`${session.memSnapshotParseProgress}%`}</ParseLoadingPercent>
                    </ParseLoadingProgressRow>
                </ParseLoadingPanel>
            </ParseLoadingMask>
            : <></>}
    </LeaksPage>;
});

export default index;
