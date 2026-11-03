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
import React from 'react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { parseAgentReportReference } from '@insight/lib/AgentReport';
import { requestHostCloseReport } from '../connection';
import { ReportPreview } from './ReportPreview';

const Page = styled.main`
    height: 100%; width: 100%; min-width: 0; display: flex;
    color: ${(props): string => props.theme.textColorPrimary};
    background: ${(props): string => props.theme.bgColor};
`;

export const ReportPage = (): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const params = new URLSearchParams(window.location.search);
    const report = parseAgentReportReference({
        artifactId: params.get('reportId'), version: Number(params.get('reportVersion')), title: t('reportTitle'),
    });
    return <Page>
        {report
            ? <ReportPreview report={{ ...report, kind: 'visualization', description: '', createdAt: '' }}
                onClose={requestHostCloseReport} />
            : <div role="alert">{t('reportLoadFailed')}</div>}
    </Page>;
};
