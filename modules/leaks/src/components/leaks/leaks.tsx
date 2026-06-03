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

    return <LeaksPage>
        <ContentArea>
            <MemoryStack session={session} />
        </ContentArea>
        <BottomTab session={session} />
        {session.memSnapshotParseLoading
            ? <ParseLoadingMask>
                <ParseLoadingPanel>
                    <ParseLoadingHead>
                        <div>
                            <ParseLoadingTitle>
                                <ParseLoadingSpinner />
                                {t('parsing')}
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
